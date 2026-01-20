/**
 * Power Management HAL
 *
 * Hardware abstraction for power management across different board types:
 * - Simple GPIO control (VEXT, LED)
 * - PMU control (AXP192/AXP2101 on T-Beam)
 * - PA control (Heltec V4 power amplifier)
 */

#ifndef MESHGRID_POWER_H
#define MESHGRID_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Power rail identifiers
 */
enum power_rail {
    POWER_RAIL_DISPLAY = 0, /* OLED/LCD/E-Ink display */
    POWER_RAIL_RADIO,       /* LoRa radio module */
    POWER_RAIL_GPS,         /* GPS module */
    POWER_RAIL_COUNT
};

/**
 * Power management operations structure
 *
 * Linux kernel-style ops for different power implementations.
 * Boards provide their own implementation by setting board->power_ops.
 */
struct power_ops {
    const char* name;

    /* Initialize power subsystem */
    int (*init)(void);

    /* Power rail control - enable/disable individual peripherals
     * Returns: 0 on success, -1 if not supported */
    int (*enable_rail)(enum power_rail rail, bool enable);

    /* Radio TX/RX hooks for PA (Power Amplifier) control
     * Called before/after radio transmission (optional) */
    void (*radio_tx_begin)(void);
    void (*radio_tx_end)(void);
};

/**
 * High-level power management API
 * These functions use board->power_ops if available, with safe fallbacks
 */

/* Initialize power subsystem for current board */
int power_init(void);

/* Enable/disable a specific power rail */
int power_enable_rail(enum power_rail rail, bool enable);

/* Radio transmission hooks - called by radio driver */
void power_radio_tx_begin(void);
void power_radio_tx_end(void);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_POWER_H */
