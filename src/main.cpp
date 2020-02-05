#include "Arduino.h"
#include "esp32-hal.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Private libraries
#include "powerbolt-protocol.h"

// Project-specific
#include "trinket-powerbolt.h"

// Private configuration
#include "aws-iot-credentials.h"
#include "wifi-credentials.h"

// IO Configuration
#define I_BUTTON                0   // Button labeled "BOOT" on ESP32 dev board
#define I_BOLT_LOCKED           25  // Microswitch input to detect when deadbolt is fully locked
#define I_BOLT_UNLOCKED         26  // Microswitch input to detect when deadbolt is fully unlocked
#define IO_DEADBOLT_RW          32  // Signal to the deadbolt, either from keypad or from this device simulating a keypad
#define I_KEYPAD_READ           35  // Signal intended to reach the keypad, for responses to commands
#define O_BLOCK_KEYPAD_RX       33  // Output low to block keypad from receiving lights from deadbolt (otherwise high-Z)
#define O_BLOCK_BUZZER          27  // Output to block the buzzer

// IoT Configuration
#define DEVICE_NAME             "trinket-esp32-1"   // Used for AWS cert and as MQTT topic
#define MQTT_SERVER             "ahu6v4hx3ap4w-ats.iot.us-east-1.amazonaws.com"
#define MQTT_PORT               8883

// Generic Configuration
#define EVENT_WAIT_TIME_MS      10000   // Time in ms since the last event before entering sleep
#define SLEEP_TIME_S            30

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

static void on_powerbolt_read(uint8_t port, powerbolt_read_t received);

static volatile union {
    uint8_t flags;
    struct {
        uint8_t mqtt: 1;
        uint8_t rmt: 1;
        uint8_t locked: 1;
        uint8_t unlocked: 1;
    };
} triggered_event_flags;

static void on_button_press() {
    // There is no configuration mode so do nothing
    Serial.println("Button press");
}

static unsigned long bolt_lock_debounce = 0;
static void on_bolt_lock() {
    Serial.println("Bolt locked");
    unsigned long timestamp = millis();
    if (timestamp - bolt_lock_debounce > 100) {
        bolt_lock_debounce = timestamp;
        triggered_event_flags.locked = true;
    }
}

static unsigned long bolt_unlock_debounce = 0;
static void on_bolt_unlock() {
    Serial.println("Bolt unlocked");
    unsigned long timestamp = millis();
    if (timestamp - bolt_unlock_debounce > 100) {
        bolt_unlock_debounce = timestamp;
        triggered_event_flags.unlocked = true;
    }
}

void setup()
{
    // Hardware init
    pinMode(I_BUTTON, INPUT_PULLUP);
    pinMode(I_BOLT_LOCKED, INPUT_PULLDOWN);
    pinMode(I_BOLT_UNLOCKED, INPUT_PULLDOWN);
    pinMode(O_BLOCK_KEYPAD_RX, OUTPUT);

    trinket_powerbolt_setup(I_KEYPAD_READ, IO_DEADBOLT_RW);
    trinket_powerbolt_on_read(on_powerbolt_read);
    attachInterrupt(I_BUTTON, on_button_press, FALLING);
    attachInterrupt(I_BOLT_LOCKED, on_bolt_lock, RISING);
    attachInterrupt(I_BOLT_UNLOCKED, on_bolt_unlock, RISING);

    Serial.begin(115200);

    // Get wakeup reason (timer, pin)
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
        Serial.println("Wakeup from button");

    // If device was woken up from physical interaction with the deadbolt
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t wakeup_interrupt = esp_sleep_get_ext1_wakeup_status();

        // No action for keypad since the RMT read is not done
        if (wakeup_interrupt == (1ULL << I_KEYPAD_READ | 1ULL << IO_DEADBOLT_RW))
            Serial.println("Wakeup from keypad");

        // If the device was woken up from a lock/unlock
        else if (wakeup_interrupt == 1ULL << I_BOLT_LOCKED)
            on_bolt_lock();

        else if (wakeup_interrupt == 1ULL << I_BOLT_UNLOCKED)
            on_bolt_unlock();
    }

    // No action for timer, main loop will check for messages from MQTT server
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
        Serial.println("Wakeup from timer");
    
   Serial.println("Ready");
}

// Put received messages into a queue
static void on_powerbolt_read(uint8_t port, powerbolt_read_t received) {
    triggered_event_flags.rmt = true;
    // TODO: Queue the received msg

    Serial.print(port == 0 ? "Powerbolt" : "Keypad");
    Serial.print(": ");
    if (received.valid)
        Serial.printf("%02x", received.data);
    else
        Serial.print("invalid");

    Serial.println();
}

// Prevent the keypad from receiving signals from the powerbolt
// No need to exit this mode while the device is running
static void block_keypad_lights() {
    pinMode(O_BLOCK_KEYPAD_RX, OUTPUT);
    digitalWrite(O_BLOCK_KEYPAD_RX, LOW);
}

// Prevent the powerbolt from sounding the buzzer on inputs
static void block_powerbolt_buzzer() {
    digitalWrite(O_BLOCK_BUZZER, HIGH);
}

static unsigned long last_powerbolt_write = 0;
static void powerbolt_write(POWERBOLT_KEY_CODES key_code) {
    unsigned long timestamp = millis();
    unsigned long elapsed_since_last_write = timestamp - last_powerbolt_write;
    if (last_powerbolt_write > 0 && elapsed_since_last_write < 500)
        delay(500 - elapsed_since_last_write);

    block_keypad_lights();
    block_powerbolt_buzzer();
    trinket_powerbolt_write(key_code);
}

// MQTT messages handled immediately
static const POWERBOLT_KEY_CODES mqtt_key_map[] = {
    KEY_90, KEY_12, KEY_12, KEY_34, KEY_34, KEY_56, KEY_56, KEY_78, KEY_78, KEY_90
};
static byte mqtt_payload_buffer[20];
static void mqtt_received(char *topic, byte *payload, unsigned int length)
{
    triggered_event_flags.mqtt = true;

    Serial.print("MQTT (");
    Serial.print(length, DEC);
    Serial.print("): ");
    for (int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();

    if (length > 20) {
        Serial.println("MQTT message too long to process");
        return;
    }

    // MQTT send from this handler will ruin the payload buffer, so copy it locally
    memcpy(mqtt_payload_buffer, payload, length);

    // Handle actual payload, each character separately
    // Protocol:
    //      Deadbolt: 0 - 9, L = lock button, X = unpressable button
    //      General: ? = locked status
    for (int i = 0; i < length; i++) {
        // 0 - 9, S, L (Deadbolt)
        if (mqtt_payload_buffer[i] >= '0' && mqtt_payload_buffer[i] <= '9') {
            uint8_t key_map_idx = mqtt_payload_buffer[i] - '0';
            powerbolt_write(mqtt_key_map[key_map_idx]);
        }
        else if (mqtt_payload_buffer[i] == 'L')
            powerbolt_write(KEY_LOCK);
        else if (mqtt_payload_buffer[i] == 'X')
            powerbolt_write(KEY_HIDDEN);

        // Get status, don't continue processing
        else if (mqtt_payload_buffer[i] == '?') {
            bool locked = digitalRead(I_BOLT_LOCKED);
            bool unlocked = digitalRead(I_BOLT_UNLOCKED);
            const char * mqtt_response = locked ? "> locked" : unlocked ? "> unlocked" : "> unknown";
            mqtt_client.publish(DEVICE_NAME, mqtt_response);
            return;
        }

        // If a character can not be processed, do not continue processing
        // The device ends up reading its own responses, we don't want it parsing
        else
            return;
    }
}

static bool connect_to_wifi() {
    WiFi.setHostname(DEVICE_NAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint8_t wifi_timeout_count = 100;
    while (WiFi.status() != WL_CONNECTED && wifi_timeout_count--) {
        delay(100);
    }
    return wifi_timeout_count;
}

static bool connect_to_mqtt() {
    wifi_client.setCACert(AWS_CERT_CA);
    wifi_client.setCertificate(AWS_CERT_CRT);
    wifi_client.setPrivateKey(AWS_CERT_PRIVATE);

    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);

    // Make sure to use a non-clean session to receive persistent messages
    return mqtt_client.connect(DEVICE_NAME, NULL, NULL, NULL, false, false, NULL, false);
}

static void enter_deep_sleep() {
    const uint64_t bitmask = 1ULL << I_KEYPAD_READ | 1ULL << IO_DEADBOLT_RW | 1ULL << I_BOLT_LOCKED | 1ULL << I_BOLT_UNLOCKED;
    esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_S * 1000 * 1000);

    Serial.println("Sleepy time");
    esp_deep_sleep_start();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to wifi");
        if (!connect_to_wifi()) {
            Serial.println("Failed to connect to wifi");
            return enter_deep_sleep();
        }
    }

    if (mqtt_client.state() != MQTT_CONNECTED) {
        Serial.println("Connecting to MQTT");
        if (!connect_to_mqtt()) {
            Serial.print("Failed to connect to MQTT - ");
            Serial.println(mqtt_client.state());
            return enter_deep_sleep();
        }
    }

    Serial.println("Subscribing to MQTT topic");
    if (!mqtt_client.subscribe(DEVICE_NAME)) {
        Serial.println("Failed to subscribe to topic");
        return enter_deep_sleep();
    }

    mqtt_client.setCallback(mqtt_received);

    // Wait for events until timeout
    Serial.println("Waiting for events");
    unsigned long last_event = millis();
    while (millis() - last_event < EVENT_WAIT_TIME_MS) {
        // Restart the main loop if wifi or MQTT have dropped out 
        if (mqtt_client.state() != MQTT_CONNECTED || WiFi.status() != WL_CONNECTED)
            return;

        mqtt_client.loop();

        // Nothing happened, keep waiting
        if (!triggered_event_flags.flags) {
            delay(50);
            continue;
        }

        // Only process one event per loop
        last_event = millis();
        if (triggered_event_flags.mqtt) {
            // MQTT events are handled immediately during callback
            // This just makes sure the device gives up and goes to sleep after the delay
            triggered_event_flags.mqtt = false;
        }
        else if (triggered_event_flags.rmt) {
            // Send RMT events from the queue to the MQTT server
            triggered_event_flags.rmt = false;
        }
        else if (triggered_event_flags.locked) {
            mqtt_client.publish(DEVICE_NAME, "> Locked");
            triggered_event_flags.locked = false;
        }
        else if (triggered_event_flags.unlocked) {
            mqtt_client.publish(DEVICE_NAME, "> Unlocked");
            triggered_event_flags.unlocked = false;
        }
    }

    // No recent events, ok to turn off
    enter_deep_sleep();
}