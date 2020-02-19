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
#define POWERBOLT_WRITE_WAIT_MS 750     // Time to wait between writes to the deadbolt
#define POWERBOLT_QUEUE_SIZE    100

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

typedef struct {
    uint8_t data :8;
    uint8_t port :1;
} trinket_powerbolt_queued_msg_t;

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
    unsigned long timestamp = millis();
    if (bolt_lock_debounce == 0 || timestamp - bolt_lock_debounce > 100) {
        Serial.println("Bolt locked");
        bolt_lock_debounce = timestamp;
        triggered_event_flags.locked = true;
    }
}

static unsigned long bolt_unlock_debounce = 0;
static void on_bolt_unlock() {
    unsigned long timestamp = millis();
    if (bolt_unlock_debounce == 0 || timestamp - bolt_unlock_debounce > 100) {
        Serial.println("Bolt unlocked");
        bolt_unlock_debounce = timestamp;
        triggered_event_flags.unlocked = true;
    }
}

void setup()
{
    // Hardware init (RMT reader first)
    pinMode(O_BLOCK_KEYPAD_RX, INPUT);
    trinket_powerbolt_setup(I_KEYPAD_READ, IO_DEADBOLT_RW);
    trinket_powerbolt_on_read(on_powerbolt_read);

    pinMode(I_BUTTON, INPUT_PULLUP);
    pinMode(I_BOLT_LOCKED, INPUT_PULLDOWN);
    pinMode(I_BOLT_UNLOCKED, INPUT_PULLDOWN);
    pinMode(O_BLOCK_BUZZER, INPUT);

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
        if (wakeup_interrupt & (1ULL << I_KEYPAD_READ | 1ULL << IO_DEADBOLT_RW))
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

static void allow_keypad_lights() {
    pinMode(O_BLOCK_KEYPAD_RX, INPUT);
}

static void block_keypad_lights() {
    pinMode(O_BLOCK_KEYPAD_RX, OUTPUT);
    digitalWrite(O_BLOCK_KEYPAD_RX, LOW);
}

static void allow_powerbolt_buzzer() {
    pinMode(O_BLOCK_BUZZER, INPUT);
}

static void block_powerbolt_buzzer() {
    pinMode(O_BLOCK_BUZZER, OUTPUT);
    digitalWrite(O_BLOCK_BUZZER, LOW);
}

// Ring buffer for storing powerbolt messages
static uint8_t powerbolt_queue_read_ptr = 0;
static uint8_t powerbolt_queue_write_ptr = 0;
trinket_powerbolt_queued_msg_t powerbolt_queue[POWERBOLT_QUEUE_SIZE];

static void on_powerbolt_read(uint8_t port, powerbolt_read_t received) {
    Serial.print(port == 0 ? "Powerbolt" : "Keypad");
    Serial.print(": ");
    if (!received.valid) {
        Serial.println("invalid");
        return;
    }

    Serial.printf("%02x", received.data);

    // write to write ptr then increment, logic for read is read != write then read[]
    // If the queue is not full, insert received messages
    uint8_t next_write_ptr = (powerbolt_queue_write_ptr + 1) % POWERBOLT_QUEUE_SIZE;
    if (next_write_ptr != powerbolt_queue_read_ptr) {
        Serial.println(" Q");
        powerbolt_queue[powerbolt_queue_write_ptr] = {
            .data = received.data,
            .port = port
        };
        powerbolt_queue_write_ptr = next_write_ptr;
        triggered_event_flags.rmt = true;
    } else {
        Serial.println(" XXX");
    }

    // Stop blocking the lights and buzzer when the deadbolt sends C7
    if (port == 0 && received.valid && received.data == 0xC7) {
        allow_powerbolt_buzzer();
        allow_keypad_lights();
    }
}

static unsigned long last_powerbolt_write = 0;
static void powerbolt_write(POWERBOLT_KEY_CODES key_code) {
    unsigned long timestamp = millis();
    unsigned long elapsed_since_last_write = timestamp - last_powerbolt_write;
    if (last_powerbolt_write > 0 && elapsed_since_last_write < POWERBOLT_WRITE_WAIT_MS)
        delay(POWERBOLT_WRITE_WAIT_MS - elapsed_since_last_write);

    block_keypad_lights();
    block_powerbolt_buzzer();
    trinket_powerbolt_write(key_code);
    last_powerbolt_write = millis();
}

static const POWERBOLT_KEY_CODES mqtt_key_map[] = {
    KEY_90, KEY_12, KEY_12, KEY_34, KEY_34, KEY_56, KEY_56, KEY_78, KEY_78, KEY_90
};
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
    byte mqtt_payload_buffer[20];
    memcpy(mqtt_payload_buffer, payload, length);

    // Expect all messages to start with DB for deadbolt (could be extended later)
    // Helps ensure that responses in the MQTT channel aren't treated as commands
    if (mqtt_payload_buffer[0] != 'D' || mqtt_payload_buffer[1] != 'B')
        return;

    // Handle actual payload, each character separately
    // Protocol:
    //      Deadbolt: 0 - 9, L = lock button, X = unpressable button
    //      General: ? = locked status
    for (int i = 2; i < length; i++) {
        const byte payload_byte = mqtt_payload_buffer[i];
        // 0 - 9, S, L (Deadbolt)
        if (payload_byte >= '0' && payload_byte <= '9') {
            uint8_t key_map_idx = payload_byte - '0';
            powerbolt_write(mqtt_key_map[key_map_idx]);
        }
        else if (payload_byte == 'L')
            powerbolt_write(KEY_LOCK);
        else if (payload_byte == 'X')
            powerbolt_write(KEY_HIDDEN);

        // Get status, don't continue processing
        else if (payload_byte == '?') {
            bool locked = digitalRead(I_BOLT_LOCKED);
            bool unlocked = digitalRead(I_BOLT_UNLOCKED);
            const char * mqtt_response = locked ? "> locked" : unlocked ? "> unlocked" : "> unknown";
            mqtt_client.publish(DEVICE_NAME, mqtt_response);
            return;
        }

        // If a character can not be processed, do not continue processing
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
    // Interrupts are on high, so make sure an input isn't already high
    uint64_t bitmask = 1ULL << I_KEYPAD_READ | 1ULL << IO_DEADBOLT_RW;
    if (!digitalRead(I_BOLT_LOCKED))
        bitmask |= 1 << I_BOLT_LOCKED;
    if (!digitalRead(I_BOLT_UNLOCKED))
        bitmask |= 1 << I_BOLT_UNLOCKED;

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
            // Send each RMT character from the queue to the MQTT server
            char rmt_string[20];
            while (powerbolt_queue_read_ptr != powerbolt_queue_write_ptr) {
                trinket_powerbolt_queued_msg_t current = powerbolt_queue[powerbolt_queue_read_ptr];
                sprintf(rmt_string, ">%c %02x", current.port == 0 ? 'D' : 'K', current.data);
                mqtt_client.publish(DEVICE_NAME, rmt_string);

                powerbolt_queue_read_ptr = (powerbolt_queue_read_ptr + 1) % POWERBOLT_QUEUE_SIZE;
            }

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