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
    char pin[7];                  // BLE pairing PIN (6 digits + null)
    char serial_password[33];     // Serial/USB password (up to 32 chars + null)
    bool serial_auth_enabled;     // Enable/disable serial password requirement
    bool authenticated;           // Current session authenticated
    uint8_t failed_attempts;      // Failed auth attempts
    uint32_t lockout_until;       // Timestamp for lockout end
    uint32_t last_activity_ms;    // Last authenticated command timestamp
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
bool security_authenticate(const char* pin);

/**
 * Check if currently in lockout period
 */
bool security_is_locked(void);

/**
 * Set new BLE PIN (requires authentication)
 */
bool security_set_pin(const char* new_pin);

/**
 * Set serial password (requires authentication or first-time setup)
 */
bool security_set_serial_password(const char* new_password);

/**
 * Disable serial authentication (requires authentication)
 */
void security_disable_serial_auth(void);

/**
 * Enable serial authentication
 */
void security_enable_serial_auth(void);

#endif // MESHGRID_SECURITY_H
