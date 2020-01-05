#include "powerbolt-interface.h"

static void powerbolt_write_buffer_bit(rmt_data_t *rmt_buffer_bit, uint32_t high_duration, uint32_t low_duration) {
    rmt_buffer_bit->level0 = 1;
    rmt_buffer_bit->duration0 = high_duration;
    rmt_buffer_bit->level1 = 0;
    rmt_buffer_bit->duration1 = low_duration;
}

static void powerbolt_write_buffer_bit_high(rmt_data_t *rmt_buffer_bit) {
    powerbolt_write_buffer_bit(rmt_buffer_bit, 7, 3);
}

static void powerbolt_write_buffer_bit_low(rmt_data_t *rmt_buffer_bit) {
    powerbolt_write_buffer_bit(rmt_buffer_bit, 3, 7);
}

// Writes 20 bits to the RMT output buffer (repeated twice: 1 start, 8 data, 1 end)
void powerbolt_write_buffer(rmt_data_t rmt_buffer[], POWERBOLT_KEY_CODES key_code) {
    uint8_t bit_num = 0;

    // Write everything twice
    for (uint8_t write_cnt = 0; write_cnt < 2; write_cnt++) {
        // Start bit (30ms high, 1.4ms low)
        powerbolt_write_buffer_bit(&rmt_buffer[bit_num++], 300, 14);

        // 8 command bits (MSB to LSB)
        for (uint8_t i = 0; i < 8; i++) {
            if ((powerbolt_key_codes[key_code] & (0x80 >> i)) == 0) {
                powerbolt_write_buffer_bit_low(&rmt_buffer[bit_num++]);
            } else {
                powerbolt_write_buffer_bit_high(&rmt_buffer[bit_num++]);
            }
        }

        // End bit
        powerbolt_write_buffer_bit(&rmt_buffer[bit_num++], 3, 100);
    }
}