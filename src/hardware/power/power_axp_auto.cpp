/**
 * AXP Auto-Detection Power Driver
 *
 * Detects AXP2101 (T-Beam v1.2) or AXP192 (T-Beam v1.0/1.1)
 * and uses the appropriate driver.
 */

#include "power.h"
#include <Arduino.h>

#ifdef BOARD_LILYGO_TBEAM

/* External power drivers */
extern const struct power_ops axp2101_power_ops;
extern const struct power_ops axp192_power_ops;

static const struct power_ops *active_driver = NULL;

static int axp_auto_init(void) {
    Serial.println("[AXP] Auto-detecting power chip...");

    /* Try AXP2101 first (T-Beam v1.2) */
    if (axp2101_power_ops.init() == 0) {
        Serial.println("[AXP] Detected AXP2101 (T-Beam v1.2)");
        active_driver = &axp2101_power_ops;
        return 0;
    }

    /* Fall back to AXP192 (T-Beam v1.0/1.1) */
    Serial.println("[AXP] Trying AXP192...");
    if (axp192_power_ops.init() == 0) {
        Serial.println("[AXP] Detected AXP192 (T-Beam v1.0/1.1)");
        active_driver = &axp192_power_ops;
        return 0;
    }

    Serial.println("[AXP] ERROR: No AXP chip detected!");
    return -1;
}

static int axp_auto_enable_rail(enum power_rail rail, bool enable) {
    if (active_driver && active_driver->enable_rail) {
        return active_driver->enable_rail(rail, enable);
    }
    return -1;
}

static void axp_auto_radio_tx_begin(void) {
    if (active_driver && active_driver->radio_tx_begin) {
        active_driver->radio_tx_begin();
    }
}

static void axp_auto_radio_tx_end(void) {
    if (active_driver && active_driver->radio_tx_end) {
        active_driver->radio_tx_end();
    }
}

/* AXP auto-detect power operations export */
const struct power_ops axp_auto_power_ops = {
    .name = "AXP-Auto",
    .init = axp_auto_init,
    .enable_rail = axp_auto_enable_rail,
    .radio_tx_begin = axp_auto_radio_tx_begin,
    .radio_tx_end = axp_auto_radio_tx_end,
};

#endif /* BOARD_LILYGO_TBEAM */
