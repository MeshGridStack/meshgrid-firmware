/**
 * Serial command handling (refactored)
 * Routes commands to specialized handlers
 */

#include "commands.h"
#include "commands/common.h"
#include "commands/info_commands.h"
#include "commands/message_commands.h"
#include "commands/channel_commands.h"
#include "commands/config_commands.h"
#include "commands/network_commands.h"
#include "commands/system_commands.h"
#include "security.h"
#include "hardware/bluetooth/serial_bridge.h"
#include "utils/cobs.h"
#include "utils/debug.h"
#include <Arduino.h>

/* COBS protocol buffers */
static uint8_t cobs_rx_buf[512];
static size_t cobs_rx_len = 0;
static uint8_t cobs_decode_buf[256];

/* Forward declarations */
static void process_command(const String& cmd);

void serial_commands_init(void) {
    /* Clear any garbage from serial buffer on boot */
    cobs_rx_len = 0;
    while (serial_bridge_available()) {
        serial_bridge_read();
    }
}

void handle_serial(void) {
    /* COBS protocol only - zero-byte delimited frames */
    while (serial_bridge_available()) {
        uint8_t c = serial_bridge_read();

        if (c == 0) {
            /* COBS packet boundary - decode and process */
            if (cobs_rx_len > 0) {
                DEBUG_INFOF("COBS RX len=%d bytes", cobs_rx_len);

                size_t decoded_len = cobs_decode(cobs_decode_buf, cobs_rx_buf, cobs_rx_len);
                DEBUG_INFOF("COBS decoded len=%d", decoded_len);

                if (decoded_len > 0 && decoded_len < sizeof(cobs_decode_buf)) {
                    cobs_decode_buf[decoded_len] = '\0';
                    String cmd((char*)cobs_decode_buf);
                    cmd.trim();
                    if (cmd.length() > 0) {
                        process_command(cmd);
                    }
                }
            }
            cobs_rx_len = 0;
            /* Continue processing more frames if available */
        } else {
            /* Accumulate data for COBS frame */
            if (cobs_rx_len < sizeof(cobs_rx_buf)) {
                cobs_rx_buf[cobs_rx_len++] = c;
            } else {
                /* Buffer overflow - discard this frame */
                cobs_rx_len = 0;
            }
        }
    }
}

/* Process a decoded command */
static void process_command(const String& cmd) {
    if (cmd.length() == 0)
        return;

    /* PING command (always allowed) */
    if (cmd == "PING") {
        response_println("PONG");
        return;
    }

    /* AUTH STATUS command (check first, before AUTH <password>) */
    if (cmd == "AUTH STATUS") {
        char buf[64];
        snprintf(buf, sizeof(buf), "OK Serial: %s | BLE PIN: %s", security.serial_auth_enabled ? "ON" : "OFF",
                 security.pin);
        response_println(buf);
        return;
    }

    /* AUTH ENABLE command (always allowed to enable protection) */
    if (cmd == "AUTH ENABLE") {
        security_enable_serial_auth();
        response_println("OK Serial auth enabled");
        return;
    }

    /* AUTH DISABLE command (requires authentication - check before AUTH <password>) */
    if (cmd == "AUTH DISABLE") {
        if (!security_check_auth()) {
            response_println("ERR Not authenticated. Send: AUTH <password>");
            return;
        }
        security_disable_serial_auth();
        response_println("OK Serial auth disabled");
        return;
    }

    /* AUTH <password> command (authenticate) */
    if (cmd.startsWith("AUTH ")) {
        String password = cmd.substring(5);
        password.trim();
        if (security_authenticate(password.c_str())) {
            response_println("OK Authenticated");
        } else {
            response_println("ERR Invalid password");
        }
        return;
    }

    /* SETPASS command (always allowed for initial setup) */
    if (cmd.startsWith("SETPASS ")) {
        String new_password = cmd.substring(8);
        new_password.trim();
        if (security_set_serial_password(new_password.c_str())) {
            response_println("OK Password set");
        } else {
            response_println("ERR Password must be 4-32 characters");
        }
        return;
    }

    /* SETPIN command (set BLE PIN) */
    if (cmd.startsWith("SETPIN ")) {
        String new_pin = cmd.substring(7);
        new_pin.trim();
        if (security_set_pin(new_pin.c_str())) {
            response_println("OK BLE PIN set");
        } else {
            response_println("ERR PIN must be 6 digits");
        }
        return;
    }

    /* Check authentication before processing commands */
    if (!security_check_auth()) {
        if (security_is_locked()) {
            uint32_t remaining = (security.lockout_until - millis()) / 1000;
            String resp = "ERR Device locked for " + String(remaining) + " seconds";
            response_println(resp);
        } else {
            response_println("ERR Not authenticated. Send: AUTH <password>");
        }
        return;
    }

    /* === INFO COMMANDS === */
    if (cmd == "INFO") {
        cmd_info();
    } else if (cmd == "TELEMETRY") {
        cmd_telemetry();
    } else if (cmd == "STATS") {
        cmd_stats();
    } else if (cmd == "TIME") {
        cmd_time_show();
    } else if (cmd == "CONFIG") {
        cmd_config();

        /* === NETWORK COMMANDS === */
    } else if (cmd == "NEIGHBORS") {
        cmd_neighbors();
    } else if (cmd.startsWith("ADVERT")) {
        /* Parse ADVERT [LOCAL|FLOOD] */
        if (cmd == "ADVERT LOCAL") {
            cmd_advert_local();
        } else if (cmd == "ADVERT FLOOD") {
            cmd_advert_flood();
        } else if (cmd == "ADVERT") {
            cmd_advert(); /* Default: send flood */
        } else {
            response_println("ERR Unknown ADVERT command");
        }

        /* === MESSAGE COMMANDS === */
    } else if (cmd == "MESSAGES" || cmd == "INBOX") {
        cmd_messages();
    } else if (cmd == "MESSAGES CLEAR") {
        cmd_messages_clear();

        /* === CHANNEL COMMANDS === */
    } else if (cmd == "CHANNELS") {
        cmd_channels();
    } else if (cmd.startsWith("CHANNEL JOIN ")) {
        cmd_channel_join(cmd.substring(13));
    } else if (cmd.startsWith("CHANNEL SEND ")) {
        cmd_channel_send(cmd.substring(13));

        /* === CONFIG COMMANDS === */
    } else if (cmd == "CONFIG SAVE") {
        cmd_config_save();
    } else if (cmd == "CONFIG RESET") {
        cmd_config_reset();
    } else if (cmd.startsWith("SET NAME ")) {
        String name = cmd.substring(9);
        name.trim();
        cmd_set_name(name);
    } else if (cmd.startsWith("SET FREQ ")) {
        float freq = cmd.substring(9).toFloat();
        cmd_set_freq(freq);
    } else if (cmd.startsWith("SET BW ")) {
        float bw = cmd.substring(7).toFloat();
        cmd_set_bw(bw);
    } else if (cmd.startsWith("SET SF ")) {
        int sf = cmd.substring(7).toInt();
        cmd_set_sf(sf);
    } else if (cmd.startsWith("SET CR ")) {
        int cr = cmd.substring(7).toInt();
        cmd_set_cr(cr);
    } else if (cmd.startsWith("SET POWER ")) {
        int power = cmd.substring(10).toInt();
        cmd_set_power(power);
    } else if (cmd.startsWith("SET PREAMBLE ")) {
        int preamble = cmd.substring(13).toInt();
        cmd_set_preamble(preamble);
    } else if (cmd == "SET PRESET EU_NARROW" || cmd == "SET PRESET EU") {
        cmd_set_preset("EU");
    } else if (cmd == "SET PRESET US_STANDARD" || cmd == "SET PRESET US") {
        cmd_set_preset("US");
    } else if (cmd == "SET PRESET US_FAST") {
        cmd_set_preset("US_FAST");
    } else if (cmd == "SET PRESET LONG_RANGE") {
        cmd_set_preset("LONG_RANGE");

        /* === SEND COMMANDS === */
    } else if (cmd.startsWith("SEND GROUP ")) {
        cmd_send_group(cmd.substring(11));
    } else if (cmd.startsWith("SEND ")) {
        cmd_send(cmd.substring(5));
    } else if (cmd.startsWith("TRACE ")) {
        cmd_trace(cmd.substring(6));

        /* === SYSTEM COMMANDS === */
    } else if (cmd == "REBOOT") {
        cmd_reboot();
    } else if (cmd == "IDENTITY ROTATE") {
        cmd_identity_rotate();

#ifdef ENABLE_BLE
    } else if (cmd == "BLE STATUS") {
        cmd_ble_status();
    } else if (cmd == "BLE") {
        cmd_ble();
#endif

        /* === SLASH COMMANDS === */
    } else if (cmd.startsWith("/mode ")) {
        cmd_mode(cmd.substring(6));
    } else if (cmd.startsWith("/test ")) {
        cmd_test(cmd.substring(6));
    } else if (cmd.startsWith("/time ")) {
        cmd_time(cmd.substring(6));
    } else if (cmd.startsWith("/pin ")) {
        cmd_pin(cmd.substring(5));

    } else {
        response_print("ERR Unknown command: ");
        response_println(cmd);
    }
}
