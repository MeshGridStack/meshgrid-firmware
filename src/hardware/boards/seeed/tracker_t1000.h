/**
 * Seeed Card Tracker T1000-E board configuration
 *
 * nRF52840 + LR1110
 */

#ifndef BOARD_SEEED_TRACKER_CONFIG_H
#define BOARD_SEEED_TRACKER_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_SEEED_TRACKER

static const struct board_config seeed_tracker_config = {
    .name = "T1000-E",
    .vendor = "Seeed",

    .radio = RADIO_LR1110,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,  // LR1110 has built-in GPS

    .radio_pins = {
        .mosi = 45,
        .miso = 46,
        .sck = 3,
        .cs = 44,
        .reset = 42,
        .busy = 43,
        .dio0 = -1,
        .dio1 = 40,
        .rxen = -1,
        .txen = -1,
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
    },

    .early_init = NULL,
    .late_init = NULL,
};

#define CURRENT_BOARD_CONFIG seeed_tracker_config

#endif
#endif
