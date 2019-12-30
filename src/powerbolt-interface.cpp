#include "powerbolt-interface.h"

// Writes 20 bits to the RMT output buffer (repeated twice: 1 start, 8 data, 1 end)
// TODO: Consider using binary for key_code instead of array
void powerbolt_write_buffer(rmt_data_t rmt_buffer[], POWERBOLT_KEY_CODES key_code) {
    uint8_t bit_num = 0;

    // Write everything twice
    for (uint8_t write_cnt = 0; write_cnt < 2; write_cnt++) {
        // Start bit (30ms high, 1.5ms low)
        rmt_buffer[bit_num].level0 = 1;
        rmt_buffer[bit_num].duration0 = 300;
        rmt_buffer[bit_num].level1 = 0;
        rmt_buffer[bit_num].duration1 = 14;
        bit_num++;

        // 8 command bits
        for (uint8_t i = 1; i <= 8; i++) {
                rmt_buffer[bit_num].level0 = 1;
                rmt_buffer[bit_num].level1 = 0;
            if (key_data[key_code][i-1] == 0) {
                rmt_buffer[bit_num].duration0 = 3;
                rmt_buffer[bit_num].duration1 = 7;
            } else {
                rmt_buffer[bit_num].duration0 = 7;
                rmt_buffer[bit_num].duration1 = 3;
            }
            
            bit_num++;
        }

        // End bit
        rmt_buffer[bit_num].level0 = 1;
        rmt_buffer[bit_num].duration0 = 3;
        rmt_buffer[bit_num].level1 = 0;
        rmt_buffer[bit_num].duration1 = 100;
        bit_num++;
    }
}