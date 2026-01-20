/**
 * nRF52 Pro Micro DIY board configuration
 *
 * nRF52840 Pro Micro form factor + external LoRa module
 * DIY mesh node configuration
 */

#ifndef BOARD_NRF52_PROMICRO_DIY_CONFIG_H
#define BOARD_NRF52_PROMICRO_DIY_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_NRF52_PROMICRO_DIY

static const struct board_config nrf52_promicro_diy_config = {
    .name = "nRF52 Pro Micro DIY",
    .vendor = "DIY",

    .radio = RADIO_SX1262, // External module
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 10, // Standard SPI pins
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
            .led = 3, // Red LED on Pro Micro
            .vbat_adc = 31, // AIN7
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

#    define CURRENT_BOARD_CONFIG nrf52_promicro_diy_config

#endif // BOARD_NRF52_PROMICRO_DIY
#endif // BOARD_NRF52_PROMICRO_DIY_CONFIG_H
