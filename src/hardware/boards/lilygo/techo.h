/**
 * LilyGo T-Echo board configuration
 *
 * nRF52840 + SX1262 + E-Ink display
 */

#ifndef BOARD_LILYGO_TECHO_CONFIG_H
#define BOARD_LILYGO_TECHO_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_LILYGO_TECHO

static const struct board_config lilygo_techo_config = {
    .name = "T-Echo",
    .vendor = "LilyGo",

    .radio = RADIO_SX1262,
    .display = DISPLAY_EINK_GDEY0154D67,
    .gps = GPS_L76K,

    .radio_pins = {
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

    .display_pins = {
        .sda = -1,  // E-Ink uses SPI
        .scl = -1,
        .reset = 2,
        .addr = 0,
        .width = 200,
        .height = 200,
    },

    .gps_pins = {
        .rx = 9,
        .tx = 10,
        .pps = -1,
        .enable = 3,
        .baud = 9600,
    },

    .power_pins = {
        .vext = -1,
        .led = 14,
        .vbat_adc = 4,
        .button = 32,
        .vext_active_low = false,
    },

    .lora_defaults = {
        .frequency = 868.0,
        .bandwidth = 125.0,
        .spreading_factor = 9,
        .coding_rate = 7,
        .tx_power = 22,
        .preamble_len = 8,
        .use_crc = true,
        .sync_word = 0x12,          // RADIOLIB_SX126X_SYNC_WORD_PRIVATE for MeshCore compat
    },

    .early_init = NULL,
    .late_init = NULL,
};

#define CURRENT_BOARD_CONFIG lilygo_techo_config

#endif
#endif
