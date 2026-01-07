/**
 * meshgrid telemetry - Core implementation
 */

#include "telemetry.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

/* Platform-specific free heap */
#if defined(ARCH_ESP32S3) || defined(ARCH_ESP32) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)
#define GET_FREE_HEAP() ESP.getFreeHeap()
#elif defined(ARCH_NRF52840)
extern "C" uint32_t dbgHeapTotal(void);
#define GET_FREE_HEAP() dbgHeapTotal()
#elif defined(ARCH_RP2040)
extern "C" char *sbrk(int incr);
#define GET_FREE_HEAP() (256 * 1024 - ((char*)sbrk(0) - (char*)0x20000000))
#else
#define GET_FREE_HEAP() 0
#endif

/* External board-specific ops (defined by board driver) */
extern const struct telemetry_ops *board_telemetry_ops;

static bool telemetry_initialized = false;
static uint32_t boot_time_ms = 0;

const struct telemetry_ops *telemetry_get_ops(void)
{
    return board_telemetry_ops;
}

void telemetry_init(void)
{
    const struct telemetry_ops *ops = telemetry_get_ops();

    if (ops && ops->init) {
        ops->init();
    }

    boot_time_ms = millis();
    telemetry_initialized = true;
}

void telemetry_read(struct telemetry_data *data)
{
    const struct telemetry_ops *ops = telemetry_get_ops();

    memset(data, 0, sizeof(*data));

    if (!ops) {
        return;
    }

    /* Enable ADC if board supports it */
    if (ops->adc_enable) {
        ops->adc_enable(true);
        delay(10);  /* Let ADC settle */
    }

    /* Read battery */
    if (ops->read_battery_mv) {
        data->battery_mv = ops->read_battery_mv();
        data->battery_pct = telemetry_voltage_to_percent(data->battery_mv);
    }

    /* Read solar */
    if (ops->read_solar_mv) {
        data->solar_mv = ops->read_solar_mv();
        data->has_solar = (data->solar_mv > 100);  /* >0.1V = solar present */
    }

    /* Check power state */
    if (ops->is_usb_power) {
        data->is_usb_power = ops->is_usb_power();
    }
    if (ops->is_charging) {
        data->is_charging = ops->is_charging();
    }

    /* Read temperature */
    if (ops->read_temp) {
        data->temp_deci_c = ops->read_temp();
        data->has_temp = true;
    }

    /* Disable ADC to save power */
    if (ops->adc_enable) {
        ops->adc_enable(false);
    }

    /* System stats */
    data->uptime_secs = (millis() - boot_time_ms) / 1000;
    data->free_heap = GET_FREE_HEAP();

    /* CPU load estimation (simplified) */
    data->cpu_load = 0;  /* TODO: implement proper CPU load tracking */
}

/**
 * Li-ion battery discharge curve (3.0V - 4.2V)
 * Based on typical discharge curve for 18650 cells
 */
uint8_t telemetry_voltage_to_percent(uint16_t mv)
{
    /* Voltage thresholds for Li-ion */
    const uint16_t VBAT_FULL = 4200;    /* 100% */
    const uint16_t VBAT_NOMINAL = 3700; /* ~50% */
    const uint16_t VBAT_LOW = 3400;     /* ~20% */
    const uint16_t VBAT_EMPTY = 3000;   /* 0% - cutoff */

    if (mv >= VBAT_FULL) {
        return 100;
    }
    if (mv <= VBAT_EMPTY) {
        return 0;
    }

    /* Piecewise linear approximation of discharge curve */
    if (mv >= VBAT_NOMINAL) {
        /* 50-100%: relatively flat */
        return 50 + ((mv - VBAT_NOMINAL) * 50) / (VBAT_FULL - VBAT_NOMINAL);
    } else if (mv >= VBAT_LOW) {
        /* 20-50%: steeper slope */
        return 20 + ((mv - VBAT_LOW) * 30) / (VBAT_NOMINAL - VBAT_LOW);
    } else {
        /* 0-20%: steep drop */
        return ((mv - VBAT_EMPTY) * 20) / (VBAT_LOW - VBAT_EMPTY);
    }
}

void telemetry_format_battery(char *buf, size_t len, const struct telemetry_data *data)
{
    const char *status = "";

    if (data->is_charging) {
        status = " CHG";
    } else if (data->is_usb_power) {
        status = " USB";
    }

    snprintf(buf, len, "%d.%02dV %d%%%s",
             data->battery_mv / 1000,
             (data->battery_mv % 1000) / 10,
             data->battery_pct,
             status);
}

void telemetry_format_power(char *buf, size_t len, const struct telemetry_data *data)
{
    if (data->has_solar && data->solar_mv > 100) {
        snprintf(buf, len, "Solar: %d.%02dV",
                 data->solar_mv / 1000,
                 (data->solar_mv % 1000) / 10);
    } else {
        snprintf(buf, len, "No solar");
    }
}
