/**
 * LilyGo T-LoRa T3S3 board configuration
 *
 * ESP32-S3 + SX1262 + SSD1306 128x64 OLED
 */

#ifndef BOARD_LILYGO_T3S3_CONFIG_H
#define BOARD_LILYGO_T3S3_CONFIG_H

#include "lib/board.h"

#ifdef BOARD_LILYGO_T3S3

static const struct board_config lilygo_t3s3_config = {
    .name = "T3S3",
    .vendor = "LilyGo",

    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,

    .radio_pins = {
        .mosi = 11,
        .miso = 13,
        .sck = 12,
        .cs = 10,
        .reset = 5,
        .busy = 4,
        .dio0 = -1,
        .dio1 = 1,
        .rxen = -1,
        .txen = -1,
    },

    .display_pins = {
        .sda = 18,
        .scl = 17,
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
        .led = -1,
        .vbat_adc = -1,
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

#define CURRENT_BOARD_CONFIG lilygo_t3s3_config

#endif // BOARD_LILYGO_T3S3
#endif // BOARD_LILYGO_T3S3_CONFIG_H
