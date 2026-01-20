/**
 * LilyGo T-Beam telemetry driver
 *
 * T-Beam has AXP192/AXP2101 PMU for power management
 * Supports battery, solar input, USB power detection
 */

#include "telemetry.h"
#include <Arduino.h>
#include <Wire.h>

/* AXP192 I2C address */
#define AXP192_ADDR         0x34
#define AXP2101_ADDR        0x34

/* AXP192 registers */
#define AXP192_POWER_STATUS     0x00
#define AXP192_CHARGE_STATUS    0x01
#define AXP192_VBAT_H           0x78
#define AXP192_VBAT_L           0x79
#define AXP192_VBUS_H           0x5A
#define AXP192_VBUS_L           0x5B
#define AXP192_TEMP_H           0x5E
#define AXP192_TEMP_L           0x5F
#define AXP192_ACIN_V_H         0x56  /* Solar/ACIN voltage */
#define AXP192_ACIN_V_L         0x57

/* T-Beam I2C pins */
#define I2C_SDA                 21
#define I2C_SCL                 22

static bool axp_detected = false;

static uint8_t axp_read_reg(uint8_t reg)
{
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)AXP192_ADDR, (uint8_t)1);
    return Wire.read();
}

static uint16_t axp_read_reg16(uint8_t reg_h, uint8_t reg_l)
{
    uint8_t h = axp_read_reg(reg_h);
    uint8_t l = axp_read_reg(reg_l);
    return ((uint16_t)h << 4) | (l & 0x0F);
}

static int tbeam_init(void)
{
    /* Wire.begin() already called in main.cpp setup() */

    /* Detect AXP192/AXP2101 */
    Wire.beginTransmission(AXP192_ADDR);
    if (Wire.endTransmission() == 0) {
        axp_detected = true;
    }

    return axp_detected ? 0 : -1;
}

static uint16_t tbeam_read_battery_mv(void)
{
    if (!axp_detected) {
        return 0;
    }

    /* AXP192 VBAT: 12-bit, 1.1mV per LSB */
    uint16_t raw = axp_read_reg16(AXP192_VBAT_H, AXP192_VBAT_L);
    return (raw * 11) / 10;  /* Convert to mV */
}

static uint16_t tbeam_read_solar_mv(void)
{
    if (!axp_detected) {
        return 0;
    }

    /* ACIN voltage: 12-bit, 1.7mV per LSB */
    uint16_t raw = axp_read_reg16(AXP192_ACIN_V_H, AXP192_ACIN_V_L);
    return (raw * 17) / 10;  /* Convert to mV */
}

static bool tbeam_is_usb_power(void)
{
    if (!axp_detected) {
        return false;
    }

    uint8_t status = axp_read_reg(AXP192_POWER_STATUS);
    return (status & 0x10) != 0;  /* VBUS presence bit */
}

static bool tbeam_is_charging(void)
{
    if (!axp_detected) {
        return false;
    }

    uint8_t status = axp_read_reg(AXP192_CHARGE_STATUS);
    return (status & 0x40) != 0;  /* Charging bit */
}

static int16_t tbeam_read_temp(void)
{
    if (!axp_detected) {
        return 0;
    }

    /* AXP192 internal temp: offset -144.7°C, 0.1°C per LSB */
    uint16_t raw = axp_read_reg16(AXP192_TEMP_H, AXP192_TEMP_L);
    int16_t temp = raw - 1447;  /* Already in 0.1°C units */
    return temp;
}

static void tbeam_adc_enable(bool enable)
{
    /* AXP192 ADC is always on, nothing to do */
    (void)enable;
}

/* Export operations struct */
static const struct telemetry_ops tbeam_telemetry_ops = {
    .init = tbeam_init,
    .read_battery_mv = tbeam_read_battery_mv,
    .read_solar_mv = tbeam_read_solar_mv,
    .is_usb_power = tbeam_is_usb_power,
    .is_charging = tbeam_is_charging,
    .read_temp = tbeam_read_temp,
    .adc_enable = tbeam_adc_enable,
};

/* Board selector will point to this for T-Beam */
#if defined(BOARD_LILYGO_TBEAM) || defined(BOARD_LILYGO_TBEAM_SUPREME)
const struct telemetry_ops *board_telemetry_ops = &tbeam_telemetry_ops;
#endif
