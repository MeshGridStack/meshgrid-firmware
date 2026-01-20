/**
 * Heltec WiFi LoRa 32 V4 board configuration
 *
 * ESP32-S3 + SX1262 + SSD1306 128x64 OLED
 */

#ifndef BOARD_HELTEC_V4_CONFIG_H
#define BOARD_HELTEC_V4_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_HELTEC_V4

/* Heltec V4 power ops - implemented in drivers/power/power_heltec_v4.cpp */
extern const struct power_ops heltec_v4_power_ops;

static const struct board_config heltec_v4_config = {
    .name = "Heltec V4",
    .vendor = "Heltec",

    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,

    .radio_pins =
        {
            .mosi = 10,
            .miso = 11,
            .sck = 9,
            .cs = 8,
            .reset = 12,
            .busy = 13,
            .dio0 = -1,
            .dio1 = 14,
            .rxen = -1,
            .txen = -1,
        },

    .display_pins =
        {
            .sda = 17, /* V4 uses same I2C as V3 */
            .scl = 18,
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
            .vext = 36,
            .led = 35,
            .vbat_adc = 1,
            .button = 0,
            .vext_active_low = true, /* V4 leaves VEXT inactive (LOW) - same as V3 */
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
            .tcxo_voltage = 1.8,       // Heltec V4 has TCXO
            .dio2_as_rf_switch = true, // Uses DIO2 for RF switching
            .sync_word = 0x12,         // RADIOLIB_SX126X_SYNC_WORD_PRIVATE for MeshCore compat
        },

    .radio_ops = NULL, /* Auto-detect from radio type */
    .power_ops = &heltec_v4_power_ops,
    .gps_ops = NULL,

    .early_init = NULL,
    .late_init = NULL,
};

#    define CURRENT_BOARD_CONFIG heltec_v4_config

#endif // BOARD_HELTEC_V4
#endif // BOARD_HELTEC_V4_CONFIG_H
