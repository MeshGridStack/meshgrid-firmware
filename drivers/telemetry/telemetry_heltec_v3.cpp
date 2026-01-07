/**
 * Heltec WiFi LoRa 32 V3 telemetry driver
 *
 * Battery: ADC on GPIO1, controlled by GPIO37
 * No solar input on stock board
 * USB power detection via voltage level
 */

#include "telemetry.h"
#include <Arduino.h>

/* Heltec V3 pin definitions */
#define PIN_VBAT_READ       1   /* ADC input for battery voltage */
#define PIN_ADC_CTRL        37  /* ADC enable control */

/* Heltec V3 voltage divider calibration */
/* Formula from MeshCore: (5.42 * (3.3 / 1024.0) * raw) * 1000 */
/* For 12-bit ADC: (5.42 * (3.3 / 4096.0) * raw) * 1000 */
#define VBAT_CALIBRATION_FACTOR  5.42f
#define ADC_VREF                 3.3f
#define ADC_MAX                  4095.0f  /* 12-bit ADC */

static int heltec_v3_init(void)
{
    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW);  /* Start with ADC disabled */

    /* Configure ADC */
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  /* Full range 0-3.3V */

    return 0;
}

static uint16_t heltec_v3_read_battery_mv(void)
{
    uint32_t raw = analogRead(PIN_VBAT_READ);

    /* Apply calibration formula */
    float voltage = VBAT_CALIBRATION_FACTOR * (ADC_VREF / ADC_MAX) * raw;
    uint16_t mv = (uint16_t)(voltage * 1000.0f);

    return mv;
}

static uint16_t heltec_v3_read_solar_mv(void)
{
    /* Stock Heltec V3 has no solar input */
    return 0;
}

static bool heltec_v3_is_usb_power(void)
{
    /* Detect USB by battery voltage being high (>4.3V indicates USB power) */
    uint16_t mv = heltec_v3_read_battery_mv();
    return (mv > 4300);
}

static bool heltec_v3_is_charging(void)
{
    /* Heltec V3 has no charge status pin exposed */
    /* Estimate: USB connected + battery < 4.15V = probably charging */
    if (heltec_v3_is_usb_power()) {
        uint16_t mv = heltec_v3_read_battery_mv();
        return (mv < 4150);
    }
    return false;
}

static int16_t heltec_v3_read_temp(void)
{
    /* Use ESP32-S3 internal temperature sensor */
    /* Returns temp in 0.1°C units */
    float temp = temperatureRead();
    return (int16_t)(temp * 10.0f);
}

static void heltec_v3_adc_enable(bool enable)
{
    digitalWrite(PIN_ADC_CTRL, enable ? HIGH : LOW);
}

/* Export operations struct */
static const struct telemetry_ops heltec_v3_telemetry_ops = {
    .init = heltec_v3_init,
    .read_battery_mv = heltec_v3_read_battery_mv,
    .read_solar_mv = heltec_v3_read_solar_mv,
    .is_usb_power = heltec_v3_is_usb_power,
    .is_charging = heltec_v3_is_charging,
    .read_temp = heltec_v3_read_temp,
    .adc_enable = heltec_v3_adc_enable,
};

/* Board selector will point to this */
const struct telemetry_ops *board_telemetry_ops = &heltec_v3_telemetry_ops;
