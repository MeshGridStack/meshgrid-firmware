/**
 * Seeed Wio Tracker L1 board configuration
 *
 * nRF52840 + LR1110 + GPS
 * Compact tracker with optional e-ink display
 */

#ifndef BOARD_SEEED_WIO_TRACKER_L1_CONFIG_H
#define BOARD_SEEED_WIO_TRACKER_L1_CONFIG_H

#include "hardware/board.h"

/* ============================================================================
 * Wio Tracker L1 (LCD variant)
 * ========================================================================== */
#ifdef BOARD_SEEED_WIO_TRACKER_L1

static const struct board_config seeed_wio_tracker_l1_config = {
    .name = "Wio Tracker L1",
    .vendor = "Seeed Studio",

    .radio = RADIO_LR1110,
    .display = DISPLAY_NONE, // LCD not yet supported
    .gps = GPS_NONE, // LR1110 has GNSS capabilities

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

#    define CURRENT_BOARD_CONFIG seeed_wio_tracker_l1_config

#endif // BOARD_SEEED_WIO_TRACKER_L1

/* ============================================================================
 * Wio Tracker L1 e-Ink variant
 * ========================================================================== */
#ifdef BOARD_SEEED_WIO_TRACKER_L1_EINK

static const struct board_config seeed_wio_tracker_l1_eink_config = {
    .name = "Wio Tracker L1 E-Ink",
    .vendor = "Seeed Studio",

    .radio = RADIO_LR1110,
    .display = DISPLAY_NONE, // E-ink not yet supported
    .gps = GPS_NONE,

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

#    define CURRENT_BOARD_CONFIG seeed_wio_tracker_l1_eink_config

#endif // BOARD_SEEED_WIO_TRACKER_L1_EINK

#endif // BOARD_SEEED_WIO_TRACKER_L1_CONFIG_H
