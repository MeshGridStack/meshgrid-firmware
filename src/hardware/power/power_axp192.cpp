/**
 * AXP192 PMU Power Driver
 *
 * For LilyGo T-Beam with AXP192 power management IC.
 * Controls power rails for OLED, LoRa, and GPS.
 */

#include "power.h"
#include <Arduino.h>
#include <Wire.h>

/* AXP192 I2C address */
#define AXP192_ADDR              0x34

/* AXP192 registers */
#define AXP192_POWER_STATUS      0x00
#define AXP192_DCDC13_LDO23_CTRL 0x12  /* Power output control */
#define AXP192_DCDC1_VOLTAGE     0x26  /* DCDC1 voltage (OLED) */
#define AXP192_LDO23_VOLTAGE     0x28  /* LDO2/3 voltage (LoRa/GPS) */

/* Power rail bit masks in register 0x12 */
#define AXP192_BIT_DCDC3         (1 << 0)  /* Not used */
#define AXP192_BIT_LDO2          (1 << 1)  /* LoRa power */
#define AXP192_BIT_LDO3          (1 << 2)  /* GPS power */
#define AXP192_BIT_DCDC2         (1 << 3)  /* Not used */
#define AXP192_BIT_EXTEN         (1 << 4)  /* External enable */
#define AXP192_BIT_DCDC1         (1 << 6)  /* OLED power */

static void axp192_write_reg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t axp192_read_reg(uint8_t reg) {
    Wire.beginTransmission(AXP192_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);  /* Repeated start */
    Wire.requestFrom((uint8_t)AXP192_ADDR, (uint8_t)1);
    return Wire.read();
}

static int axp192_init(void) {
    /* Probe for AXP192 */
    Wire.beginTransmission(AXP192_ADDR);
    if (Wire.endTransmission() != 0) {
        return -1;  /* AXP192 not found */
    }

    /* Configure DCDC1 (OLED) to 3.3V */
    axp192_write_reg(AXP192_DCDC1_VOLTAGE, 0xFF);  /* Max voltage */

    /* Configure LDO2 (LoRa) and LDO3 (GPS) to 3.3V
     * LDO2: bits [7:4], LDO3: bits [3:0]
     * Value 15 (0xF) = 3.3V for both */
    axp192_write_reg(AXP192_LDO23_VOLTAGE, 0xFF);

    /* Enable all power rails: DCDC1 + LDO2 + LDO3 */
    uint8_t power_ctrl = AXP192_BIT_DCDC1 | AXP192_BIT_LDO2 | AXP192_BIT_LDO3;
    axp192_write_reg(AXP192_DCDC13_LDO23_CTRL, power_ctrl);

    delay(200);  /* Give power rails time to stabilize */

    return 0;
}

static int axp192_enable_rail(enum power_rail rail, bool enable) {
    uint8_t power_ctrl = axp192_read_reg(AXP192_DCDC13_LDO23_CTRL);

    switch (rail) {
        case POWER_RAIL_DISPLAY:
            if (enable) {
                power_ctrl |= AXP192_BIT_DCDC1;
            } else {
                power_ctrl &= ~AXP192_BIT_DCDC1;
            }
            break;

        case POWER_RAIL_RADIO:
            if (enable) {
                power_ctrl |= AXP192_BIT_LDO2;
            } else {
                power_ctrl &= ~AXP192_BIT_LDO2;
            }
            break;

        case POWER_RAIL_GPS:
            if (enable) {
                power_ctrl |= AXP192_BIT_LDO3;
            } else {
                power_ctrl &= ~AXP192_BIT_LDO3;
            }
            break;

        default:
            return -1;
    }

    axp192_write_reg(AXP192_DCDC13_LDO23_CTRL, power_ctrl);
    delay(50);  /* Allow rail to stabilize */

    return 0;
}

/* AXP192 power operations export */
const struct power_ops axp192_power_ops = {
    .name = "AXP192",
    .init = axp192_init,
    .enable_rail = axp192_enable_rail,
    .radio_tx_begin = NULL,
    .radio_tx_end = NULL,
};
