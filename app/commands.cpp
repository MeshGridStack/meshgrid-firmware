/**
 * Serial command handling
 */

#include "commands.h"
#include "neighbors.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>

#include "lib/board.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/telemetry/telemetry.h"
#include "drivers/test/hw_test.h"
#include "version.h"
}

/* Externs from main.cpp */
extern struct meshgrid_state mesh;
extern enum meshgrid_device_mode device_mode;
extern struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
} radio_config;

extern const struct board_config *board;
extern class SX1262 *radio;
extern struct telemetry_data telemetry;
extern Preferences prefs;

/* RTC time tracking */
struct rtc_time_t {
    bool valid;
    uint32_t epoch_at_boot;
};
extern struct rtc_time_t rtc_time;

#define LOG_BUFFER_SIZE 50
extern String log_buffer[LOG_BUFFER_SIZE];
extern int log_index, log_count;
extern bool log_enabled, monitor_mode, display_dirty;
extern uint32_t stat_duplicates;
extern uint32_t stat_clients, stat_repeaters, stat_rooms;

extern void config_save(void);
extern void send_advertisement(uint8_t route);
extern void send_text_message(const char *text);
extern void send_group_message(const char *text);
extern uint32_t get_uptime_secs(void);

static char serial_cmd_buf[256];
static int serial_cmd_len = 0;

void handle_cli_command(const String &cmd) {
    if (cmd == "INFO") {
        Serial.print("{\"name\":\"");
        Serial.print(mesh.name);
        Serial.print("\",\"node_hash\":");
        Serial.print(mesh.our_hash);
        Serial.print(",\"public_key\":[");
        for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
            if (i > 0) Serial.print(",");
            Serial.print(mesh.pubkey[i]);
        }
        Serial.print("],\"firmware_version\":\"");
        Serial.print(MESHGRID_VERSION);
        Serial.print("\",\"mode\":\"");
        Serial.print(device_mode == MODE_REPEATER ? "REPEATER" :
                     device_mode == MODE_ROOM ? "ROOM" : "CLIENT");
        Serial.print("\",\"freq_mhz\":");
        Serial.print(radio_config.frequency, 2);  /* Use actual radio config, not board defaults */
        Serial.print(",\"tx_power_dbm\":");
        Serial.print(radio_config.tx_power);  /* Use actual radio config, not board defaults */
        Serial.println("}");
    } else if (cmd == "NEIGHBORS") {
        Serial.print("[");
        for (int i = 0; i < neighbor_count; i++) {
            if (i > 0) Serial.print(",");
            Serial.print("{\"node_hash\":");
            Serial.print(neighbors[i].hash);
            Serial.print(",\"name\":\"");
            Serial.print(neighbors[i].name);
            Serial.print("\",\"public_key\":[");
            for (int j = 0; j < MESHGRID_PUBKEY_SIZE; j++) {
                if (j > 0) Serial.print(",");
                Serial.print(neighbors[i].pubkey[j]);
            }
            Serial.print("],\"rssi\":");
            Serial.print(neighbors[i].rssi);
            Serial.print(",\"snr\":");
            Serial.print(neighbors[i].snr);
            Serial.print(",\"last_seen_secs\":");
            Serial.print((millis() - neighbors[i].last_seen) / 1000);
            Serial.print("}");
        }
        Serial.println("]");
    } else if (cmd == "TELEMETRY") {
        Serial.print("{\"device\":{\"battery\":");
        Serial.print(telemetry.battery_pct);
        Serial.print(",\"voltage\":");
        Serial.print(telemetry.battery_mv / 1000.0, 3);
        Serial.print(",\"charging\":");
        Serial.print(telemetry.is_charging ? "true" : "false");
        Serial.print(",\"usb\":");
        Serial.print(telemetry.is_usb_power ? "true" : "false");
        Serial.print(",\"uptime\":");
        Serial.print(get_uptime_secs());
        Serial.print(",\"heap\":");
        Serial.print(telemetry.free_heap);
        if (telemetry.has_temp) {
            Serial.print(",\"cpu_temp\":");
            Serial.print(telemetry.temp_deci_c / 10.0, 1);
        }
        Serial.println("}}");
    } else if (cmd == "STATS") {
        /* Comprehensive performance statistics for CLI - single line JSON */
        Serial.print("{");
        Serial.print("\"hardware\":{");
        Serial.print("\"board\":\""); Serial.print(board->vendor); Serial.print(" "); Serial.print(board->name); Serial.print("\",");
        Serial.print("\"chip\":\"ESP32-S3\",");
        Serial.print("\"cpu_mhz\":240,");
        Serial.print("\"cores\":2");
        Serial.print("},");
        Serial.print("\"memory\":{");
        Serial.print("\"ram_total_kb\":320,");
        Serial.print("\"ram_used_kb\":32,");
        Serial.print("\"ram_free_kb\":288,");
        Serial.print("\"heap_free_kb\":"); Serial.print(telemetry.free_heap / 1024); Serial.print(",");
        Serial.print("\"flash_total_kb\":3264,");
        Serial.print("\"flash_used_kb\":481");
        Serial.print("},");
        Serial.print("\"packets\":{");
        Serial.print("\"rx\":"); Serial.print(mesh.packets_rx); Serial.print(",");
        Serial.print("\"tx\":"); Serial.print(mesh.packets_tx); Serial.print(",");
        Serial.print("\"fwd\":"); Serial.print(mesh.packets_fwd); Serial.print(",");
        Serial.print("\"dropped\":"); Serial.print(mesh.packets_dropped); Serial.print(",");
        Serial.print("\"duplicates\":"); Serial.print(stat_duplicates);
        Serial.print("},");
        Serial.print("\"neighbors\":{");
        Serial.print("\"total\":"); Serial.print(neighbor_count); Serial.print(",");
        Serial.print("\"clients\":"); Serial.print(stat_clients); Serial.print(",");
        Serial.print("\"repeaters\":"); Serial.print(stat_repeaters); Serial.print(",");
        Serial.print("\"rooms\":"); Serial.print(stat_rooms);
        Serial.print("},");
        Serial.print("\"radio\":{");
        Serial.print("\"freq_mhz\":"); Serial.print(radio_config.frequency, 2); Serial.print(",");
        Serial.print("\"bandwidth_khz\":"); Serial.print(radio_config.bandwidth, 1); Serial.print(",");
        Serial.print("\"spreading_factor\":"); Serial.print(radio_config.spreading_factor); Serial.print(",");
        Serial.print("\"coding_rate\":"); Serial.print(radio_config.coding_rate); Serial.print(",");
        Serial.print("\"tx_power_dbm\":"); Serial.print(radio_config.tx_power);
        Serial.print("},");
        Serial.print("\"power\":{");
        Serial.print("\"battery_mv\":"); Serial.print(telemetry.battery_mv); Serial.print(",");
        Serial.print("\"battery_pct\":"); Serial.print(telemetry.battery_pct); Serial.print(",");
        Serial.print("\"charging\":"); Serial.print(telemetry.is_charging ? "true" : "false"); Serial.print(",");
        Serial.print("\"usb_power\":"); Serial.print(telemetry.is_usb_power ? "true" : "false"); Serial.print(",");
        Serial.print("\"sleep_enabled\":"); Serial.print(!telemetry.is_usb_power ? "true" : "false");
        Serial.print("},");
        Serial.print("\"features\":{");
        #if defined(ARDUINO_ARCH_ESP32)
            Serial.print("\"hw_aes\":true,");
            Serial.print("\"hw_sha256\":true,");
        #elif defined(ARDUINO_ARCH_NRF52)
            Serial.print("\"hw_aes\":true,");
            Serial.print("\"hw_sha256\":false,");
        #else
            Serial.print("\"hw_aes\":false,");
            Serial.print("\"hw_sha256\":false,");
        #endif
        Serial.print("\"tx_queue_size\":16,");
        Serial.print("\"priority_scheduling\":true,");
        Serial.print("\"airtime_budget\":true,");
        Serial.print("\"secret_caching\":true");
        Serial.print("},");
        Serial.print("\"firmware\":{");
        Serial.print("\"version\":\""); Serial.print(MESHGRID_VERSION); Serial.print("\",");
        Serial.print("\"build\":\""); Serial.print(MESHGRID_BUILD_DATE); Serial.print("\",");
        Serial.print("\"uptime_secs\":"); Serial.print(get_uptime_secs()); Serial.print(",");
        Serial.print("\"mode\":\""); Serial.print(device_mode == MODE_REPEATER ? "REPEATER" :
                                                   device_mode == MODE_ROOM ? "ROOM" : "CLIENT"); Serial.print("\"");
        Serial.print("},");
        Serial.print("\"temperature\":{");
        if (telemetry.has_temp) {
            Serial.print("\"cpu_c\":"); Serial.print(telemetry.temp_deci_c / 10.0, 1);
        } else {
            Serial.print("\"cpu_c\":null");
        }
        Serial.print("}");
        Serial.println("}");
    } else if (cmd == "MONITOR") {
        monitor_mode = true;
        Serial.println("OK");
        /* Monitor mode enabled - will output ADV/MSG/ACK events */
    } else if (cmd == "LOG") {
        /* Show log buffer */
        int start = (log_count < LOG_BUFFER_SIZE) ? 0 : log_index;
        for (int i = 0; i < log_count; i++) {
            int idx = (start + i) % LOG_BUFFER_SIZE;
            Serial.println(log_buffer[idx]);
        }
        Serial.println("OK");
    } else if (cmd == "LOG ENABLE") {
        log_enabled = true;
        config_save();
        Serial.println("OK Logging enabled");
    } else if (cmd == "LOG DISABLE") {
        log_enabled = false;
        config_save();
        Serial.println("OK Logging disabled");
    } else if (cmd == "LOG CLEAR") {
        log_count = 0;
        log_index = 0;
        config_save();
        Serial.println("OK Log cleared");
    } else if (cmd == "CONFIG SAVE") {
        config_save();
        Serial.println("OK Config saved to flash");
    } else if (cmd == "CONFIG RESET") {
        prefs.begin("meshgrid", false);
        prefs.clear();
        prefs.end();
        Serial.println("OK Config cleared, rebooting...");
        delay(100);
        ESP.restart();
    } else if (cmd == "REBOOT") {
        Serial.println("OK Saving config and rebooting...");
        config_save();  /* Save everything before reboot */
        delay(100);
        ESP.restart();
    } else if (cmd == "ADVERT LOCAL") {
        send_advertisement(ROUTE_DIRECT);
        Serial.println("OK Local advertisement sent");
    } else if (cmd == "ADVERT FLOOD") {
        send_advertisement(ROUTE_FLOOD);
        Serial.println("OK Flood advertisement sent");
    } else if (cmd == "ADVERT") {
        /* Send both local and flood advertisements */
        send_advertisement(ROUTE_DIRECT);
        delay(100);  /* Small delay to ensure first packet finishes transmitting */
        send_advertisement(ROUTE_FLOOD);
        Serial.println("OK Both advertisements sent");
    } else if (cmd == "CONFIG") {
        Serial.print("{\"name\":\"");
        Serial.print(mesh.name);
        Serial.print("\",\"freq_mhz\":");
        Serial.print(radio_config.frequency, 2);
        Serial.print(",\"tx_power_dbm\":");
        Serial.print(radio_config.tx_power);
        Serial.print(",\"bandwidth_khz\":");
        Serial.print((int)(radio_config.bandwidth));
        Serial.print(",\"spreading_factor\":");
        Serial.print(radio_config.spreading_factor);
        Serial.print(",\"coding_rate\":");
        Serial.print(radio_config.coding_rate);
        Serial.print(",\"preamble_len\":");
        Serial.print(radio_config.preamble_len);
        Serial.println("}");
    } else if (cmd.startsWith("SET NAME ")) {
        String name = cmd.substring(9);
        name.trim();
        if (name.length() > 0 && name.length() <= 16) {
            strncpy(mesh.name, name.c_str(), 16);
            mesh.name[16] = '\0';
            config_save();
            Serial.println("OK");
        } else {
            Serial.println("ERR Name must be 1-16 characters");
        }
    } else if (cmd.startsWith("SET FREQ ")) {
        float freq = cmd.substring(9).toFloat();
        if (freq >= 137.0 && freq <= 1020.0) {
            radio->setFrequency(freq);
            radio_config.frequency = freq;
            config_save();
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid frequency");
        }
    } else if (cmd.startsWith("SET BW ")) {
        float bw = cmd.substring(7).toFloat();
        if (radio->setBandwidth(bw) == RADIOLIB_ERR_NONE) {
            radio_config.bandwidth = bw;
            config_save();
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid bandwidth");
        }
    } else if (cmd.startsWith("SET SF ")) {
        int sf = cmd.substring(7).toInt();
        if (sf >= 6 && sf <= 12) {
            if (radio->setSpreadingFactor(sf) == RADIOLIB_ERR_NONE) {
                radio_config.spreading_factor = sf;
                config_save();
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set SF");
            }
        } else {
            Serial.println("ERR SF must be 6-12");
        }
    } else if (cmd.startsWith("SET CR ")) {
        int cr = cmd.substring(7).toInt();
        if (cr >= 5 && cr <= 8) {
            if (radio->setCodingRate(cr) == RADIOLIB_ERR_NONE) {
                radio_config.coding_rate = cr;
                config_save();
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set CR");
            }
        } else {
            Serial.println("ERR CR must be 5-8");
        }
    } else if (cmd.startsWith("SET POWER ")) {
        int power = cmd.substring(10).toInt();
        if (power >= -9 && power <= 22) {
            if (radio->setOutputPower(power) == RADIOLIB_ERR_NONE) {
                radio_config.tx_power = power;
                config_save();
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set power");
            }
        } else {
            Serial.println("ERR Power must be -9 to 22 dBm");
        }
    } else if (cmd.startsWith("SET PREAMBLE ")) {
        int preamble = cmd.substring(13).toInt();
        if (preamble >= 6 && preamble <= 65535) {
            if (radio->setPreambleLength(preamble) == RADIOLIB_ERR_NONE) {
                radio_config.preamble_len = preamble;
                config_save();
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set preamble");
            }
        } else {
            Serial.println("ERR Preamble must be 6-65535");
        }
    } else if (cmd == "SET PRESET EU_NARROW" || cmd == "SET PRESET EU") {
        /* EU/UK (Narrow) - 869.618 MHz, 62.5 kHz BW, SF8, CR8, Preamble 16 */
        radio->setFrequency(869.618);
        radio->setBandwidth(62.5);
        radio->setSpreadingFactor(8);
        radio->setCodingRate(8);
        radio->setPreambleLength(16);  /* MeshCore uses 16 */
        radio_config.frequency = 869.618;
        radio_config.bandwidth = 62.5;
        radio_config.spreading_factor = 8;
        radio_config.coding_rate = 8;
        radio_config.preamble_len = 16;
        config_save();
        Serial.println("OK EU/UK Narrow: 869.618MHz 62.5kHz SF8 CR8 Pre16");
    } else if (cmd == "SET PRESET US_STANDARD" || cmd == "SET PRESET US") {
        /* US Standard - 915 MHz, 250 kHz BW, SF10, CR7 */
        radio->setFrequency(915.0);
        radio->setBandwidth(250.0);
        radio->setSpreadingFactor(10);
        radio->setCodingRate(7);
        radio->setPreambleLength(16);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 250.0;
        radio_config.spreading_factor = 10;
        radio_config.coding_rate = 7;
        radio_config.preamble_len = 16;
        config_save();
        Serial.println("OK US Standard: 915MHz 250kHz SF10 CR7");
    } else if (cmd == "SET PRESET US_FAST") {
        /* US Fast - 915 MHz, 500 kHz BW, SF7, CR5 */
        radio->setFrequency(915.0);
        radio->setBandwidth(500.0);
        radio->setSpreadingFactor(7);
        radio->setCodingRate(5);
        radio->setPreambleLength(8);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 500.0;
        radio_config.spreading_factor = 7;
        radio_config.coding_rate = 5;
        radio_config.preamble_len = 8;
        config_save();
        Serial.println("OK US Fast: 915MHz 500kHz SF7 CR5");
    } else if (cmd == "SET PRESET LONG_RANGE") {
        /* Long range - SF12, 125 kHz BW */
        radio->setBandwidth(125.0);
        radio->setSpreadingFactor(12);
        radio->setCodingRate(8);
        radio->setPreambleLength(16);
        radio_config.bandwidth = 125.0;
        radio_config.spreading_factor = 12;
        radio_config.coding_rate = 8;
        radio_config.preamble_len = 16;
        config_save();
        Serial.println("OK Long Range: 125kHz SF12 CR8");
    } else if (cmd.startsWith("SEND GROUP ")) {
        /* SEND GROUP <message> - send encrypted group message to public channel */
        String message = cmd.substring(11);
        message.trim();
        if (message.length() > 0 && message.length() <= 200) {
            send_group_message(message.c_str());
            Serial.println("OK");
        } else {
            Serial.println("ERR Message too long or empty");
        }
    } else if (cmd.startsWith("SEND ")) {
        /* SEND <message> - broadcast text message */
        /* SEND <dest> <message> - direct message to specific node */
        String rest = cmd.substring(5);
        rest.trim();

        /* Check if this is a direct message (has dest parameter) */
        int spaceIdx = rest.indexOf(' ');
        if (spaceIdx > 0 && spaceIdx < rest.length() - 1) {
            /* Looks like: SEND <dest> <message> */
            String dest = rest.substring(0, spaceIdx);
            String message = rest.substring(spaceIdx + 1);
            message.trim();

            /* For now, send as broadcast with destination prefix */
            /* Format: "TO:<dest>:<message>" so receiver can filter */
            String directMsg = "TO:" + dest + ":" + message;
            if (directMsg.length() <= 200) {
                send_text_message(directMsg.c_str());
                Serial.println("OK");
            } else {
                Serial.println("ERR Message too long");
            }
        } else {
            /* Broadcast message */
            if (rest.length() > 0 && rest.length() <= 200) {
                send_text_message(rest.c_str());
                Serial.println("OK");
            } else {
                Serial.println("ERR Message too long or empty");
            }
        }
    } else if (cmd.startsWith("TRACE ")) {
        /* TRACE <target> - send trace packet to map route */
        String target = cmd.substring(6);
        target.trim();

        if (target.length() > 0) {
            /* For now, send a text message with TRACE prefix */
            /* Format: "TRACE:<target>" */
            String traceMsg = "TRACE:" + target;
            send_text_message(traceMsg.c_str());

            /* Basic response - full implementation would require protocol changes */
            Serial.print("{\"path\":[\"");
            Serial.print(mesh.name);
            Serial.print("\"],\"hop_count\":0,\"rtt_ms\":0,\"status\":\"sent\"}");
            Serial.println();
        } else {
            Serial.println("ERR Target required");
        }
    } else {
        Serial.print("ERR Unknown command: ");
        Serial.println(cmd);
    }
}


void handle_serial(void) {
    // Read characters one at a time (non-blocking like MeshCore)
    bool complete = false;
    while (Serial.available() && serial_cmd_len < sizeof(serial_cmd_buf) - 1) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            // Complete line received
            serial_cmd_buf[serial_cmd_len] = '\0';
            complete = true;
            break;
        }
        serial_cmd_buf[serial_cmd_len++] = c;
    }

    // Check if we have a complete command
    if (!complete) {
        return;  // Not a complete line yet
    }

    String cmd(serial_cmd_buf);
    serial_cmd_len = 0;  // Reset for next command
    cmd.trim();
    if (cmd.length() == 0) return;

    /* Echo for debugging */
    if (cmd == "PING") {
        Serial.println("PONG");
        return;
    }

    /* Check for CLI commands (uppercase commands without slash) */
    /* Route all uppercase commands to CLI handler (INFO, STATS, SET, etc.) */
    if (!cmd.startsWith("/") && isupper(cmd.charAt(0))) {
        handle_cli_command(cmd);
        return;
    }

    /* Human-readable commands */
    if (cmd == "/mode repeater" || cmd == "/mode rpt") {
        device_mode = MODE_REPEATER;
        config_save();
        Serial.println("Mode: REPEATER");
    } else if (cmd == "/mode room") {
        device_mode = MODE_ROOM;
        config_save();
        Serial.println("Mode: ROOM");
    } else if (cmd == "/mode client" || cmd == "/mode cli") {
        device_mode = MODE_CLIENT;
        config_save();
        Serial.println("Mode: CLIENT");
    } else if (cmd == "/test battery" || cmd == "/test bat") {
        Serial.println("Starting battery drain test (1 minute)...");
        struct hw_test_result result;
        hw_test_battery(&result, 60000, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "battery");
        Serial.println(buf);
    } else if (cmd == "/test solar") {
        Serial.println("Starting solar panel test...");
        struct hw_test_result result;
        hw_test_solar(&result, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "solar");
        Serial.println(buf);
    } else if (cmd == "/test radio") {
        Serial.println("Starting radio TX test...");
        struct hw_test_result result;
        hw_test_radio(&result, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "radio");
        Serial.println(buf);
    } else if (cmd.startsWith("/time ")) {
        /* Set RTC time: /time YYYY-MM-DD HH:MM:SS */
        String timestr = cmd.substring(6);
        int year, month, day, hour, minute, second;
        if (sscanf(timestr.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
            /* Convert to Unix epoch (simplified, from year 2000) */
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

            Serial.println("OK Time set");
        } else {
            Serial.println("ERR Invalid time format. Usage: /time YYYY-MM-DD HH:MM:SS");
        }
    } else {
        Serial.print("Unknown: ");
        Serial.println(cmd);
    }

    display_dirty = true;
}
