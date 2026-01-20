/**
 * Seeed Wio-WM1110 board configuration
 *
 * nRF52840 + SX1262 + GNSS
 * Dev kit for Seeed WM1110 module
 */

#ifndef BOARD_SEEED_WIO_WM1110_CONFIG_H
#define BOARD_SEEED_WIO_WM1110_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_SEEED_WIO_WM1110

static const struct board_config seeed_wio_wm1110_config = {
    .name = "Wio-WM1110",
    .vendor = "Seeed Studio",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE, // Has GNSS but needs separate driver

    .radio_pins =
        {
            .mosi = 10,
            .miso = 9,
            .sck = 8,
            .cs = 7,
            .reset = 6,
            .busy = 5,
            .dio0 = -1,
            .dio1 = 4,
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
            .led = 26,
            .vbat_adc = 32,
            .button = 3,
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

#    define CURRENT_BOARD_CONFIG seeed_wio_wm1110_config

#endif // BOARD_SEEED_WIO_WM1110
#endif // BOARD_SEEED_WIO_WM1110_CONFIG_H
