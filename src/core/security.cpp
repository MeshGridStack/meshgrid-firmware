/**
 * Security implementation
 */

#include "security.h"
#include "utils/debug.h"
#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)
#    include <Preferences.h>
#endif

/* Include local config overrides if present */
#if __has_include("../config.local.h")
#    include "../config.local.h"
#endif

/* Default security setting (can be overridden in config.local.h) */
#ifndef DEFAULT_SECURITY_ENABLED
#    define DEFAULT_SECURITY_ENABLED false /* Disabled for testing/development */
#endif

#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)
extern Preferences prefs;
#endif

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
    prefs.begin("security", false); // Read-write

    // Load serial auth state
    security.serial_auth_enabled = prefs.getBool("serial_auth_en", DEFAULT_SECURITY_ENABLED);

    // Load or generate BLE PIN
    String saved_pin = prefs.getString("ble_pin", "");
    if (saved_pin.length() == 6) {
        strncpy(security.pin, saved_pin.c_str(), 7);
    } else {
        // Generate random 6-digit BLE PIN
        uint32_t random_pin = esp_random() % 1000000;
        snprintf(security.pin, 7, "%06lu", random_pin);
        prefs.putString("ble_pin", security.pin);
        DEBUG_INFO("Security: Generated new BLE PIN");
    }

    // Load serial password
    String saved_password = prefs.getString("serial_pass", "");
    if (saved_password.length() > 0) {
        strncpy(security.serial_password, saved_password.c_str(), 33);
        DEBUG_INFOF("Serial auth %s", security.serial_auth_enabled ? "ON" : "OFF");
    } else {
        security.serial_password[0] = '\0';
        DEBUG_INFO("Serial password not set - use SETPASS <password>");
    }

    prefs.end();

    // Initialize session state
    security.authenticated = false;
    security.failed_attempts = 0;
    security.lockout_until = 0;
    security.last_activity_ms = 0;

    DEBUG_INFOF("Security initialized: BLE PIN=%s", security.pin);
}

bool security_check_auth(void) {
    // If serial auth is disabled, always authenticated
    if (!security.serial_auth_enabled) {
        return true;
    }

    // Check lockout
    if (security_is_locked()) {
        return false;
    }

    // Check session timeout (2 hours)
    if (security.authenticated && security.last_activity_ms > 0) {
        if (millis() - security.last_activity_ms > 7200000) { // 2 hours
            security.authenticated = false;
            DEBUG_INFO("Session expired after 2 hours");
        }
    }

    return security.authenticated;
}

bool security_authenticate(const char* password) {
    // Check lockout first
    if (security_is_locked()) {
        uint32_t remaining = (security.lockout_until - millis()) / 1000;
        DEBUG_ERRORF("Locked out for %lu more seconds", remaining);
        return false;
    }

    // Check if password is set
    if (security.serial_password[0] == '\0') {
        DEBUG_ERROR("No password set. Use SETPASS <password> first");
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    bool valid = true;
    size_t input_len = strlen(password);
    size_t stored_len = strlen(security.serial_password);

    if (input_len != stored_len) {
        valid = false;
    }

    uint8_t diff = 0;
    size_t max_len = (stored_len > 32) ? 32 : stored_len;
    for (size_t i = 0; i < max_len; i++) {
        diff |= password[i] ^ security.serial_password[i];
    }
    if (diff != 0) {
        valid = false;
    }

    if (valid) {
        security.authenticated = true;
        security.failed_attempts = 0;
        security.last_activity_ms = millis();
        DEBUG_INFO("Authenticated");
        return true;
    } else {
        security.failed_attempts++;

        if (security.failed_attempts >= 3) {
            security.lockout_until = millis() + 300000; // 5 minutes
            DEBUG_ERROR("Too many failed attempts. Locked for 5 minutes.");
        } else {
            DEBUG_ERRORF("Invalid password (%d attempts remaining)", 3 - security.failed_attempts);
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
    prefs.putString("ble_pin", security.pin);
    prefs.end();

    DEBUG_INFO("OK: BLE PIN changed successfully");
    return true;
}

bool security_set_serial_password(const char* new_password) {
    // Validate password (4-32 characters)
    size_t len = strlen(new_password);
    if (len < 4 || len > 32) {
        DEBUG_INFO("ERROR: Password must be 4-32 characters");
        return false;
    }

    // Update password
    strncpy(security.serial_password, new_password, 33);

    // Save to NVS
    prefs.begin("security", false);
    prefs.putString("serial_pass", security.serial_password);
    prefs.end();

    DEBUG_INFO("OK: Serial password set successfully");
    return true;
}

void security_disable_serial_auth(void) {
    security.serial_auth_enabled = false;

    // Save to NVS
    prefs.begin("security", false);
    prefs.putBool("serial_auth_en", false);
    prefs.end();

    DEBUG_INFO("WARNING: Serial authentication disabled");
}

void security_enable_serial_auth(void) {
    // Check if password is set first
    if (security.serial_password[0] == '\0') {
        DEBUG_ERROR("Cannot enable auth - no password set. Use SETPASS <password> first");
        return;
    }

    security.serial_auth_enabled = true;

    // Save to NVS
    prefs.begin("security", false);
    prefs.putBool("serial_auth_en", true);
    prefs.end();

    DEBUG_INFO("Serial authentication enabled");
}
