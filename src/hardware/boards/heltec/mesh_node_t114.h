/**
 * Heltec Mesh Node T114 board configuration
 *
 * nRF52840 + SX1262
 * Compact mesh node with 1.14" LCD display
 */

#ifndef BOARD_HELTEC_MESH_NODE_T114_CONFIG_H
#define BOARD_HELTEC_MESH_NODE_T114_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_HELTEC_MESH_NODE_T114

static const struct board_config heltec_mesh_node_t114_config = {
    .name = "Mesh Node T114",
    .vendor = "Heltec",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE, // LCD, not OLED (not yet supported)
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
            .vbat_adc = 31, // AIN7
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

#    define CURRENT_BOARD_CONFIG heltec_mesh_node_t114_config

#endif // BOARD_HELTEC_MESH_NODE_T114
#endif // BOARD_HELTEC_MESH_NODE_T114_CONFIG_H
