/**
 * Security implementation
 */

#include "security.h"
#include "utils/debug.h"
#include <Preferences.h>

/* Include local config overrides if present */
#if __has_include("../config.local.h")
#    include "../config.local.h"
#endif

/* Default security setting (can be overridden in config.local.h) */
#ifndef DEFAULT_SECURITY_ENABLED
#    define DEFAULT_SECURITY_ENABLED false /* Disabled for testing/development */
#endif

extern Preferences prefs;

struct device_security security;

/**
 * Check if string is all digits
 */
static bool is_numeric(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return false;
        }
    }
    return true;
}

void security_init(void) {
    prefs.begin("security", true); // Read-only

    // Load PIN enabled state (default from config.local.h or true)
    security.pin_enabled = prefs.getBool("pin_enabled", DEFAULT_SECURITY_ENABLED);

    // Load or generate PIN
    String saved_pin = prefs.getString("pin", "");
    if (saved_pin.length() == 6) {
        strncpy(security.pin, saved_pin.c_str(), 7);
        DEBUG_INFOF("Security: PIN authentication %s", security.pin_enabled ? "enabled" : "disabled");
    } else {
        // Generate random 6-digit PIN
        uint32_t random_pin = esp_random() % 1000000;
        snprintf(security.pin, 7, "%06lu", random_pin);

        // Save to NVS
        prefs.end();
        prefs.begin("security", false); // Read-write
        prefs.putString("pin", security.pin);
        prefs.putBool("pin_enabled", DEFAULT_SECURITY_ENABLED);
        prefs.end();
        prefs.begin("security", true);

        // Update runtime state to match saved default
        security.pin_enabled = DEFAULT_SECURITY_ENABLED;

        DEBUG_INFO("Security: Generated new PIN");
        DEBUG_INFO("View PIN on OLED: Navigate to Security screen");
    }

    prefs.end();

    // Initialize session state
    security.authenticated = false;
    security.failed_attempts = 0;
    security.lockout_until = 0;
}

bool security_check_auth(void) {
    // If PIN is disabled, always authenticated
    if (!security.pin_enabled) {
        return true;
    }

    // Check lockout
    if (security_is_locked()) {
        return false;
    }

    return security.authenticated;
}

bool security_authenticate(const char* pin) {
    // Check lockout first
    if (security_is_locked()) {
        uint32_t remaining = (security.lockout_until - millis()) / 1000;
        DEBUG_ERRORF("Locked out for %lu more seconds", remaining);
        return false;
    }

    // Verify PIN
    if (strcmp(pin, security.pin) == 0) {
        security.authenticated = true;
        security.failed_attempts = 0;
        DEBUG_INFO("Authenticated");
        return true;
    } else {
        security.failed_attempts++;

        if (security.failed_attempts >= 3) {
            security.lockout_until = millis() + 300000; // 5 minutes
            DEBUG_ERROR("Too many failed attempts. Locked for 5 minutes.");
        } else {
            DEBUG_ERRORF("Invalid PIN (%d attempts remaining)", 3 - security.failed_attempts);
        }
        return false;
    }
}

bool security_is_locked(void) {
    if (security.lockout_until == 0) {
        return false;
    }

    if (millis() < security.lockout_until) {
        return true;
    }

    // Lockout expired
    security.lockout_until = 0;
    security.failed_attempts = 0;
    return false;
}

bool security_set_pin(const char* new_pin) {
    // Validate PIN format
    if (strlen(new_pin) != 6) {
        DEBUG_INFO("ERROR: PIN must be exactly 6 digits");
        return false;
    }

    if (!is_numeric(new_pin)) {
        DEBUG_INFO("ERROR: PIN must contain only digits");
        return false;
    }

    // Update PIN
    strncpy(security.pin, new_pin, 7);

    // Save to NVS
    prefs.begin("security", false);
    prefs.putString("pin", security.pin);
    prefs.end();

    DEBUG_INFO("OK: PIN changed successfully");
    return true;
}

void security_disable_pin(void) {
    security.pin_enabled = false;

    // Save to NVS
    prefs.begin("security", false);
    prefs.putBool("pin_enabled", false);
    prefs.end();

    DEBUG_INFO("WARNING: PIN authentication disabled - device is now unsecured!");
}

void security_enable_pin(void) {
    security.pin_enabled = true;

    // Save to NVS
    prefs.begin("security", false);
    prefs.putBool("pin_enabled", true);
    prefs.end();

    DEBUG_INFOF("PIN authentication enabled (PIN: %s)", security.pin);
}
