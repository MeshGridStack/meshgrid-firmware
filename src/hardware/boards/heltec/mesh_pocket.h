/**
 * Heltec Mesh Pocket board configuration
 *
 * nRF52840 + SX1262
 * Pocket-sized mesh node
 */

#ifndef BOARD_HELTEC_MESH_POCKET_CONFIG_H
#define BOARD_HELTEC_MESH_POCKET_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_HELTEC_MESH_POCKET

static const struct board_config heltec_mesh_pocket_config = {
    .name = "Mesh Pocket",
    .vendor = "Heltec",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 3,
            .miso = 4,
            .sck = 2,
            .cs = 10,
            .reset = 9,
            .busy = 11,
            .dio0 = -1,
            .dio1 = 8,
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
            .led = 15,
            .vbat_adc = 31,
            .button = 23,
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

#    define CURRENT_BOARD_CONFIG heltec_mesh_pocket_config

#endif // BOARD_HELTEC_MESH_POCKET
#endif // BOARD_HELTEC_MESH_POCKET_CONFIG_H
