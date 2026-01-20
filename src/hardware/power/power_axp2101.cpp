/**
 * AXP2101 PMU Power Driver
 *
 * For LilyGo T-Beam v1.2 with AXP2101 power management IC.
 * Uses XPowersLib library.
 */

#include "power.h"
#include <Arduino.h>
#include <Wire.h>

#ifdef BOARD_LILYGO_TBEAM

#    define XPOWERS_CHIP_AXP2101
#    include <XPowersLib.h>

/* I2C pins for T-Beam */
#    define I2C_SDA 21
#    define I2C_SCL 22
#    define PMU_IRQ 35

static XPowersPMU power;
static bool axp2101_initialized = false;

static int axp2101_init(void) {
    /* Initialize I2C bus first - critical for preventing freezes */
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(50);

    /* Initialize AXP2101 */
    bool result = power.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);

    if (!result) {
        Serial.println("[AXP2101] PMU not found, trying AXP192...");
        return -1; /* AXP2101 not found - might be v1.0/1.1 with AXP192 */
    }

    Serial.printf("[AXP2101] Found chip ID: 0x%x\n", power.getChipID());

    /* Configure power limits */
    power.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
    power.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
    power.setSysPowerDownVoltage(2600);

    /* Enable DCDC outputs for LoRa and other peripherals */
    power.setDC3Voltage(3300); /* 3.3V for LoRa */
    power.enableDC3();

    /* Enable ALDO outputs */
    power.setALDO1Voltage(3300); /* 3.3V */
    power.enableALDO1();

    power.setALDO2Voltage(3300); /* 3.3V for GPS */
    power.enableALDO2();

    power.setALDO3Voltage(3300); /* 3.3V */
    power.enableALDO3();

    power.setALDO4Voltage(3300); /* 3.3V */
    power.enableALDO4();

    /* Enable BLDO outputs */
    power.setBLDO1Voltage(3300); /* 3.3V */
    power.enableBLDO1();

    /* Configure charging */
    power.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    power.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

    /* Enable charge LED */
    power.setChargingLedMode(XPOWERS_CHG_LED_ON);

    axp2101_initialized = true;
    delay(200); /* Give power rails time to stabilize */

    Serial.println("[AXP2101] Initialization complete");
    return 0;
}

static int axp2101_enable_rail(enum power_rail rail, bool enable) {
    if (!axp2101_initialized) {
        return -1;
    }

    switch (rail) {
        case POWER_RAIL_DISPLAY:
            /* OLED on ALDO4 */
            if (enable) {
                power.enableALDO4();
            } else {
                power.disableALDO4();
            }
            break;

        case POWER_RAIL_RADIO:
            /* LoRa on DC3 */
            if (enable) {
                power.enableDC3();
            } else {
                power.disableDC3();
            }
            break;

        case POWER_RAIL_GPS:
            /* GPS on ALDO2 */
            if (enable) {
                power.enableALDO2();
            } else {
                power.disableALDO2();
            }
            break;

        default:
            return -1;
    }

    delay(50); /* Allow rail to stabilize */
    return 0;
}

/* AXP2101 power operations export */
const struct power_ops axp2101_power_ops = {
    .name = "AXP2101",
    .init = axp2101_init,
    .enable_rail = axp2101_enable_rail,
    .radio_tx_begin = NULL,
    .radio_tx_end = NULL,
};

#endif /* BOARD_LILYGO_TBEAM */
