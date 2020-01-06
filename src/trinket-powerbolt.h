#ifndef TRINKET_POWERBOLT_H
#define TRINKET_POWERBOLT_H

#include "powerbolt-protocol.h"

extern "C" {
    void trinket_powerbolt_setup(int keypad_read_pin, int powerbolt_read_pin, int powerbolt_write_pin);
    void trinket_powerbolt_write(POWERBOLT_KEY_CODES key_code);
}

#endif