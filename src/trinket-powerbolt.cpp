#include "trinket-powerbolt.h"

#include "esp32-hal.h"
#include "powerbolt-protocol.h"

// Private declarations
static void rmt_on_receive_from_powerbolt(uint32_t *data, size_t len);
static void rmt_on_receive_from_keypad(uint32_t *data, size_t len);

// Private variables
rmt_obj_t *rmt_writer = NULL; 
rmt_data_t rmt_send_buffer[20];
rmt_obj_t *rmt_reader_from_powerbolt = NULL;
rmt_obj_t *rmt_reader_from_keypad = NULL;
rmt_data_t rmt_read_from_powerbolt_buffer[20];
rmt_data_t rmt_read_from_keypad_buffer[20];
void (*read_callback)(uint8_t, powerbolt_read_t) = NULL;

void trinket_powerbolt_setup(int keypad_read_pin, int powerbolt_read_write_pin) {
    // Configure RMT writer to interface with Powerbolt, but detach the writer from the pin
    rmt_writer = rmtInit(powerbolt_read_write_pin, true, RMT_MEM_64);
    pinMatrixOutDetach(powerbolt_read_write_pin, 0, 0);

    // Configure RMT reader to interface with Powerbolt
    rmt_reader_from_powerbolt = rmtInit(keypad_read_pin, false, RMT_MEM_128);
    rmt_reader_from_keypad = rmtInit(powerbolt_read_write_pin, false, RMT_MEM_128);

    // Set RMT tick rates
    // Write is 10x slower than read because it needs to output very long start/stop pulses
    rmtSetTick(rmt_reader_from_powerbolt, 10000);
    rmtSetTick(rmt_reader_from_keypad, 10000);
    rmtSetTick(rmt_writer, 100000);

    // Start RMT reading on both ports
    rmtRead(rmt_reader_from_powerbolt, rmt_on_receive_from_powerbolt);
    rmtRead(rmt_reader_from_keypad, rmt_on_receive_from_keypad);
}

void trinket_powerbolt_write(POWERBOLT_KEY_CODES key_code) {
    powerbolt_write_buffer(rmt_send_buffer, key_code);

    // Temporarily assign the RMT channel as an output
    pinMatrixOutAttach(rmt_writer->pin, RMT_SIG_OUT0_IDX + rmt_writer->channel, 0, 0);
    rmtWrite(rmt_writer, rmt_send_buffer, 20);

    // Wait for write to complete... this could be improved
    delay(100);

    // Set the RMT channel back to read mode so the keypad can continue to work
    pinMatrixOutDetach(rmt_writer->pin, 0, 0);
    pinMode(rmt_writer->pin, INPUT);
}

void trinket_powerbolt_on_read(void (*callback)(uint8_t, powerbolt_read_t)) {
    read_callback = callback;
}

static void rmt_on_receive(uint8_t port, uint32_t *data, size_t len) {
    if (len != 9)
        return;

    powerbolt_read_t received_byte = powerbolt_parse_buffer(data);

    if (read_callback != NULL) {
        (*read_callback)(port, received_byte);
    }
}

static void rmt_on_receive_from_powerbolt(uint32_t *data, size_t len) {
    rmt_on_receive(0, data, len);
}

static void rmt_on_receive_from_keypad(uint32_t *data, size_t len) {
    rmt_on_receive(1, data, len);
}