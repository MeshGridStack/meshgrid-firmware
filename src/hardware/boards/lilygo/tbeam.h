/**
 * LilyGo T-Beam board configuration
 *
 * ESP32 + SX1276 + GPS
 */

#ifndef BOARD_LILYGO_TBEAM_CONFIG_H
#define BOARD_LILYGO_TBEAM_CONFIG_H

#include "hardware/board.h"

#ifdef BOARD_LILYGO_TBEAM

/* AXP auto-detect power ops - supports both AXP192 (v1.0/1.1) and AXP2101 (v1.2) */
extern const struct power_ops axp_auto_power_ops;

static const struct board_config lilygo_tbeam_config = {
    .name = "T-Beam",
    .vendor = "LilyGo",

    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_UBLOX,

    .radio_pins =
        {
            .mosi = 27,
            .miso = 19,
            .sck = 5,
            .cs = 18,
            .reset = 23,
            .busy = -1,
            .dio0 = 26,
            .dio1 = -1, // SX1276 uses DIO0 only, not DIO1
            .rxen = -1,
            .txen = -1,
        },

    .display_pins =
        {
            .sda = 21,
            .scl = 22,
            .reset = -1,
            .addr = 0x3C,
            .width = 128,
            .height = 64,
        },

    .gps_pins =
        {
            .rx = 34,
            .tx = 12,
            .pps = -1,
            .enable = -1,
            .baud = 9600,
        },

    .power_pins =
        {
            .vext = -1,
            .led = 4,
            .vbat_adc = 35,
            .button = 38,
            .vext_active_low = false,
        },

    .lora_defaults =
        {
            .frequency = 868.0,
            .bandwidth = 125.0,
            .spreading_factor = 9,
            .coding_rate = 7,
            .tx_power = 20,
            .preamble_len = 8,
            .use_crc = true,
            .tcxo_voltage = 0.0,       // No TCXO on T-Beam
            .dio2_as_rf_switch = false, // SX1276 doesn't use DIO2 as RF switch
            .sync_word = 0,            // Use RadioLib default for SX1276
        },

    .radio_ops = NULL, /* Auto-detect from radio type */
    .power_ops = &axp_auto_power_ops,
    .gps_ops = NULL,

    .early_init = NULL,
    .late_init = NULL,
};

#    define CURRENT_BOARD_CONFIG lilygo_tbeam_config

#endif
#endif
