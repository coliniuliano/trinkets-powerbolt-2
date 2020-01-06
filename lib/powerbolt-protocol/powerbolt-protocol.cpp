#include "powerbolt-protocol.h"

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

// Reads 9 bits from an RMT input buffer into a char and checks validity
powerbolt_read_t powerbolt_parse_buffer(uint32_t *data) {
    powerbolt_read_t result;
    result.valid = false;
    result.data = 0;

    // Look for 8 bits of logical 0 or 1
    for (uint8_t i = 0; i < 8; i++) {
        rmt_data_t *rmt_data = (rmt_data_t *) &data[i];

        if (rmt_data->level0 != 1 || rmt_data->level1 != 0)
            return result;

        // "0" bit
        if (rmt_data->duration0 >= 27 && rmt_data->duration0 <= 33 && rmt_data->duration1 >= 67 && rmt_data->duration1 <= 73)
            ;
        // "1" bit
        else if (rmt_data->duration0 >= 67 && rmt_data->duration0 <= 73 && rmt_data->duration1 >= 27 && rmt_data->duration1 <= 33)
            result.data |= 0x80 >> i;
        else
            return result;
    }

    // Stop bit has a duration1 that is too long for the pulse timer to receive
    rmt_data_t *rmt_data = (rmt_data_t *) &data[8];
    if (rmt_data->level0 == 1 && rmt_data->level1 == 0 && rmt_data->duration0 >= 27 && rmt_data->duration0 <= 33 && rmt_data->duration1 == 0)
        result.valid = true;

    return result;
}