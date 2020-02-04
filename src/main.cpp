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
#define I_BUTTON                0
/*
#define I_BOLT_LOCKED           0   // Microswitch input to detect when deadbolt is fully locked
#define I_BOLT_UNLOCKED         0   // Microswitch input to detect when deadbolt is fully unlocked
#define I_KEYPAD_READ           19  // Signal to keypad sniffed
#define I_DEADBOLT_READ         18  // Signal to deadbolt sniffer
#define O_BLOCK_KEYPAD_READ     0   // Output to block communication from deadbolt to keypad
#define O_BLOCK_BUZZER          0   // Output to block the buzzer
*/

// IoT Configuration
#define DEVICE_NAME             "trinket-esp32-1"

#define MQTT_SERVER             "ahu6v4hx3ap4w-ats.iot.us-east-1.amazonaws.com"
#define MQTT_PORT               8883
#define MQTT_TOPIC              "test"

// Generic Configuration
#define EVENT_WAIT_TIME_MS      10000   // Time in ms since the last event before entering sleep
#define SLEEP_TIME_S            10

/*
Main Program Flow:
    - hardware init
        - configure RMT
        - attach interrupts
    Configuration Mode:
    - open network presenting a configuration page
    - block from continuing
    - flash keypad LEDs occasionally to indicate that the device is wasting power in config mode
    Normal Mode:
    - start connecting to wifi
        - if device is wifi connection fails, enter configuration mode
    - if cause of wakeup was a pinpad interrupt
        - if configuration is requested (by key combination)
            - enter configuration mode
        - observe pinpad and deadbolt comms and write to MQTT
    - if cause of wakeup was a lock/unlock
        - write to MQTT
    - if cause of wakeup was a timer
        - process commands from MQTT server
    - enter deep sleep mode
*/

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

//static void on_powerbolt_read(uint8_t port, powerbolt_read_t received);

static volatile union {
    uint8_t flags;
    struct {
        uint8_t mqtt: 1;
        uint8_t rmt: 1;
        // Temporary
        uint8_t button: 1;
        //uint8_t locked: 1;
        //uint8_t unlocked: 1;
    };
} triggered_event_flags;

// Temporary
static void on_button_press() {
    Serial.println("Button press");
    triggered_event_flags.button = true;
}

void setup()
{
    // Hardware init
    pinMode(I_BUTTON, INPUT_PULLUP);
    /*pinMode(I_BOLT_LOCKED, INPUT_PULLDOWN);
    pinMode(I_BOLT_UNLOCKED, INPUT_PULLDOWN);
    pinMode(O_BLOCK_KEYPAD_READ, OUTPUT);
    trinket_powerbolt_setup(I_KEYPAD_READ, I_DEADBOLT_READ);*/
    attachInterrupt(I_BUTTON, on_button_press, FALLING);
    //

    Serial.begin(115200);

    // No wifi or MQTT configuration in this hard-coded version

    // Get wakeup reason (timer, pin)
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Wakeup from button");
    }

    // If device was woken up from physical interaction with the deadbolt
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        /*int wakeup_interrupt = esp_sleep_get_ext1_wakeup_status();

        // If the device was woken up from pinpad or keypad activity
        if (wakeup_interrupt == (1 << I_KEYPAD_READ | 1 << I_DEADBOLT_READ)) {
            Serial.println("Wakeup from keypad");
            // Wait for RMT events
        }

        // If the device was woken up from a lock/unlock
        if (wakeup_interrupt == (1 << I_BOLT_LOCKED & 1 << I_BOLT_UNLOCKED)) {
            // Send to MQTT
            Serial.println("Wakeup from lock/unlock");
        }*/
    }

    // If device was woken up from timer
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Check for command messages on MQTT server
        Serial.println("Wakeup from timer");
    }

    /*trinket_powerbolt_read(on_powerbolt_read);
    */
   Serial.println("Ready");
}

// Put received messages into a queue until Wifi and MQTT connections are established
/*
volatile static bool rmt_event_triggered = false;
static void on_powerbolt_read(uint8_t port, powerbolt_read_t received) {
    rmt_event_triggered = true;
    Serial.print(port == 0 ? "Powerbolt" : "Keypad");
    Serial.print(": ");
    if (received.valid)
        Serial.printf("%02x", received.data);
    else
        Serial.print("invalid");

    Serial.println();
}

// Prevent the keypad from receiving signals from the powerbolt
static void block_keypad_read() {
    digitalWrite(O_BLOCK_KEYPAD_READ, HIGH);
}

// Prevent the powerbolt from sounding the buzzer on inputs
static void block_powerbolt_buzzer() {
    digitalWrite(O_BLOCK_BUZZER, HIGH);
}*/

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

static void mqtt_received(char *topic, byte *payload, unsigned int length)
{
    triggered_event_flags.mqtt = true;

    Serial.print("MQTT [");
    Serial.print(topic);
    Serial.print("]: ");
    for (int i = 0; i < length; i++)
        Serial.print((char)payload[i]);
    Serial.println();
}

static void enter_deep_sleep() {
    /*const uint64_t bitmask = 1 << I_KEYPAD_READ | 1 << I_DEADBOLT_READ | 1 << I_BOLT_LOCKED | 1 << I_BOLT_UNLOCKED;
    esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);*/
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_S * 1000 * 1000);
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
    if (!mqtt_client.subscribe(MQTT_TOPIC)) {
        Serial.println("Failed to subscribe to topic");
        return enter_deep_sleep();
    }

    mqtt_client.setCallback(mqtt_received);

    // Temporary just to know something happened
    Serial.println("Saying hello");
    mqtt_client.publish(MQTT_TOPIC, "hello");

    // Wait for events until timeout
    Serial.println("Waiting for events");
    unsigned long last_event = millis();
    while (millis() - last_event < EVENT_WAIT_TIME_MS) {
        mqtt_client.loop();

        // Nothing happened, keep waiting
        if (!triggered_event_flags.flags) {
            delay(50);
            continue;
        }

        // Only process one event per loop
        last_event = millis();
        if (triggered_event_flags.mqtt) {
            triggered_event_flags.mqtt = false;
            //
        }
        else if (triggered_event_flags.rmt) {
            triggered_event_flags.rmt = false;
            // Send RMT events from the queue to the MQTT server
        }
        // Temporary
        else if (triggered_event_flags.button) {
            triggered_event_flags.button = false;
            mqtt_client.publish(MQTT_TOPIC, "button");
        }
    }

    // Go to sleep
    Serial.println("Sleepy time");
    enter_deep_sleep();
}