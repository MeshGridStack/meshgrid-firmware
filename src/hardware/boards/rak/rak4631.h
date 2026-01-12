/**
 * RAK WisBlock 4631 board configuration
 *
 * nRF52840 + SX1262
 */

#ifndef BOARD_RAK4631_CONFIG_H
#define BOARD_RAK4631_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_RAK4631

static const struct board_config rak4631_config = {
    .name = "RAK4631",
    .vendor = "RAKwireless",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,    // No built-in display
    .gps = GPS_NONE,

    .radio_pins = {
        .mosi = 45,     // P1.13
        .miso = 46,     // P1.14
        .sck = 43,      // P1.11
        .cs = 42,       // P1.10
        .reset = 38,    // P1.06
        .busy = 39,     // P1.07
        .dio0 = -1,
        .dio1 = 47,     // P1.15
        .rxen = 37,     // P1.05
        .txen = 36,     // P1.04
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
        .led = 35,      // Green LED
        .vbat_adc = 5,  // AIN3
        .button = -1,
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

#define CURRENT_BOARD_CONFIG rak4631_config

#endif // BOARD_RAK4631
#endif // BOARD_RAK4631_CONFIG_H
