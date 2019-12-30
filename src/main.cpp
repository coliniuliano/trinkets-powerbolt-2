#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

#include "powerbolt-interface.h"

rmt_obj_t* rmt_send = NULL;
rmt_data_t buffer_out[20];

// IO Configuration
#define IO_LOCKED_IN    0  // Microswitch input to detect when deadbolt is fully locked
#define IO_UNLOCKED_IN  0  // Microswitch input to detect when deadbolt is fully unlocked
#define IO_KEYPAD_IN    0  // Signal from deadbolt to keypad, marked IN on keypad
#define IO_KEYPAD_OUT   0  // Signal from keypad to deadbolt, marked OUT on keypad
#define IO_BLOCK_IN	    0  // Output to block communication from deadbolt to keypad
#define IO_BLOCK_OUT    0  // Output to block communication from keypad to deadbolt

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

// Global Variables
rmt_obj_t* rmt_sender = NULL;
rmt_data_t rmt_send_buffer[20];

void setup() 
{
    // Configure IO first
    //pinMode(IO_BLOCK_IN, OUTPUT);
    //pinMode(IO_BLOCK_OUT, OUTPUT);
    //
    // etc

    Serial.begin(115200);

    // Get wakeup reason (timer, pin)
    // var reason = getReason();
    Serial.print("Awake - ");
    Serial.println("{reason TODO}");

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

    // Configure RMT sender to talk to Powerbolt
    if ((rmt_sender = rmtInit(18, true, RMT_MEM_64)) == NULL)
        Serial.println("init sender failed\n");

    // Configure RMT tick  for sender (100k)
    float realTick = rmtSetTick(rmt_sender, 100000);
    Serial.printf("RMT tick set to: %fns\n", realTick);

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
            powerbolt_write_buffer(rmt_send_buffer, KEY_12);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case '3':
        case '4':
            Serial.println("Sending key: 3/4");
            powerbolt_write_buffer(rmt_send_buffer, KEY_34);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case '5':
        case '6':
            Serial.println("Sending key: 5/6");
            powerbolt_write_buffer(rmt_send_buffer, KEY_56);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case '7':
        case '8':
            Serial.println("Sending key: 7/8");
            powerbolt_write_buffer(rmt_send_buffer, KEY_78);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case '9':
        case '0':
            Serial.println("Sending key: 9/0");
            powerbolt_write_buffer(rmt_send_buffer, KEY_90);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case 'u':
        case 'U':
            Serial.println("Sending key: unknown");
            powerbolt_write_buffer(rmt_send_buffer, KEY_UNKNOWN);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
        case 'l':
        case 'L':
            Serial.println("Sending key: Lock");
            powerbolt_write_buffer(rmt_send_buffer, KEY_LOCK);
            rmtWrite(rmt_sender, rmt_send_buffer, 20);
            break;
    }
    delay(10);
}