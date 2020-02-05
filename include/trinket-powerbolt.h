#ifndef TRINKET_POWERBOLT_H
#define TRINKET_POWERBOLT_H

#include "powerbolt-protocol.h"

// Stolen from esp32-hal-rmt.c
// Required for finding pin and channel data from the RMT object
struct rmt_obj_s
{
    bool allocated;
    void *events;
    int pin;
    int channel;
};

extern "C" {
    void trinket_powerbolt_setup(int keypad_read_pin, int powerbolt_read_write_pin);
    void trinket_powerbolt_write(POWERBOLT_KEY_CODES key_code);
    void trinket_powerbolt_on_read(void (*callback)(uint8_t, powerbolt_read_t));
}

#endif