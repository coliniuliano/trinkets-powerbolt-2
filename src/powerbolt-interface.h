#ifndef POWERBOLT_INTERFACE_H
#define POWERBOLT_INTERFACE_H

#include "Arduino.h"

enum POWERBOLT_KEY_CODES { KEY_12, KEY_34, KEY_56, KEY_78, KEY_90, KEY_LOCK, 
    KEY_UNKNOWN, KEY_CLEAR 
};
const uint8_t powerbolt_key_codes[] = {
    0x01,   // 12
    0x02,   // 34
    0x03,   // 56
    0x04,   // 78
    0x05,   // 90

    0x07,   // Unknown (random testing - not on keypad)

    0x0E,   // Lock

    0xD2,   // Clear (no response)
    0xD4    // Clear (C3 C7 response)
};

enum POWERBOLT_RESP_CODES { 
    GREEN_SHORT_1, GREEN_LONG_1, RED_SHORT_3, GREEN_BLIP_1, GREEN_LONG_2, YELLOW_LONG_1, 
    YELLOW_SHORT_3, YELLOW_SHORT_1, RED_SHORT_5, RED_SHORT_10 
};
const uint8_t powerbolt_response_codes[] = {
    //0xC0  // 1 green short (unused)
    0xC1,   // 1 green short
    0xC2,   // 1 green long
    0xC3,   // 3 red short
    0xC4,   // 1 green blip, lights stay on after blip
    0xC5,   // 2 green long
    //0xC6, // nothing
    0xC7,   // lights off, clear/reset code
    0xC8,   // 1 yellow long
    //0xC9, // nothing
    0xCA,   // 3 yellow short
    0xCB,   // 1 yellow medium
    //0xCC  // nothing
    0xCD,   // 5 red short
    0xCE,   // 10 red short
    //0xCF  // nothing
};

void powerbolt_write_buffer(rmt_data_t rmt_buffer[], POWERBOLT_KEY_CODES key_code);

#endif