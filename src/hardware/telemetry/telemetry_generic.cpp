/**
 * Generic telemetry driver - Fallback for unknown boards
 *
 * Provides minimal telemetry using ESP32 internal sensors
 */

#include "telemetry.h"
#include <Arduino.h>

static int generic_init(void)
{
    return 0;
}

static uint16_t generic_read_battery_mv(void)
{
    /* No battery ADC - return 0 */
    return 0;
}

static uint16_t generic_read_solar_mv(void)
{
    return 0;
}

static bool generic_is_usb_power(void)
{
    /* Assume USB power if we're running */
    return true;
}

static bool generic_is_charging(void)
{
    return false;
}

static int16_t generic_read_temp(void)
{
#ifdef ESP32
    /* Use ESP32 internal temperature sensor */
    float temp = temperatureRead();
    return (int16_t)(temp * 10.0f);
#else
    return 0;
#endif
}

static void generic_adc_enable(bool enable)
{
    (void)enable;
}

/* Export operations struct */
static const struct telemetry_ops generic_telemetry_ops = {
    .init = generic_init,
    .read_battery_mv = generic_read_battery_mv,
    .read_solar_mv = generic_read_solar_mv,
    .is_usb_power = generic_is_usb_power,
    .is_charging = generic_is_charging,
    .read_temp = generic_read_temp,
    .adc_enable = generic_adc_enable,
};

/* Used when no specific board driver is selected */
#if !defined(BOARD_HELTEC_V3) && !defined(BOARD_HELTEC_V4) && !defined(BOARD_LILYGO_TBEAM) && !defined(BOARD_LILYGO_TBEAM_SUPREME)
const struct telemetry_ops *board_telemetry_ops = &generic_telemetry_ops;
#endif
