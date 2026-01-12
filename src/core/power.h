/**
 * Power management
 */

#ifndef MESHGRID_APP_POWER_H
#define MESHGRID_APP_POWER_H

#include <stdint.h>

/**
 * Check power state and enter low-power mode if appropriate
 * Called periodically from main loop
 */
void power_check_sleep(void);

/**
 * Mark activity to prevent sleep
 * Call this when user interaction or important events occur
 */
void power_mark_activity(void);

#endif // MESHGRID_APP_POWER_H
