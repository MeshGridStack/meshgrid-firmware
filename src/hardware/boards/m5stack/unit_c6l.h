/**
 * M5Stack Unit C6L board configuration
 *
 * ESP32-C6 + SX1262 LoRa module
 * Compact LoRa unit for M5Stack ecosystem
 */

#ifndef BOARD_M5STACK_UNIT_C6L_CONFIG_H
#define BOARD_M5STACK_UNIT_C6L_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_M5STACK_UNIT_C6L

static const struct board_config m5stack_unit_c6l_config = {
    .name = "M5Stack Unit C6L",
    .vendor = "M5Stack",

    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 7,
            .miso = 2,
            .sck = 6,
            .cs = 3,
            .reset = 5,
            .busy = 4,
            .dio0 = -1,
            .dio1 = 1,
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
            .vbat_adc = 0, // ADC1_CH0
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

#    define CURRENT_BOARD_CONFIG m5stack_unit_c6l_config

#endif // BOARD_M5STACK_UNIT_C6L
#endif // BOARD_M5STACK_UNIT_C6L_CONFIG_H
