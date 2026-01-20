/**
 * Muzi product family configurations
 *
 * nRF52840 + SX1262 based mesh nodes
 */

#ifndef BOARD_MUZI_VARIANTS_H
#define BOARD_MUZI_VARIANTS_H

#include "hardware/board.h"

/* ============================================================================
 * Muzi Base
 * ========================================================================== */
#ifdef BOARD_MUZI_BASE

static const struct board_config muzi_base_config = {
    .name = "Muzi Base",
    .vendor = "Muzi",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 45,
            .miso = 46,
            .sck = 43,
            .cs = 42,
            .reset = 38,
            .busy = 39,
            .dio0 = -1,
            .dio1 = 47,
            .rxen = 37,
            .txen = 36,
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
            .led = 35,
            .vbat_adc = 5,
            .button = -1,
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

#    define CURRENT_BOARD_CONFIG muzi_base_config

#endif // BOARD_MUZI_BASE

/* ============================================================================
 * Muzi R1 Neo
 * ========================================================================== */
#ifdef BOARD_MUZI_R1_NEO

static const struct board_config muzi_r1_neo_config = {
    .name = "Muzi R1 Neo",
    .vendor = "Muzi",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 45,
            .miso = 46,
            .sck = 43,
            .cs = 42,
            .reset = 38,
            .busy = 39,
            .dio0 = -1,
            .dio1 = 47,
            .rxen = 37,
            .txen = 36,
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
            .led = 35,
            .vbat_adc = 5,
            .button = -1,
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

#    define CURRENT_BOARD_CONFIG muzi_r1_neo_config

#endif // BOARD_MUZI_R1_NEO

#endif // BOARD_MUZI_VARIANTS_H
