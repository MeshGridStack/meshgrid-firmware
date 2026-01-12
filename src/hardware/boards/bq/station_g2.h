/**
 * B&Q Station G2 board configuration
 *
 * ESP32-S3 + SX1262 + Ethernet
 */

#ifndef BOARD_STATION_G2_CONFIG_H
#define BOARD_STATION_G2_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_STATION_G2

static const struct board_config station_g2_config = {
    .name = "Station G2",
    .vendor = "B&Q Consulting",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
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
        .rxen = 48,
        .txen = 47,
    },

    .display_pins = {
        .sda = -1,
        .scl = -1,
        .reset = -1,
        .addr = 0,
        .width = 0,
        .height = 0,
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
        .led = 37,
        .vbat_adc = -1,
        .button = 0,
        .vext_active_low = false,
    },

    .lora_defaults = {
        .frequency = 868.0,
        .bandwidth = 125.0,
        .spreading_factor = 9,
        .coding_rate = 7,
        .tx_power = 30,  // High power station
        .preamble_len = 8,
        .use_crc = true,
    },

    .early_init = NULL,
    .late_init = NULL,
};

#define CURRENT_BOARD_CONFIG station_g2_config

#endif
#endif
