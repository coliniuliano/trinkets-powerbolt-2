#ifndef POWERBOLT_INTERFACE_H
#define POWERBOLT_INTERFACE_H

#include "Arduino.h"

// TODO: Use binary representation instead of wasteful array
enum POWERBOLT_KEY_CODES { KEY_LOCK, KEY_12, KEY_34, KEY_56, KEY_78, KEY_90, KEY_UNKNOWN };
const uint8_t key_data[][8] = {
    {0, 0, 0, 0, 1, 1, 1, 0},   // Lock
    {0, 0, 0, 0, 0, 0, 0, 1},   // 12
    {0, 0, 0, 0, 0, 0, 1, 0},   // 34
    {0, 0, 0, 0, 0, 0, 1, 1},   // 56
    {0, 0, 0, 0, 0, 1, 0, 0},   // 78
    {0, 0, 0, 0, 0, 1, 0, 1},   // 90
    {0, 0, 0, 0, 0, 1, 1, 1}    // Unknown (not on keypad)
};

enum POWERBOLT_LED_CODES { 
    GREEN_SHORT_1, GREEN_LONG_1, RED_SHORT_3, GREEN_LONG_2, YELLOW_LONG_1, 
    YELLOW_SHORT_3, YELLOW_SHORT_1, RED_SHORT_5, RED_SHORT_10 
};
const uint8_t led_data[][8] = {
    {1, 1, 0, 0, 0, 0, 0, 0},   // 1 green short
    //{1, 1, 0, 0, 0, 0, 0, 1}, // 1 green short
    {1, 1, 0, 0, 0, 0, 1, 0},   // 1 green long
    {1, 1, 0, 0, 0, 0, 1, 1},   // 3 red short
    //{1, 1, 0, 0, 0, 1, 0, 0}, // 1 green blip (bad)
    {1, 1, 0, 0, 0, 1, 0, 1},   // 2 green long
    //{1, 1, 0, 0, 0, 1, 1, 0}, // nothing
    //{1, 1, 0, 0, 0, 1, 1, 1}, // nothing
    {1, 1, 0, 0, 1, 0, 0, 0},   // yellow long
    //{1, 1, 0, 0, 1, 0, 0, 1}, // nothing
    {1, 1, 0, 0, 1, 0, 1, 0},   // 3 yellow short
    {1, 1, 0, 0, 1, 0, 1, 1},   // 1 yellow
    //{1, 1, 0, 0, 1, 1, 0, 0}, // nothing
    {1, 1, 0, 0, 1, 1, 0, 1},   // 5 red short
    {1, 1, 0, 0, 1, 1, 1, 0},   // 10 red short
    //{1, 1, 0, 0, 1, 1, 1, 1}, // nothing
};

void powerbolt_write_buffer(rmt_data_t rmt_buffer[], POWERBOLT_KEY_CODES key_code);

#endif