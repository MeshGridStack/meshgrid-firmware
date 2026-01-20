/**
 * LilyGo T-LoRa T3S3 board configuration
 *
 * ESP32-S3 + SX1262 + SSD1306 128x64 OLED
 */

#ifndef BOARD_LILYGO_T3S3_CONFIG_H
#define BOARD_LILYGO_T3S3_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_LILYGO_T3S3

static const struct board_config lilygo_t3s3_config = {
    .name = "T3S3",
    .vendor = "LilyGo",

    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 6,
            .miso = 3,
            .sck = 5,
            .cs = 7,
            .reset = 8,
            .busy = 34,
            .dio0 = -1,
            .dio1 = 33,
            .rxen = -1,
            .txen = -1,
        },

    .display_pins =
        {
            .sda = 18,
            .scl = 17,
            .reset = 21,
            .addr = 0x3C,
            .width = 128,
            .height = 64,
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
            .led = 37,
            .vbat_adc = 1,
            .button = 0,
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
            .tcxo_voltage = 1.8,       // T3S3 has TCXO
            .dio2_as_rf_switch = true, // Uses DIO2 for RF switching
            .sync_word = 0x12,         // RADIOLIB_SX126X_SYNC_WORD_PRIVATE for MeshCore compat
        },

    .radio_ops = NULL, /* Auto-detect from radio type */
    .power_ops = NULL, /* Use simple GPIO power (fallback) */
    .gps_ops = NULL,

    .early_init = NULL,
    .late_init = NULL,
};

#    define CURRENT_BOARD_CONFIG lilygo_t3s3_config

#endif // BOARD_LILYGO_T3S3
#endif // BOARD_LILYGO_T3S3_CONFIG_H
