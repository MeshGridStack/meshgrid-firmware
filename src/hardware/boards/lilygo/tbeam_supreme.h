/**
 * LilyGo T-Beam Supreme board configuration
 *
 * ESP32-S3 + SX1262 + GPS
 */

#ifndef BOARD_LILYGO_TBEAM_SUPREME_CONFIG_H
#define BOARD_LILYGO_TBEAM_SUPREME_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_LILYGO_TBEAM_SUPREME

static const struct board_config lilygo_tbeam_supreme_config = {
    .name = "T-Beam Supreme",
    .vendor = "LilyGo",

    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_UBLOX,

    .radio_pins = {
        .mosi = 35,
        .miso = 37,
        .sck = 36,
        .cs = 39,
        .reset = 38,
        .busy = 33,
        .dio0 = -1,
        .dio1 = 34,
        .rxen = 21,
        .txen = 10,
    },

    .display_pins = {
        .sda = 17,
        .scl = 18,
        .reset = -1,
        .addr = 0x3C,
        .width = 128,
        .height = 64,
    },

    .gps_pins = {
        .rx = 9,
        .tx = 8,
        .pps = 6,
        .enable = 7,
        .baud = 38400,
    },

    .power_pins = {
        .vext = -1,
        .led = 4,
        .vbat_adc = 1,
        .button = 0,
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
    },

    .early_init = NULL,
    .late_init = NULL,
};

#define CURRENT_BOARD_CONFIG lilygo_tbeam_supreme_config

#endif
#endif
