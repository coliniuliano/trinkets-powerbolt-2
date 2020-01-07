#include "Arduino.h"
#include "esp32-hal.h"

// Private libraries
#include "powerbolt-protocol.h"

// Project-specific
#include "trinket-powerbolt.h"


// IO Configuration
#define I_CONFIG_BTN            0   // Button to force configuration mode on startup
#define I_BOLT_LOCKED           0   // Microswitch input to detect when deadbolt is fully locked
#define I_BOLT_UNLOCKED         0   // Microswitch input to detect when deadbolt is fully unlocked
#define I_KEYPAD_READ           19  // Signal to keypad sniffed
#define I_DEADBOLT_READ         18  // Signal to deadbolt sniffer
#define O_DEADBOLT_WRITE        0   // Output to deadbolt from this device
#define O_BLOCK_KEYPAD_READ     0   // Output to block communication from deadbolt to keypad

/*
Considerations:
    - sleep mode
    - esp-now vs MQTT based on configuration
        - start with one for simplicity
    - OTA firmware updates to simplify future development once installed
    - configuration override button
*/

/*
Main Program Flow:
    - if device is not configured
        - open network presenting a configuration page
    - hardware init
        - configure RMT
        - attach interrupts
        - determine current state
    - enter deep sleep mode and wait for input or timer
    - upon wake, check for remote commands
    - handle input and go back to sleep
*/

void setup()
{
    // Configure IO first
    //pinMode(IO_BLOCK_IN, OUTPUT);
    //pinMode(IO_BLOCK_OUT, OUTPUT);
    //
    // 

    trinket_powerbolt_setup(I_KEYPAD_READ, I_DEADBOLT_READ, O_DEADBOLT_WRITE);

    Serial.begin(115200);


    // Get wakeup reason (timer, pin)
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.print("Awake - ");
    Serial.print(reset_reason);
    if (reset_reason == ESP_RST_DEEPSLEEP)
        Serial.println(" (deepsleep)");
    else
        Serial.println();

    // Detect configuration override button
    //bool configModerequested = // button pressed

    // Retrieve device configuration
    // var config = getconfig();

    //if (!config || configModeRequested) {
        // Enter configuration mode
        //Serial.println("Configuration mode");
        //
    //}

    // If device was woken up from external input
    //

    // If device was woken up from timer
    //

    // Try to establish a connection
    //

    // Ready for main loop
    Serial.println("Ready for input");
}

void loop()
{
    int readByte = Serial.read();
    switch (readByte) {
        case '1':
        case '2':
            Serial.println("Sending key: 1/2");
            trinket_powerbolt_write(KEY_12);
            break;
        case '3':
        case '4':
            Serial.println("Sending key: 3/4");
            trinket_powerbolt_write(KEY_34);
            break;
        case '5':
        case '6':
            Serial.println("Sending key: 5/6");
            trinket_powerbolt_write(KEY_56);
            break;
        case '7':
        case '8':
            Serial.println("Sending key: 7/8");
            trinket_powerbolt_write(KEY_78);
            break;
        case '9':
        case '0':
            Serial.println("Sending key: 9/0");
            trinket_powerbolt_write(KEY_90);
            break;
        case 'u':
        case 'U':
            Serial.println("Sending key: unknown");
            trinket_powerbolt_write(KEY_UNKNOWN);
            break;
        case 'l':
        case 'L':
            Serial.println("Sending key: Lock");
            trinket_powerbolt_write(KEY_LOCK);
            break;
        case 'c':
        case 'C':
            Serial.println("Sending key: Clear");
            trinket_powerbolt_write(KEY_CLEAR);
            break;
    }
    delay(10);
}