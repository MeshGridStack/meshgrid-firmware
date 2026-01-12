/**
 * meshgrid-firmware - Board selection header
 *
 * This file selects the appropriate board configuration based on
 * the BOARD_* define from platformio.ini
 */

#ifndef MESHGRID_BOARDS_H
#define MESHGRID_BOARDS_H

#include "hardware/board.h"

// =============================================================================
// Default LoRa configurations by region
// =============================================================================

#define LORA_CONFIG_EU868 { \
    .frequency = 869.618, \
    .bandwidth = 62.5, \
    .spreading_factor = 8, \
    .coding_rate = 8, \
    .tx_power = 14, \
    .preamble_len = 8, \
    .use_crc = true, \
    .tcxo_voltage = 1.8, \
    .dio2_as_rf_switch = true \
}

#define LORA_CONFIG_US915 { \
    .frequency = 915.0, \
    .bandwidth = 250.0, \
    .spreading_factor = 10, \
    .coding_rate = 7, \
    .tx_power = 22, \
    .preamble_len = 16, \
    .use_crc = true, \
    .tcxo_voltage = 1.8, \
    .dio2_as_rf_switch = true \
}

#define LORA_CONFIG_AU915 LORA_CONFIG_US915

#define LORA_CONFIG_CN470 { \
    .frequency = 470.0, \
    .bandwidth = 250.0, \
    .spreading_factor = 10, \
    .coding_rate = 7, \
    .tx_power = 17, \
    .preamble_len = 16, \
    .use_crc = true, \
    .tcxo_voltage = 1.8, \
    .dio2_as_rf_switch = true \
}

// Default to US915 - can be overridden
#ifndef LORA_REGION
#define LORA_REGION LORA_CONFIG_US915
#endif

// =============================================================================
// Heltec ESP32-S3 Boards
// =============================================================================

#if defined(BOARD_HELTEC_V3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "V3",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 17, .scl = 18, .reset = 21, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 36, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = true },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_V4)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "V4",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 17, .scl = 18, .reset = 21, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 36, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_WIRELESS_STICK_LITE_V3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Wireless Stick Lite V3",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_WIRELESS_TRACKER)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Wireless Tracker",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7735,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 18, .scl = 17, .reset = 21, .addr = 0, .width = 80, .height = 160 },
    .gps_pins = { .rx = 34, .tx = 33, .pps = -1, .enable = 3, .baud = 9600 },
    .power_pins = { .vext = 36, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_WIRELESS_PAPER)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Wireless Paper",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_EINK_GDEY0213B74,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = 21, .addr = 0, .width = 250, .height = 122 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 45, .led = -1, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_VISION_MASTER_T190)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Vision Master T190",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = 21, .addr = 0, .width = 170, .height = 320 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 36, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_VISION_MASTER_E213) || defined(BOARD_HELTEC_VISION_MASTER_E290)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Vision Master E-Ink",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_EINK_GDEY0213B74,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 10, .miso = 11, .sck = 9, .cs = 8, .reset = 12, .busy = 13, .dio0 = -1, .dio1 = 14, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = 21, .addr = 0, .width = 250, .height = 122 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 36, .led = 35, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_HT62)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "HT62",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 5, .sck = 4, .cs = 7, .reset = 3, .busy = 2, .dio0 = -1, .dio1 = 1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 18, .vbat_adc = 0, .button = 9, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_MESH_NODE_T114)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Mesh Node T114",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 44, .miso = 46, .sck = 45, .cs = 42, .reset = 17, .busy = 13, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = 26, .addr = 0, .width = 135, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = 32, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_HELTEC_MESH_POCKET)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "MeshPocket",
    .vendor = "Heltec",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 44, .miso = 46, .sck = 45, .cs = 42, .reset = 17, .busy = 13, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 12, .scl = 13, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = 32, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// LilyGo Boards
// =============================================================================

#elif defined(BOARD_LILYGO_T3S3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-LoRa T3-S3",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 18, .scl = 17, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_T3S3_EINK)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-LoRa T3-S3 E-Ink",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_EINK_GDEY0154D67,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 200, .height = 200 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TBEAM_SUPREME)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Beam Supreme",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 35, .miso = 37, .sck = 36, .cs = 39, .reset = 38, .busy = 33, .dio0 = -1, .dio1 = 34, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 17, .scl = 18, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = 9, .tx = 8, .pps = -1, .enable = 7, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 4, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TBEAM)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Beam",
    .vendor = "LilyGo",
    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 23, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 21, .scl = 22, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = 34, .tx = 12, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 4, .vbat_adc = 35, .button = 38, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TECHO)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Echo",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_EINK_GDEY0154D67,
    .gps = GPS_L76K,
    .radio_pins = { .mosi = 22, .miso = 23, .sck = 19, .cs = 24, .reset = 25, .busy = 17, .dio0 = -1, .dio1 = 20, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 200, .height = 200 },
    .gps_pins = { .rx = 9, .tx = 10, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = 32, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TDECK)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Deck",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 41, .miso = 38, .sck = 40, .cs = 9, .reset = 17, .busy = 13, .dio0 = -1, .dio1 = 45, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 320, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = 10, .led = -1, .vbat_adc = 4, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TDECK_PRO)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Deck Pro",
    .vendor = "LilyGo",
    .radio = RADIO_LR1121,
    .display = DISPLAY_ST7789,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 41, .miso = 38, .sck = 40, .cs = 9, .reset = 17, .busy = 13, .dio0 = -1, .dio1 = 45, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 320, .height = 240 },
    .gps_pins = { .rx = 44, .tx = 43, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = 10, .led = -1, .vbat_adc = 4, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TLORA_PAGER)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-LoRa Pager",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 240, .height = 320 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TWATCH_S3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-Watch S3",
    .vendor = "LilyGo",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 13, .miso = 14, .sck = 3, .cs = 12, .reset = 45, .busy = 47, .dio0 = -1, .dio1 = 48, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 240, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = -1, .vbat_adc = 4, .button = 21, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_LILYGO_TLORA_V21_16) || defined(BOARD_LILYGO_TLORA_V21_18)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "T-LoRa V2.1",
    .vendor = "LilyGo",
    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 14, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 21, .scl = 22, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 25, .vbat_adc = 35, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// RAK Boards
// =============================================================================

#elif defined(BOARD_RAK4631) || defined(BOARD_RAK_WISMESH_REPEATER) || defined(BOARD_RAK_WISMESH_TAP) || defined(BOARD_RAK_WISMESH_TAG) || defined(BOARD_RAK3401_1W)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "WisBlock 4631",
    .vendor = "RAK",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = 37, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_RAK11200)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "WisBlock 11200",
    .vendor = "RAK",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 19, .miso = 21, .sck = 18, .cs = 32, .reset = 14, .busy = 34, .dio0 = -1, .dio1 = 39, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 12, .vbat_adc = 35, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_RAK11310)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "WisBlock 11310",
    .vendor = "RAK",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 11, .miso = 12, .sck = 10, .cs = 13, .reset = 14, .busy = 15, .dio0 = -1, .dio1 = 29, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 23, .vbat_adc = 26, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_RAK3312)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "RAK3312",
    .vendor = "RAK",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 35, .miso = 37, .sck = 36, .cs = 39, .reset = 38, .busy = 33, .dio0 = -1, .dio1 = 34, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 4, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// Seeed Boards
// =============================================================================

#elif defined(BOARD_SEEED_TRACKER_T1000E)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Card Tracker T1000-E",
    .vendor = "Seeed",
    .radio = RADIO_LR1110,
    .display = DISPLAY_NONE,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 44, .reset = 42, .busy = 47, .dio0 = -1, .dio1 = 2, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = 9, .tx = 10, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = 32, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_SEEED_XIAO_NRF52840)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Xiao nRF52840 Kit",
    .vendor = "Seeed",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 44, .reset = 42, .busy = 47, .dio0 = -1, .dio1 = 2, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_SEEED_XIAO_ESP32S3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Xiao ESP32-S3",
    .vendor = "Seeed",
    .radio = RADIO_NONE,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 9, .miso = 8, .sck = 7, .cs = 44, .reset = 3, .busy = 5, .dio0 = -1, .dio1 = 4, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 21, .vbat_adc = -1, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_SEEED_SENSECAP_INDICATOR)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "SenseCAP Indicator",
    .vendor = "Seeed",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 5, .sck = 4, .cs = 3, .reset = 2, .busy = 1, .dio0 = -1, .dio1 = 0, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 480, .height = 480 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = -1, .vbat_adc = -1, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_SEEED_SENSECAP_SOLAR) || defined(BOARD_SEEED_WIO_TRACKER_L1) || defined(BOARD_SEEED_WIO_TRACKER_L1_EINK) || defined(BOARD_SEEED_WIO_WM1110)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "SenseCAP Solar",
    .vendor = "Seeed",
    .radio = RADIO_LR1110,
    .display = DISPLAY_NONE,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 44, .reset = 42, .busy = 47, .dio0 = -1, .dio1 = 2, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = 9, .tx = 10, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 14, .vbat_adc = 4, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// Elecrow ThinkNode Boards
// =============================================================================

#elif defined(BOARD_THINKNODE_M1)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "ThinkNode M1",
    .vendor = "Elecrow",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_THINKNODE_M2) || defined(BOARD_THINKNODE_M5)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "ThinkNode M2/M5",
    .vendor = "Elecrow",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 320, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_THINKNODE_M3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "ThinkNode M3",
    .vendor = "Elecrow",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_CROWPANEL_24TFT) || defined(BOARD_CROWPANEL_35TFT) || defined(BOARD_CROWPANEL_43TFT)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Crowpanel Adv TFT",
    .vendor = "Elecrow",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 480, .height = 320 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = -1, .vbat_adc = -1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// B&Q Consulting Boards
// =============================================================================

#elif defined(BOARD_STATION_G2)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Station G2",
    .vendor = "B&Q",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 35, .miso = 37, .sck = 36, .cs = 39, .reset = 38, .busy = 33, .dio0 = -1, .dio1 = 34, .rxen = 21, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 4, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_STATION_G1)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Station G1",
    .vendor = "B&Q",
    .radio = RADIO_SX1276,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 23, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 2, .vbat_adc = 35, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_NANO_G1) || defined(BOARD_NANO_G1_EXPLORER)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Nano G1",
    .vendor = "B&Q",
    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 23, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 21, .scl = 22, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 2, .vbat_adc = 35, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_NANO_G2_ULTRA)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Nano G2 Ultra",
    .vendor = "B&Q",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

// =============================================================================
// Other/DIY Boards
// =============================================================================

#elif defined(BOARD_MUZI_BASE) || defined(BOARD_MUZI_R1_NEO)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "muzi BASE",
    .vendor = "muzi",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_NOMADSTAR_METEOR_PRO)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Meteor Pro",
    .vendor = "NomadStar",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_UBLOX,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 13, .scl = 14, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = 9, .tx = 10, .pps = -1, .enable = -1, .baud = 9600 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_CANARY_ONE)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Canary One",
    .vendor = "Canary",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 45, .miso = 46, .sck = 43, .cs = 42, .reset = 38, .busy = 39, .dio0 = -1, .dio1 = 47, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 35, .vbat_adc = 5, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_RADIOMASTER_900_BANDIT)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "900 Bandit Nano",
    .vendor = "RadioMaster",
    .radio = RADIO_SX1276,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 14, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 2, .vbat_adc = -1, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_M5STACK)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "M5 Stack",
    .vendor = "M5Stack",
    .radio = RADIO_NONE,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = -1, .miso = -1, .sck = -1, .cs = -1, .reset = -1, .busy = -1, .dio0 = -1, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 320, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = -1, .vbat_adc = -1, .button = 39, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_M5STACK_UNIT_C6L)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Unit C6L",
    .vendor = "M5Stack",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 5, .sck = 4, .cs = 7, .reset = 3, .busy = 2, .dio0 = -1, .dio1 = 1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 8, .vbat_adc = -1, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_EBYTE_EORA_S3)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "EoRa-S3",
    .vendor = "EByte",
    .radio = RADIO_SX1262,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 18, .scl = 17, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_DIY_V1) || defined(BOARD_HYDRA) || defined(BOARD_NRF52_PROMICRO_DIY)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "DIY",
    .vendor = "DIY",
    .radio = RADIO_SX1276,
    .display = DISPLAY_SSD1306_128X64,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 27, .miso = 19, .sck = 5, .cs = 18, .reset = 14, .busy = -1, .dio0 = 26, .dio1 = -1, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = 21, .scl = 22, .reset = -1, .addr = 0x3C, .width = 128, .height = 64 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 2, .vbat_adc = 35, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_RP2040_LORA) || defined(BOARD_RPI_PICO) || defined(BOARD_RPI_PICO_W)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "RP2040 LoRa",
    .vendor = "Waveshare/RPi",
    .radio = RADIO_SX1262,
    .display = DISPLAY_NONE,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 11, .miso = 12, .sck = 10, .cs = 3, .reset = 15, .busy = 2, .dio0 = -1, .dio1 = 20, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 0, .height = 0 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 25, .vbat_adc = -1, .button = -1, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#elif defined(BOARD_TRACKSENGER_SMALL) || defined(BOARD_TRACKSENGER_BIG) || defined(BOARD_PI_COMPUTER_S3) || defined(BOARD_UNPHONE)
static const struct board_config CURRENT_BOARD_CONFIG = {
    .name = "Generic ESP32-S3",
    .vendor = "Generic",
    .radio = RADIO_SX1262,
    .display = DISPLAY_ST7789,
    .gps = GPS_NONE,
    .radio_pins = { .mosi = 6, .miso = 3, .sck = 5, .cs = 7, .reset = 8, .busy = 34, .dio0 = -1, .dio1 = 33, .rxen = -1, .txen = -1 },
    .display_pins = { .sda = -1, .scl = -1, .reset = -1, .addr = 0, .width = 320, .height = 240 },
    .gps_pins = { .rx = -1, .tx = -1, .pps = -1, .enable = -1, .baud = 0 },
    .power_pins = { .vext = -1, .led = 37, .vbat_adc = 1, .button = 0, .vext_active_low = false },
    .lora_defaults = LORA_REGION,
    .early_init = NULL,
    .late_init = NULL,
};

#else
#error "No board defined! Please set BOARD_* in platformio.ini"
#endif

#endif // MESHGRID_BOARDS_H
