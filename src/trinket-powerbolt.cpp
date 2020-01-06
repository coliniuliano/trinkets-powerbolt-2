#include "trinket-powerbolt.h"

#include "esp32-hal.h"
//#include "powerbolt-protocol.h"

// Private declarations
static void rmt_on_receive_from_powerbolt(uint32_t *data, size_t len);
static void rmt_on_receive_from_keypad(uint32_t *data, size_t len);

// Private variables
rmt_obj_t *rmt_sender = NULL;
rmt_data_t rmt_send_buffer[20];
rmt_obj_t *rmt_reader_from_powerbolt = NULL;
rmt_obj_t *rmt_reader_from_keypad = NULL;
rmt_data_t rmt_read_from_powerbolt_buffer[20];
rmt_data_t rmt_read_from_keypad_buffer[20];

/*static void rmt_on_receive_keypad(uint32_t *data, size_t len) {
    //
}

static void rmt_on_receive_powerbolt(uint32_t *data, size_t len) {
    //
}*/

void trinket_powerbolt_setup(int keypad_read_pin, int powerbolt_read_pin, int powerbolt_write_pin) {
    // Configure RMT reader to interface with Powerbolt
    rmt_reader_from_powerbolt = rmtInit(keypad_read_pin, false, RMT_MEM_128);
    rmt_reader_from_keypad = rmtInit(powerbolt_read_pin, false, RMT_MEM_128);
    rmt_sender = rmtInit(powerbolt_write_pin, true, RMT_MEM_64);

    // Set RMT tick rates
    // Write is 10x slower than read because it needs to output very long start/stop pulses
    rmtSetTick(rmt_reader_from_powerbolt, 10000);
    rmtSetTick(rmt_reader_from_keypad, 10000);
    rmtSetTick(rmt_sender, 100000);

    // Start RMT reading on both ports
    rmtRead(rmt_reader_from_powerbolt, rmt_on_receive_from_powerbolt);
    rmtRead(rmt_reader_from_keypad, rmt_on_receive_from_keypad);
}

void trinket_powerbolt_write(POWERBOLT_KEY_CODES key_code) {
    powerbolt_write_buffer(rmt_send_buffer, key_code);
    rmtWrite(rmt_sender, rmt_send_buffer, 20);
}

static void rmt_on_receive(const char * port, uint32_t *data, size_t len) {
    if (len != 9)
        return;

    powerbolt_read_t received_byte = powerbolt_parse_buffer(data);

    Serial.print(port);
    Serial.print(": ");
    if (received_byte.valid)
        Serial.printf("%02x", received_byte.data);
    else
        Serial.print("invalid");

    Serial.println();
}

static void rmt_on_receive_from_powerbolt(uint32_t *data, size_t len) {
    rmt_on_receive("Powerbolt", data, len);
}

static void rmt_on_receive_from_keypad(uint32_t *data, size_t len) {
    rmt_on_receive("Keypad", data, len);
}