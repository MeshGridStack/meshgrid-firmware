/**
 * Raspberry Pi Pico board configuration
 *
 * RP2040 + external LoRa module
 * Standard Pico with external radio
 */

#ifndef BOARD_RPI_PICO_CONFIG_H
#define BOARD_RPI_PICO_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_RPI_PICO

static const struct board_config rpi_pico_config = {
    .name = "Raspberry Pi Pico",
    .vendor = "Raspberry Pi",

    .radio = RADIO_SX1262, // External module
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 19,  // GP19/SPI0_TX
            .miso = 16,  // GP16/SPI0_RX
            .sck = 18,   // GP18/SPI0_SCK
            .cs = 17,    // GP17/SPI0_CS
            .reset = 14, // GP14
            .busy = 13,  // GP13
            .dio0 = -1,
            .dio1 = 15, // GP15
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
            .led = 25,      // Built-in LED
            .vbat_adc = 26, // ADC0
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

#    define CURRENT_BOARD_CONFIG rpi_pico_config

#endif // BOARD_RPI_PICO
#endif // BOARD_RPI_PICO_CONFIG_H
