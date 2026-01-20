/**
 * Heltec HT62 board configuration
 *
 * ESP32-C3 + SX1262
 * Compact mesh node
 */

#ifndef BOARD_HELTEC_HT62_CONFIG_H
#define BOARD_HELTEC_HT62_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_HELTEC_HT62

static const struct board_config heltec_ht62_config = {
    .name = "Heltec HT62",
    .vendor = "Heltec",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 6,
            .miso = 5,
            .sck = 4,
            .cs = 7,
            .reset = 10,
            .busy = 9,
            .dio0 = -1,
            .dio1 = 3,
            .rxen = -1,
            .txen = -1,
        },

    .display_pins =
        {
            .sda = -1,
            .scl = -1,
            .reset = -1,
            .addr = 0,
            .width = 0,
            .height = 0,
        },

    .gps_pins =
        {
            .rx = -1,
            .tx = -1,
            .pps = -1,
            .enable = -1,
            .baud = 0,
        },

    .power_pins =
        {
            .vext = -1,
            .led = 8,
            .vbat_adc = 1, // ADC1
            .button = 0,
            .vext_active_low = false,
        },

    .lora_defaults =
        {
            .frequency = 868.0,
            .bandwidth = 125.0,
            .spreading_factor = 9,
            .coding_rate = 7,
            .tx_power = 22,
            .preamble_len = 8,
            .use_crc = true,
            .sync_word = 0x12,
        },

    .early_init = NULL,
    .late_init = NULL,
};

#    define CURRENT_BOARD_CONFIG heltec_ht62_config

#endif // BOARD_HELTEC_HT62
#endif // BOARD_HELTEC_HT62_CONFIG_H
