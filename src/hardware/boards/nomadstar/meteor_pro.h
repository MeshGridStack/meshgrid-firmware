/**
 * NomadStar Meteor Pro board configuration
 *
 * nRF52840 + SX1262
 * Rugged outdoor mesh node
 */

#ifndef BOARD_NOMADSTAR_METEOR_PRO_CONFIG_H
#define BOARD_NOMADSTAR_METEOR_PRO_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_NOMADSTAR_METEOR_PRO

static const struct board_config nomadstar_meteor_pro_config = {
    .name = "Meteor Pro",
    .vendor = "NomadStar",

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

#    define CURRENT_BOARD_CONFIG nomadstar_meteor_pro_config

#endif // BOARD_NOMADSTAR_METEOR_PRO
#endif // BOARD_NOMADSTAR_METEOR_PRO_CONFIG_H
