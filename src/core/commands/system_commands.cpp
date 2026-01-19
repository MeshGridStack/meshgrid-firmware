/**
 * System command implementations
 */

#include "system_commands.h"
#include "common.h"
#include "core/security.h"
#include "utils/constants.h"
#include "utils/types.h"
#include "utils/memory.h"
#include "utils/debug.h"
#include "ui/screens.h"
#include <Preferences.h>
#include <Esp.h>

#ifdef ENABLE_BLE
#include "hardware/bluetooth/ble_serial.h"
#endif

extern "C" {
#include "hardware/telemetry/telemetry.h"
#include "hardware/test/hw_test.h"
#include "network/protocol.h"
}

extern struct meshgrid_state mesh;
extern enum meshgrid_device_mode device_mode;
extern Preferences prefs;
extern struct rtc_time_t rtc_time;
extern struct display_state display_state;  /* Defined in utils/types.h */

extern struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
extern int public_msg_index, public_msg_count;
extern struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
extern int direct_msg_index, direct_msg_count;
extern struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[MAX_CUSTOM_CHANNELS];
extern int channel_msg_count[MAX_CUSTOM_CHANNELS];

extern void config_save(void);
extern void neighbors_save_to_nvs(void);
extern void channels_save_to_nvs(void);

void cmd_reboot() {
    response_println("OK Saving config and rebooting...");
    config_save();
    neighbors_save_to_nvs();
    channels_save_to_nvs();
    delay(100);
    ESP.restart();
}

void cmd_identity_rotate() {
    /* Clear messages */
    public_msg_count = 0;
    public_msg_index = 0;
    direct_msg_count = 0;
    direct_msg_index = 0;
    for (int i = 0; i < MAX_CUSTOM_CHANNELS; i++) {
        channel_msg_count[i] = 0;
        channel_msg_index[i] = 0;
    }

    /* Clear saved neighbors */
    prefs.begin("neighbors", false);
    prefs.clear();
    prefs.end();

    /* Clear identity from NVS */
    prefs.begin("meshgrid", false);
    prefs.putBool("has_identity", false);
    prefs.remove("pubkey");
    prefs.remove("privkey");
    prefs.end();

    response_println("OK Identity rotated - all encrypted data cleared, rebooting...");
    delay(100);
    ESP.restart();
}

#ifdef ENABLE_BLE
void cmd_ble_status() {
    if (ble_serial_connected()) {
        response_println("OK BLE connected");
    } else {
        response_println("OK BLE disconnected");
    }
}

void cmd_ble() {
    response_println("{\"ble_enabled\":true,\"connected\":" + String(ble_serial_connected() ? "true" : "false") + "}");
}
#endif

/* Slash commands */

void cmd_mode(const String &mode) {
    if (mode == "repeater" || mode == "rpt") {
        device_mode = MODE_REPEATER;
        config_save();
        response_println("Mode: REPEATER");
    } else if (mode == "client" || mode == "cli") {
        device_mode = MODE_CLIENT;
        config_save();
        response_println("Mode: CLIENT");
    } else {
        response_println("ERR Unknown mode");
    }
    display_state.dirty = true;
}

void cmd_test(const String &test_type) {
    if (test_type == "debug") {
        response_println("OK");
        DEBUG_ERROR("Test ERROR message");
        DEBUG_WARN("Test WARN message");
        DEBUG_INFO("Test INFO message");
        DEBUG_DEBUG("Test DEBUG message");
        DEBUG_INFOF("Test formatted message: %d + %d = %d", 1, 2, 3);
    } else if (test_type == "battery" || test_type == "bat") {
        response_println("Starting battery drain test (1 minute)...");
        struct hw_test_result result;
        hw_test_battery(&result, 60000, [](const char *status, uint8_t pct) {
            response_print("  ["); response_print(pct); response_print("%] ");
            response_println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "battery");
        response_println(buf);
    } else if (test_type == "solar") {
        response_println("Starting solar panel test...");
        struct hw_test_result result;
        hw_test_solar(&result, [](const char *status, uint8_t pct) {
            response_print("  ["); response_print(pct); response_print("%] ");
            response_println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "solar");
        response_println(buf);
    } else if (test_type == "radio") {
        response_println("Starting radio TX test...");
        struct hw_test_result result;
        hw_test_radio(&result, [](const char *status, uint8_t pct) {
            response_print("  ["); response_print(pct); response_print("%] ");
            response_println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "radio");
        response_println(buf);
    } else {
        response_println("ERR Unknown test type");
    }
}

void cmd_time(const String &timestr) {
    int year, month, day, hour, minute, second;
    if (sscanf(timestr.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        /* Convert to Unix epoch */
        uint32_t days = 0;
        for (int y = 2000; y < year; y++) {
            days += ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 366 : 365;
        }
        const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        for (int m = 0; m < month - 1; m++) {
            days += days_in_month[m];
            if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) days++;
        }
        days += day - 1;

        uint32_t epoch = days * 86400 + hour * 3600 + minute * 60 + second;
        rtc_time.epoch_at_boot = epoch - (millis() / 1000);
        rtc_time.valid = true;

        /* Save RTC time to NVS */
        prefs.begin("meshgrid", false);
        prefs.putBool("rtc_valid", true);
        prefs.putUInt("rtc_epoch", rtc_time.epoch_at_boot);
        prefs.end();

        response_println("OK Time set");
    } else {
        response_println("ERR Invalid time format. Usage: /time YYYY-MM-DD HH:MM:SS");
    }
}

void cmd_pin(const String &subcmd) {
    if (subcmd == "show") {
        response_print("PIN: ");
        response_print(security.pin);
        response_print("\nStatus: ");
        response_println(security.pin_enabled ? "ENABLED" : "DISABLED");
    } else if (subcmd.startsWith("set ")) {
        String new_pin = subcmd.substring(4);
        new_pin.trim();
        security_set_pin(new_pin.c_str());
    } else if (subcmd == "enable") {
        security_enable_pin();
    } else if (subcmd == "disable") {
        security_disable_pin();
    } else {
        response_println("ERR Unknown PIN command");
    }
    display_state.dirty = true;
}
