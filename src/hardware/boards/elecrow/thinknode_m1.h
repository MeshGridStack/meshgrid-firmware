/**
 * Elecrow ThinkNode M1 board configuration
 *
 * nRF52840 + SX1262
 */

#ifndef BOARD_THINKNODE_M1_CONFIG_H
#define BOARD_THINKNODE_M1_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_THINKNODE_M1

static const struct board_config thinknode_m1_config = {
    .name = "ThinkNode M1",
    .vendor = "Elecrow",

    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 22,
            .miso = 23,
            .sck = 19,
            .cs = 24,
            .reset = 25,
            .busy = 17,
            .dio0 = -1,
            .dio1 = 20,
            .rxen = -1,
            .txen = -1,
        },

    .display_pins =
        {
            .sda = 26,
            .scl = 27,
            .reset = -1,
            .addr = 0x3C,
            .width = 128,
            .height = 64,
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
            .led = 14,
            .vbat_adc = 4,
            .button = 32,
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
            .sync_word = 0x12, // RADIOLIB_SX126X_SYNC_WORD_PRIVATE for MeshCore compat
        },

    .early_init = NULL,
    .late_init = NULL,
};

#    define CURRENT_BOARD_CONFIG thinknode_m1_config

#endif
#endif
