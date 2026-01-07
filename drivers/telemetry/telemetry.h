/**
 * meshgrid telemetry - Battery, solar, power monitoring
 *
 * Board-agnostic interface with per-board implementations.
 */

#ifndef MESHGRID_TELEMETRY_H
#define MESHGRID_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Telemetry data structure
 */
struct telemetry_data {
    /* Battery */
    uint16_t battery_mv;        /* Battery voltage in millivolts */
    uint8_t battery_pct;        /* Battery percentage 0-100 */
    bool is_charging;           /* True if battery is charging */
    bool is_usb_power;          /* True if USB power connected */

    /* Solar */
    uint16_t solar_mv;          /* Solar panel voltage in millivolts */
    uint16_t solar_ma;          /* Solar current in milliamps */
    bool has_solar;             /* True if solar panel detected */

    /* Power consumption */
    uint16_t current_ma;        /* Current draw in milliamps */

    /* Temperature */
    int16_t temp_deci_c;        /* Temperature in 0.1°C units */
    bool has_temp;              /* True if temperature sensor available */

    /* System */
    uint32_t uptime_secs;       /* Uptime in seconds */
    uint32_t free_heap;         /* Free heap memory */
    uint8_t cpu_load;           /* CPU load percentage */
};

/**
 * Board-specific telemetry operations
 */
struct telemetry_ops {
    /* Initialize telemetry hardware */
    int (*init)(void);

    /* Read battery voltage in millivolts */
    uint16_t (*read_battery_mv)(void);

    /* Read solar voltage in millivolts (0 if not supported) */
    uint16_t (*read_solar_mv)(void);

    /* Check if USB power is connected */
    bool (*is_usb_power)(void);

    /* Check if battery is charging */
    bool (*is_charging)(void);

    /* Read temperature in 0.1°C units (0 if not supported) */
    int16_t (*read_temp)(void);

    /* ADC control (enable/disable for power saving) */
    void (*adc_enable)(bool enable);
};

/**
 * Get telemetry operations for current board
 */
const struct telemetry_ops *telemetry_get_ops(void);

/**
 * High-level telemetry functions
 */
void telemetry_init(void);
void telemetry_read(struct telemetry_data *data);

/* Convert battery voltage to percentage (Li-ion 3.0V-4.2V curve) */
uint8_t telemetry_voltage_to_percent(uint16_t mv);

/* Format telemetry for display */
void telemetry_format_battery(char *buf, size_t len, const struct telemetry_data *data);
void telemetry_format_power(char *buf, size_t len, const struct telemetry_data *data);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_TELEMETRY_H */
