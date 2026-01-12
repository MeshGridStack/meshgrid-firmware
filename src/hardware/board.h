/**
 * meshgrid-firmware - Board abstraction layer
 *
 * Linux kernel-style board configuration with ops structures.
 */

#ifndef MESHGRID_BOARD_H
#define MESHGRID_BOARD_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct power_ops;
struct gps_ops;
struct radio_ops;

#ifdef __cplusplus
class Adafruit_SSD1306;  /* For display access in board-specific code */
/* Global hardware objects accessible to board code */
extern class Adafruit_SSD1306 *display;
#endif

/**
 * Pin configuration for SPI-based radio (SX126x, SX127x)
 */
struct radio_pins {
    int8_t mosi;
    int8_t miso;
    int8_t sck;
    int8_t cs;
    int8_t reset;
    int8_t busy;    // -1 if not available (SX127x)
    int8_t dio0;    // IRQ for SX127x
    int8_t dio1;    // IRQ for SX126x
    int8_t rxen;    // RX enable for external LNA (-1 if not used)
    int8_t txen;    // TX enable for external PA (-1 if not used)
};

/**
 * Pin configuration for I2C display
 */
struct display_pins {
    int8_t sda;
    int8_t scl;
    int8_t reset;   // -1 if not available
    uint8_t addr;   // I2C address (0x3C or 0x3D)
    uint16_t width;
    uint16_t height;
};

/**
 * Pin configuration for GPS
 */
struct gps_pins {
    int8_t rx;
    int8_t tx;
    int8_t pps;     // -1 if not available
    int8_t enable;  // -1 if not available
    uint32_t baud;
};

/**
 * Power management pins
 */
struct power_pins {
    int8_t vext;        // External power enable (-1 if not available)
    int8_t led;         // Status LED (-1 if not available)
    int8_t vbat_adc;    // Battery ADC pin (-1 if not available)
    int8_t button;      // User button (-1 if not available)
    bool vext_active_low;
};

/**
 * Radio type enumeration
 */
enum radio_type {
    RADIO_NONE = 0,
    RADIO_SX1262,
    RADIO_SX1268,
    RADIO_SX1276,
    RADIO_SX1278,
    RADIO_SX1280,   // 2.4GHz
    RADIO_LR1110,
    RADIO_LR1120,
    RADIO_LR1121,
};

/**
 * Display type enumeration
 */
enum display_type {
    DISPLAY_NONE = 0,
    DISPLAY_SSD1306_128X64,
    DISPLAY_SSD1306_128X32,
    DISPLAY_SH1106_128X64,
    DISPLAY_ST7735,
    DISPLAY_ST7789,
    DISPLAY_EINK_GDEY0154D67,   // 1.54" E-Ink
    DISPLAY_EINK_GDEY0213B74,   // 2.13" E-Ink
    DISPLAY_EINK_GDEY029T94,    // 2.9" E-Ink
};

/**
 * GPS type enumeration
 */
enum gps_type {
    GPS_NONE = 0,
    GPS_UBLOX,
    GPS_ATGM336H,
    GPS_L76K,
};

/**
 * LoRa default configuration
 */
struct lora_config {
    float frequency;        // MHz
    float bandwidth;        // kHz
    uint8_t spreading_factor;
    uint8_t coding_rate;    // 5-8 for 4/5 to 4/8
    int8_t tx_power;        // dBm
    uint16_t preamble_len;
    bool use_crc;
    float tcxo_voltage;     // TCXO reference voltage (0 = no TCXO)
    bool dio2_as_rf_switch; // Use DIO2 as RF switch (common on SX1262)
};

/**
 * Board configuration structure
 *
 * Each board defines one of these with its specific configuration.
 */
struct board_config {
    const char *name;
    const char *vendor;

    // Hardware
    enum radio_type radio;
    enum display_type display;
    enum gps_type gps;

    // Pin configurations
    struct radio_pins radio_pins;
    struct display_pins display_pins;
    struct gps_pins gps_pins;
    struct power_pins power_pins;

    // LoRa defaults
    struct lora_config lora_defaults;

    // Hardware abstraction layer (optional)
    const struct radio_ops *radio_ops;  /* Radio chip driver (NULL = auto-detect from radio type) */
    const struct power_ops *power_ops;  /* Power management (NULL = use simple GPIO) */
    const struct gps_ops *gps_ops;      /* GPS driver (NULL = no GPS) */

    // Board-specific init/deinit (legacy, prefer using *_ops)
    void (*early_init)(void);
    void (*late_init)(void);
};

/**
 * Get the current board configuration
 */
const struct board_config *board_get_config(void);

/**
 * Initialize the board
 */
int board_init(void);

/**
 * Board early initialization (before peripherals)
 */
void board_early_init(void);

#endif // MESHGRID_BOARD_H
