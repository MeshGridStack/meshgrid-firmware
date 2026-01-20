/**
 * Seeed XIAO nRF52840 board configuration
 *
 * nRF52840 ultra-compact development board
 * Can be used with external LoRa modules
 */

#ifndef BOARD_SEEED_XIAO_NRF52840_CONFIG_H
#define BOARD_SEEED_XIAO_NRF52840_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_SEEED_XIAO_NRF52840

static const struct board_config seeed_xiao_nrf52840_config = {
    .name = "XIAO nRF52840",
    .vendor = "Seeed Studio",

    .radio = RADIO_SX1262, // External module
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 10, // D10/MOSI
            .miso = 9,  // D9/MISO
            .sck = 8,   // D8/SCK
            .cs = 7,    // D7
            .reset = 6, // D6
            .busy = 5,  // D5
            .dio0 = -1,
            .dio1 = 4, // D4
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
            .led = 26, // Built-in LED
            .vbat_adc = 32, // AIN0
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

#    define CURRENT_BOARD_CONFIG seeed_xiao_nrf52840_config

#endif // BOARD_SEEED_XIAO_NRF52840
#endif // BOARD_SEEED_XIAO_NRF52840_CONFIG_H
