/**
 * meshgrid-firmware Global Configuration
 *
 * Centralized configuration constants for timing, thresholds, and defaults.
 */

#ifndef MESHGRID_CONSTANTS_H
#define MESHGRID_CONSTANTS_H

/* ========================================================================= */
/* Button Configuration                                                      */
/* ========================================================================= */

#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 1000

/* ========================================================================= */
/* Display Configuration                                                     */
/* ========================================================================= */

#define DISPLAY_REFRESH_INTERVAL_MS 1000

/* ========================================================================= */
/* Telemetry Configuration                                                   */
/* ========================================================================= */

#define TELEMETRY_READ_INTERVAL_MS 5000

/* ========================================================================= */
/* Radio Airtime Management                                                  */
/* ========================================================================= */

#define AIRTIME_WINDOW_MS 10000 /* 10 second rolling window */
#define AIRTIME_BUDGET_PCT 33   /* 33% duty cycle limit */

/* ========================================================================= */
/* Public Channel Configuration                                              */
/* ========================================================================= */

/* MeshCore-compatible public channel pre-shared key (Base64) */
#define PUBLIC_CHANNEL_PSK "izOH6cXN6mrJ5e26oRXNcg=="

#endif /* MESHGRID_CONSTANTS_H */
