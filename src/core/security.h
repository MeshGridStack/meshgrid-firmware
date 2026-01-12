/**
 * Security - PIN authentication for Serial and BLE access
 */

#ifndef MESHGRID_SECURITY_H
#define MESHGRID_SECURITY_H

#include <Arduino.h>

/**
 * Security state
 */
struct device_security {
    char pin[7];              // 6 digits + null terminator
    bool pin_enabled;         // Enable/disable PIN requirement
    bool authenticated;       // Current session authenticated
    uint8_t failed_attempts;  // Failed auth attempts
    uint32_t lockout_until;   // Timestamp for lockout end
};

extern struct device_security security;

/**
 * Initialize security subsystem
 * - Loads or generates PIN
 * - Sets up authentication state
 */
void security_init(void);

/**
 * Check if command requires authentication
 * Returns true if authenticated or PIN disabled
 */
bool security_check_auth(void);

/**
 * Authenticate with PIN
 * Returns true if successful
 */
bool security_authenticate(const char *pin);

/**
 * Check if currently in lockout period
 */
bool security_is_locked(void);

/**
 * Set new PIN (requires authentication)
 */
bool security_set_pin(const char *new_pin);

/**
 * Disable PIN requirement (requires authentication)
 */
void security_disable_pin(void);

/**
 * Enable PIN requirement
 */
void security_enable_pin(void);

#endif // MESHGRID_SECURITY_H
