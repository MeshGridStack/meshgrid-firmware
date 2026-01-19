/**
 * B&Q Nano G1 board configuration
 *
 * ESP32 + SX1276
 */

#ifndef BOARD_NANO_G1_CONFIG_H
#define BOARD_NANO_G1_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_NANO_G1

static const struct board_config nano_g1_config = {
    .name = "Nano G1",
    .vendor = "B&Q Consulting",

    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,

    .radio_pins = {
        .mosi = 27,
        .miso = 19,
        .sck = 5,
        .cs = 18,
        .reset = 23,
        .busy = -1,
        .dio0 = 26,
        .dio1 = -1,
        .rxen = -1,
        .txen = -1,
    },

    .display_pins = {
        .sda = 21,
        .scl = 22,
        .reset = -1,
        .addr = 0x3C,
        .width = 128,
        .height = 64,
    },

    .gps_pins = {
        .rx = -1,
        .tx = -1,
        .pps = -1,
        .enable = -1,
        .baud = 0,
    },

    .power_pins = {
        .vext = -1,
        .led = 2,
        .vbat_adc = 35,
        .button = 0,
        .vext_active_low = false,
    },

    .lora_defaults = {
        .frequency = 868.0,
        .bandwidth = 125.0,
        .spreading_factor = 9,
        .coding_rate = 7,
        .tx_power = 20,
        .preamble_len = 8,
        .use_crc = true,
        .sync_word = 0x12,          // RADIOLIB_SX126X_SYNC_WORD_PRIVATE for MeshCore compat
    },

    .early_init = NULL,
    .late_init = NULL,
};

#define CURRENT_BOARD_CONFIG nano_g1_config

#endif
#endif
