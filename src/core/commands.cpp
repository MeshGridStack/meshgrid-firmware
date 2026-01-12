/**
 * Serial command handling
 */

#include "commands.h"
#include "neighbors.h"
#include "security.h"
#include "utils/memory.h"
#include "utils/constants.h"
#include "utils/types.h"
#include "utils/serial_output.h"
#include "ui/screens.h"
#include "radio/radio_hal.h"
#include "hardware/bluetooth/serial_bridge.h"
#ifdef ENABLE_BLE
#include "hardware/bluetooth/ble_serial.h"
#endif
#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "hardware/board.h"

extern "C" {
#include "network/protocol.h"
#include "hardware/telemetry/telemetry.h"
#include "hardware/test/hw_test.h"
#include "hardware/crypto/crypto.h"
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
extern struct telemetry_data telemetry;
extern Preferences prefs;

/* Externs from main.cpp - structs defined in lib/types.h */
extern struct rtc_time_t rtc_time;
extern String log_buffer[LOG_BUFFER_SIZE];
extern int log_index, log_count;
extern bool log_enabled, monitor_mode;
extern struct display_state display_state;
extern uint32_t stat_duplicates;
extern volatile uint32_t isr_trigger_count;
extern uint32_t stat_clients, stat_repeaters, stat_rooms;
extern bool radio_in_rx_mode;

extern struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
extern int public_msg_index, public_msg_count;

extern struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
extern int direct_msg_index, direct_msg_count;

extern struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[MAX_CUSTOM_CHANNELS];
extern int channel_msg_count[MAX_CUSTOM_CHANNELS];

extern uint8_t public_channel_hash;
extern struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
extern int custom_channel_count;

extern void config_save(void);
extern void neighbors_save_to_nvs(void);
extern void channels_save_to_nvs(void);
extern void send_advertisement(uint8_t route);
extern void send_text_message(uint8_t dest_hash, const char *text);
extern void send_group_message(const char *text);
extern void send_channel_message(uint8_t channel_hash, const uint8_t *channel_secret, const char *text, const char *channel_name);
extern uint32_t get_uptime_secs(void);

static char serial_cmd_buf[256];
static int serial_cmd_len = 0;

/* Escape string for JSON output (replace control chars with .) */
static void print_json_string(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c < 32 || c > 126) {
            SerialOutput.print('.');  // Replace control chars with .
        } else if (c == '"' || c == '\\') {
            SerialOutput.print('\\');
            SerialOutput.print(c);
        } else {
            SerialOutput.print(c);
        }
    }
}

void handle_cli_command(const String &cmd) {
    if (cmd == "INFO") {
        SerialOutput.print("{\"name\":\"");
        SerialOutput.print(mesh.name);
        SerialOutput.print("\",\"node_hash\":");
        SerialOutput.print(mesh.our_hash);
        SerialOutput.print(",\"public_key\":[");
        for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
            if (i > 0) SerialOutput.print(",");
            SerialOutput.print(mesh.pubkey[i]);
        }
        SerialOutput.print("],\"firmware_version\":\"");
        SerialOutput.print(MESHGRID_VERSION);
        SerialOutput.print("\",\"mode\":\"");
        SerialOutput.print(device_mode == MODE_REPEATER ? "REPEATER" : "CLIENT");
        SerialOutput.print("\",\"freq_mhz\":");
        SerialOutput.print(radio_config.frequency, 2);  /* Use actual radio config, not board defaults */
        SerialOutput.print(",\"tx_power_dbm\":");
        SerialOutput.print(radio_config.tx_power);  /* Use actual radio config, not board defaults */
        Serial.println("}");
    } else if (cmd == "NEIGHBORS") {
        SerialOutput.print("[");
        for (int i = 0; i < neighbor_count; i++) {
            if (i > 0) SerialOutput.print(",");
            SerialOutput.print("{\"node_hash\":");
            SerialOutput.print(neighbors[i].hash);
            SerialOutput.print(",\"name\":\"");
            SerialOutput.print(neighbors[i].name);
            SerialOutput.print("\",\"public_key\":[");
            for (int j = 0; j < MESHGRID_PUBKEY_SIZE; j++) {
                if (j > 0) SerialOutput.print(",");
                SerialOutput.print(neighbors[i].pubkey[j]);
            }
            SerialOutput.print("],\"rssi\":");
            SerialOutput.print(neighbors[i].rssi);
            SerialOutput.print(",\"snr\":");
            SerialOutput.print(neighbors[i].snr);
            SerialOutput.print(",\"last_seen_secs\":");
            SerialOutput.print((millis() - neighbors[i].last_seen) / 1000);
            SerialOutput.print(",\"firmware\":\"");
            switch (neighbors[i].firmware) {
                case FW_MESHGRID: SerialOutput.print("meshgrid"); break;
                case FW_MESHCORE: SerialOutput.print("meshcore"); break;
                case FW_MESHTASTIC: SerialOutput.print("meshtastic"); break;
                default: SerialOutput.print("other"); break;
            }
            SerialOutput.print("\"}");
        }
        Serial.println("]");
    } else if (cmd == "TELEMETRY") {
        SerialOutput.print("{\"device\":{\"battery\":");
        SerialOutput.print(telemetry.battery_pct);
        SerialOutput.print(",\"voltage\":");
        SerialOutput.print(telemetry.battery_mv / 1000.0, 3);
        SerialOutput.print(",\"charging\":");
        SerialOutput.print(telemetry.is_charging ? "true" : "false");
        SerialOutput.print(",\"usb\":");
        SerialOutput.print(telemetry.is_usb_power ? "true" : "false");
        SerialOutput.print(",\"uptime\":");
        SerialOutput.print(get_uptime_secs());
        SerialOutput.print(",\"heap\":");
        SerialOutput.print(telemetry.free_heap);
        if (telemetry.has_temp) {
            SerialOutput.print(",\"cpu_temp\":");
            SerialOutput.print(telemetry.temp_deci_c / 10.0, 1);
        }
        Serial.println("}}");
    } else if (cmd == "STATS") {
        /* Comprehensive performance statistics for CLI - single line JSON */
        SerialOutput.print("{");
        SerialOutput.print("\"hardware\":{");
        SerialOutput.print("\"board\":\""); SerialOutput.print(board->vendor); SerialOutput.print(" "); SerialOutput.print(board->name); SerialOutput.print("\",");
        SerialOutput.print("\"chip\":\"ESP32-S3\",");
        SerialOutput.print("\"cpu_mhz\":240,");
        SerialOutput.print("\"cores\":2");
        SerialOutput.print("},");
        SerialOutput.print("\"memory\":{");
        SerialOutput.print("\"ram_total_kb\":320,");
        SerialOutput.print("\"ram_used_kb\":32,");
        SerialOutput.print("\"ram_free_kb\":288,");
        SerialOutput.print("\"heap_free_kb\":"); SerialOutput.print(telemetry.free_heap / 1024); SerialOutput.print(",");
        SerialOutput.print("\"flash_total_kb\":3264,");
        SerialOutput.print("\"flash_used_kb\":481");
        SerialOutput.print("},");
        SerialOutput.print("\"packets\":{");
        SerialOutput.print("\"rx\":"); SerialOutput.print(mesh.packets_rx); SerialOutput.print(",");
        SerialOutput.print("\"tx\":"); SerialOutput.print(mesh.packets_tx); SerialOutput.print(",");
        SerialOutput.print("\"fwd\":"); SerialOutput.print(mesh.packets_fwd); SerialOutput.print(",");
        SerialOutput.print("\"dropped\":"); SerialOutput.print(mesh.packets_dropped); SerialOutput.print(",");
        SerialOutput.print("\"duplicates\":"); SerialOutput.print(stat_duplicates);
        SerialOutput.print("},");
        SerialOutput.print("\"neighbors\":{");
        SerialOutput.print("\"total\":"); SerialOutput.print(neighbor_count); SerialOutput.print(",");
        SerialOutput.print("\"clients\":"); SerialOutput.print(stat_clients); SerialOutput.print(",");
        SerialOutput.print("\"repeaters\":"); SerialOutput.print(stat_repeaters); SerialOutput.print(",");
        SerialOutput.print("\"rooms\":"); SerialOutput.print(stat_rooms);
        SerialOutput.print("},");
        SerialOutput.print("\"radio\":{");
        SerialOutput.print("\"freq_mhz\":"); SerialOutput.print(radio_config.frequency, 2); SerialOutput.print(",");
        SerialOutput.print("\"bandwidth_khz\":"); SerialOutput.print(radio_config.bandwidth, 1); SerialOutput.print(",");
        SerialOutput.print("\"spreading_factor\":"); SerialOutput.print(radio_config.spreading_factor); SerialOutput.print(",");
        SerialOutput.print("\"coding_rate\":"); SerialOutput.print(radio_config.coding_rate); SerialOutput.print(",");
        SerialOutput.print("\"tx_power_dbm\":"); SerialOutput.print(radio_config.tx_power); SerialOutput.print(",");
        SerialOutput.print("\"tcxo_voltage\":"); SerialOutput.print(board->lora_defaults.tcxo_voltage, 1); SerialOutput.print(",");
        SerialOutput.print("\"dio2_as_rf_switch\":"); SerialOutput.print(board->lora_defaults.dio2_as_rf_switch ? "true" : "false"); SerialOutput.print(",");
        SerialOutput.print("\"isr_count\":"); SerialOutput.print(isr_trigger_count);
        SerialOutput.print("},");        SerialOutput.print("\"power\":{");
        SerialOutput.print("\"battery_mv\":"); SerialOutput.print(telemetry.battery_mv); SerialOutput.print(",");
        SerialOutput.print("\"battery_pct\":"); SerialOutput.print(telemetry.battery_pct); SerialOutput.print(",");
        SerialOutput.print("\"charging\":"); SerialOutput.print(telemetry.is_charging ? "true" : "false"); SerialOutput.print(",");
        SerialOutput.print("\"usb_power\":"); SerialOutput.print(telemetry.is_usb_power ? "true" : "false"); SerialOutput.print(",");
        SerialOutput.print("\"sleep_enabled\":"); SerialOutput.print(!telemetry.is_usb_power ? "true" : "false");
        SerialOutput.print("},");
        SerialOutput.print("\"features\":{");
        #if defined(ARDUINO_ARCH_ESP32)
            SerialOutput.print("\"hw_aes\":true,");
            SerialOutput.print("\"hw_sha256\":true,");
        #elif defined(ARDUINO_ARCH_NRF52)
            SerialOutput.print("\"hw_aes\":true,");
            SerialOutput.print("\"hw_sha256\":false,");
        #else
            SerialOutput.print("\"hw_aes\":false,");
            SerialOutput.print("\"hw_sha256\":false,");
        #endif
        SerialOutput.print("\"tx_queue_size\":16,");
        SerialOutput.print("\"priority_scheduling\":true,");
        SerialOutput.print("\"airtime_budget\":true,");
        SerialOutput.print("\"secret_caching\":true");
        SerialOutput.print("},");
        SerialOutput.print("\"firmware\":{");
        SerialOutput.print("\"version\":\""); SerialOutput.print(MESHGRID_VERSION); SerialOutput.print("\",");
        SerialOutput.print("\"build\":\""); SerialOutput.print(MESHGRID_BUILD_DATE); SerialOutput.print("\",");
        SerialOutput.print("\"uptime_secs\":"); SerialOutput.print(get_uptime_secs()); SerialOutput.print(",");
        SerialOutput.print("\"mode\":\""); SerialOutput.print(device_mode == MODE_REPEATER ? "REPEATER" : "CLIENT"); SerialOutput.print("\"");
        SerialOutput.print("},");
        SerialOutput.print("\"temperature\":{");
        if (telemetry.has_temp) {
            SerialOutput.print("\"cpu_c\":"); SerialOutput.print(telemetry.temp_deci_c / 10.0, 1);
        } else {
            SerialOutput.print("\"cpu_c\":null");
        }
        SerialOutput.print("}");
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
    } else if (cmd == "MESSAGES" || cmd == "INBOX") {
        /* Show message inbox - aggregate from all buffers */
        SerialOutput.print("{\"messages\":[");

        int total_shown = 0;

        /* Show public channel messages */
        int start = (public_msg_count < PUBLIC_MESSAGE_BUFFER_SIZE) ? 0 : public_msg_index;
        for (int i = 0; i < public_msg_count; i++) {
            int idx = (start + i) % PUBLIC_MESSAGE_BUFFER_SIZE;
            struct message_entry *msg = &public_messages[idx];
            if (!msg->valid) continue;

            if (total_shown > 0) SerialOutput.print(",");
            SerialOutput.print("{");
            SerialOutput.print("\"from_hash\":\"0x"); SerialOutput.print(String(msg->sender_hash, HEX)); SerialOutput.print("\",");
            SerialOutput.print("\"from_name\":\""); print_json_string(msg->sender_name); SerialOutput.print("\",");
            SerialOutput.print("\"channel\":\"");
            if (msg->channel_hash == public_channel_hash) {
                SerialOutput.print("public");
            } else {
                SerialOutput.print("0x"); SerialOutput.print(String(msg->channel_hash, HEX));
            }
            SerialOutput.print("\",");
            SerialOutput.print("\"decrypted\":"); SerialOutput.print(msg->decrypted ? "true" : "false"); SerialOutput.print(",");
            SerialOutput.print("\"timestamp\":"); SerialOutput.print(msg->timestamp); SerialOutput.print(",");
            SerialOutput.print("\"text\":\""); print_json_string(msg->text); SerialOutput.print("\"");
            SerialOutput.print("}");
            total_shown++;
        }

        /* Show direct messages */
        start = (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) ? 0 : direct_msg_index;
        for (int i = 0; i < direct_msg_count; i++) {
            int idx = (start + i) % DIRECT_MESSAGE_BUFFER_SIZE;
            struct message_entry *msg = &direct_messages[idx];
            if (!msg->valid) continue;

            if (total_shown > 0) SerialOutput.print(",");
            SerialOutput.print("{");
            SerialOutput.print("\"from_hash\":\"0x"); SerialOutput.print(String(msg->sender_hash, HEX)); SerialOutput.print("\",");
            SerialOutput.print("\"from_name\":\""); print_json_string(msg->sender_name); SerialOutput.print("\",");
            SerialOutput.print("\"channel\":\"direct\",");
            SerialOutput.print("\"decrypted\":"); SerialOutput.print(msg->decrypted ? "true" : "false"); SerialOutput.print(",");
            SerialOutput.print("\"timestamp\":"); SerialOutput.print(msg->timestamp); SerialOutput.print(",");
            SerialOutput.print("\"text\":\""); print_json_string(msg->text); SerialOutput.print("\"");
            SerialOutput.print("}");
            total_shown++;
        }

        /* Show custom channel messages */
        for (int ch = 0; ch < custom_channel_count; ch++) {
            if (!custom_channels[ch].valid) continue;

            start = (channel_msg_count[ch] < CHANNEL_MESSAGE_BUFFER_SIZE) ? 0 : channel_msg_index[ch];
            for (int i = 0; i < channel_msg_count[ch]; i++) {
                int idx = (start + i) % CHANNEL_MESSAGE_BUFFER_SIZE;
                struct message_entry *msg = &channel_messages[ch][idx];
                if (!msg->valid) continue;

                if (total_shown > 0) SerialOutput.print(",");
                SerialOutput.print("{");
                SerialOutput.print("\"from_hash\":\"0x"); SerialOutput.print(String(msg->sender_hash, HEX)); SerialOutput.print("\",");
                SerialOutput.print("\"from_name\":\""); print_json_string(msg->sender_name); SerialOutput.print("\",");
                SerialOutput.print("\"channel\":\""); print_json_string(custom_channels[ch].name); SerialOutput.print("\",");
                SerialOutput.print("\"decrypted\":"); SerialOutput.print(msg->decrypted ? "true" : "false"); SerialOutput.print(",");
                SerialOutput.print("\"timestamp\":"); SerialOutput.print(msg->timestamp); SerialOutput.print(",");
                SerialOutput.print("\"text\":\""); print_json_string(msg->text); SerialOutput.print("\"");
                SerialOutput.print("}");
                total_shown++;
            }
        }

        SerialOutput.print("],\"total\":"); SerialOutput.print(total_shown);
        Serial.println("}");
    } else if (cmd == "MESSAGES CLEAR") {
        /* Clear all message buffers */
        public_msg_count = 0;
        public_msg_index = 0;
        direct_msg_count = 0;
        direct_msg_index = 0;
        for (int i = 0; i < MAX_CUSTOM_CHANNELS; i++) {
            channel_msg_count[i] = 0;
            channel_msg_index[i] = 0;
        }
        Serial.println("OK Messages cleared");
    } else if (cmd == "CHANNELS") {
        /* List channels */
        SerialOutput.print("{\"channels\":[");
        SerialOutput.print("{\"name\":\"Public\",\"hash\":\"0x");
        SerialOutput.print(String(public_channel_hash, HEX));
        SerialOutput.print("\",\"builtin\":true}");

        for (int i = 0; i < custom_channel_count; i++) {
            if (custom_channels[i].valid) {
                SerialOutput.print(",{\"name\":\"");
                SerialOutput.print(custom_channels[i].name);
                SerialOutput.print("\",\"hash\":\"0x");
                SerialOutput.print(String(custom_channels[i].hash, HEX));
                SerialOutput.print("\",\"builtin\":false}");
            }
        }
        SerialOutput.print("],\"total\":");
        SerialOutput.print(1 + custom_channel_count);
        Serial.println("}");
    } else if (cmd.startsWith("CHANNEL JOIN ")) {
        /* Join channel: CHANNEL JOIN <name> <psk_base64> */
        String args = cmd.substring(13);
        int space = args.indexOf(' ');
        if (space < 0) {
            Serial.println("ERR Usage: CHANNEL JOIN <name> <psk_base64>");
        } else {
            String name = args.substring(0, space);
            String psk = args.substring(space + 1);

            if (custom_channel_count >= MAX_CUSTOM_CHANNELS) {
                Serial.println("ERR Maximum channels reached");
            } else {
                /* Decode PSK */
                uint8_t secret[32];
                memset(secret, 0, sizeof(secret));  /* Zero-pad for 16-byte keys */

                size_t olen = 0;
                int ret = mbedtls_base64_decode(secret, sizeof(secret), &olen,
                                                 (const unsigned char*)psk.c_str(), psk.length());

                if (ret != 0 || (olen != 16 && olen != 32)) {
                    Serial.println("ERR Invalid PSK (must be 16 or 32 bytes base64-encoded)");
                } else {
                    /* Calculate channel hash (use actual decoded length) */
                    uint8_t hash_buf[32];
                    crypto_sha256(hash_buf, sizeof(hash_buf), secret, olen);
                    uint8_t hash = hash_buf[0];

                    /* Add channel (secret is zero-padded if 16 bytes) */
                    custom_channels[custom_channel_count].valid = true;
                    custom_channels[custom_channel_count].hash = hash;
                    strncpy(custom_channels[custom_channel_count].name, name.c_str(), 16);
                    custom_channels[custom_channel_count].name[16] = '\0';
                    memcpy(custom_channels[custom_channel_count].secret, secret, 32);  /* Copy all 32 bytes (zero-padded) */
                    custom_channel_count++;

                    /* Save channels to NVS */
                    channels_save_to_nvs();

                    SerialOutput.print("OK Joined channel: ");
                    SerialOutput.print(name);
                    SerialOutput.print(" (0x");
                    SerialOutput.print(String(hash, HEX));
                    Serial.println(")");
                }
            }
        }
    } else if (cmd.startsWith("CHANNEL SEND ")) {
        /* Send to channel: CHANNEL SEND <name> <text> */
        String args = cmd.substring(13);
        int space = args.indexOf(' ');
        if (space < 0) {
            Serial.println("ERR Usage: CHANNEL SEND <name> <text>");
        } else {
            String name = args.substring(0, space);
            String text = args.substring(space + 1);

            /* Find channel by name */
            bool found = false;
            if (name == "Public" || name == "public") {
                send_group_message(text.c_str());
                found = true;
            } else {
                for (int i = 0; i < custom_channel_count; i++) {
                    if (custom_channels[i].valid && strcmp(custom_channels[i].name, name.c_str()) == 0) {
                        send_channel_message(custom_channels[i].hash,
                                             custom_channels[i].secret,
                                             text.c_str(),
                                             custom_channels[i].name);
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                Serial.println("OK Message sent");
            } else {
                Serial.println("ERR Channel not found. Use CHANNELS to list or CHANNEL JOIN to add.");
            }
        }
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
    } else if (cmd == "IDENTITY ROTATE") {
        /* Clear saved identity - will regenerate on next boot */
        /* Also clear all encrypted data that depends on old keypair */

        /* Clear messages (can't decrypt with new identity) */
        public_msg_count = 0;
        public_msg_index = 0;
        direct_msg_count = 0;
        direct_msg_index = 0;
        for (int i = 0; i < MAX_CUSTOM_CHANNELS; i++) {
            channel_msg_count[i] = 0;
            channel_msg_index[i] = 0;
        }

        /* Clear saved neighbors (shared secrets will be invalid) */
        prefs.begin("neighbors", false);
        prefs.clear();
        prefs.end();

        /* Clear identity from NVS */
        prefs.begin("meshgrid", false);
        prefs.putBool("has_identity", false);
        prefs.remove("pubkey");
        prefs.remove("privkey");
        prefs.end();

        Serial.println("OK Identity rotated - all encrypted data cleared, rebooting...");
        delay(100);
        ESP.restart();
    } else if (cmd == "REBOOT") {
        Serial.println("OK Saving config and rebooting...");
        config_save();  /* Save everything before reboot */
        neighbors_save_to_nvs();  /* Save neighbors and cached secrets */
        channels_save_to_nvs();   /* Save custom channels */
        delay(100);
        ESP.restart();
#ifdef ENABLE_BLE
    } else if (cmd == "BLE STATUS") {
        if (ble_serial_connected()) {
            Serial.println("OK BLE connected");
        } else {
            Serial.println("OK BLE disconnected");
        }
    } else if (cmd == "BLE") {
        Serial.println("{\"ble_enabled\":true,\"connected\":" + String(ble_serial_connected() ? "true" : "false") + "}");
#endif
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
        SerialOutput.print("{\"name\":\"");
        SerialOutput.print(mesh.name);
        SerialOutput.print("\",\"freq_mhz\":");
        SerialOutput.print(radio_config.frequency, 2);
        SerialOutput.print(",\"tx_power_dbm\":");
        SerialOutput.print(radio_config.tx_power);
        SerialOutput.print(",\"bandwidth_khz\":");
        SerialOutput.print((int)(radio_config.bandwidth));
        SerialOutput.print(",\"spreading_factor\":");
        SerialOutput.print(radio_config.spreading_factor);
        SerialOutput.print(",\"coding_rate\":");
        SerialOutput.print(radio_config.coding_rate);
        SerialOutput.print(",\"preamble_len\":");
        SerialOutput.print(radio_config.preamble_len);
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
            radio_set_frequency(freq);
            radio_config.frequency = freq;
            config_save();
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid frequency");
        }
    } else if (cmd.startsWith("SET BW ")) {
        float bw = cmd.substring(7).toFloat();
        if (radio_set_bandwidth(bw) == RADIOLIB_ERR_NONE) {
            radio_config.bandwidth = bw;
            config_save();
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid bandwidth");
        }
    } else if (cmd.startsWith("SET SF ")) {
        int sf = cmd.substring(7).toInt();
        if (sf >= 6 && sf <= 12) {
            if (radio_set_spreading_factor(sf) == RADIOLIB_ERR_NONE) {
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
            if (radio_set_coding_rate(cr) == RADIOLIB_ERR_NONE) {
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
            if (radio_set_output_power(power) == RADIOLIB_ERR_NONE) {
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
            if (radio_set_preamble_length(preamble) == RADIOLIB_ERR_NONE) {
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
        radio_set_frequency(869.618);
        radio_set_bandwidth(62.5);
        radio_set_spreading_factor(8);
        radio_set_coding_rate(8);
        radio_set_preamble_length(16);  /* MeshCore uses 16 */
        radio_config.frequency = 869.618;
        radio_config.bandwidth = 62.5;
        radio_config.spreading_factor = 8;
        radio_config.coding_rate = 8;
        radio_config.preamble_len = 16;
        config_save();
        Serial.println("OK EU/UK Narrow: 869.618MHz 62.5kHz SF8 CR8 Pre16");
    } else if (cmd == "SET PRESET US_STANDARD" || cmd == "SET PRESET US") {
        /* US Standard - 915 MHz, 250 kHz BW, SF10, CR7 */
        radio_set_frequency(915.0);
        radio_set_bandwidth(250.0);
        radio_set_spreading_factor(10);
        radio_set_coding_rate(7);
        radio_set_preamble_length(16);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 250.0;
        radio_config.spreading_factor = 10;
        radio_config.coding_rate = 7;
        radio_config.preamble_len = 16;
        config_save();
        Serial.println("OK US Standard: 915MHz 250kHz SF10 CR7");
    } else if (cmd == "SET PRESET US_FAST") {
        /* US Fast - 915 MHz, 500 kHz BW, SF7, CR5 */
        radio_set_frequency(915.0);
        radio_set_bandwidth(500.0);
        radio_set_spreading_factor(7);
        radio_set_coding_rate(5);
        radio_set_preamble_length(8);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 500.0;
        radio_config.spreading_factor = 7;
        radio_config.coding_rate = 5;
        radio_config.preamble_len = 8;
        config_save();
        Serial.println("OK US Fast: 915MHz 500kHz SF7 CR5");
    } else if (cmd == "SET PRESET LONG_RANGE") {
        /* Long range - SF12, 125 kHz BW */
        radio_set_bandwidth(125.0);
        radio_set_spreading_factor(12);
        radio_set_coding_rate(8);
        radio_set_preamble_length(16);
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
        /* SEND <message> - broadcast group message to public channel */
        /* SEND <dest> <message> - encrypted direct message to specific node */
        String rest = cmd.substring(5);
        rest.trim();

        /* Try to parse as direct message: check if first word is a known neighbor */
        int spaceIdx = rest.indexOf(' ');
        bool is_direct = false;
        uint8_t dest_hash = 0;

        if (spaceIdx > 0 && spaceIdx < rest.length() - 1) {
            String maybe_dest = rest.substring(0, spaceIdx);

            /* Check if it's a hex hash */
            if (maybe_dest.startsWith("0x")) {
                dest_hash = (uint8_t)strtol(maybe_dest.c_str() + 2, NULL, 16);
                is_direct = true;
            } else {
                /* Check if it matches a neighbor name */
                for (int i = 0; i < neighbor_count; i++) {
                    if (strcmp(neighbors[i].name, maybe_dest.c_str()) == 0) {
                        dest_hash = neighbors[i].hash;
                        is_direct = true;
                        break;
                    }
                }
            }
        }

        if (is_direct) {
            /* Direct message to specific neighbor */
            String message = rest.substring(spaceIdx + 1);
            message.trim();

            if (message.length() > 0 && message.length() <= 150) {
                send_text_message(dest_hash, message.c_str());
                Serial.println("OK");
            } else {
                Serial.println("ERR Message too long");
            }
        } else {
            /* Broadcast to public channel (entire rest is the message) */
            if (rest.length() > 0 && rest.length() <= 150) {
                send_group_message(rest.c_str());
                Serial.println("OK");
            } else {
                Serial.println("ERR Message too long or empty");
            }
        }
    } else if (cmd.startsWith("TRACE ")) {
        /* TRACE <target> - send trace packet to map route */
        String target = cmd.substring(6);
        target.trim();

        if (target.length() == 0) {
            Serial.println("ERR Usage: TRACE <name|hash>");
            return;
        }

        uint8_t dest_hash = 0;
        bool found = false;

        /* Check if it's a hex hash */
        if (target.startsWith("0x")) {
            dest_hash = (uint8_t)strtol(target.c_str() + 2, NULL, 16);
            found = true;
        } else {
            /* Check if it matches a neighbor name */
            for (int i = 0; i < neighbor_count; i++) {
                if (strcmp(neighbors[i].name, target.c_str()) == 0) {
                    dest_hash = neighbors[i].hash;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            Serial.println("ERR Target not found in neighbor table");
            return;
        }

        /* Create trace packet */
        struct meshgrid_packet pkt;
        memset(&pkt, 0, sizeof(pkt));

        pkt.route_type = ROUTE_DIRECT;
        pkt.payload_type = PAYLOAD_TRACE;
        pkt.version = PAYLOAD_VER_MESHCORE;
        pkt.header = MESHGRID_MAKE_HEADER(pkt.route_type, pkt.payload_type, pkt.version);

        /* Build MeshCore-compatible TRACE packet */
        /* Payload format:
         *   0-3: trace_tag (trace ID)
         *   4-7: auth_code (timestamp or 0)
         *   8: flags (lower 2 bits = path_sz: 0=1byte hash)
         *   9+: path hashes (route to target)
         */
        uint32_t trace_id = millis();  /* Use timestamp as trace ID */
        uint32_t auth_code = get_uptime_secs();
        uint8_t flags = 0x00;  /* path_sz = 0 (1-byte hashes) */

        memcpy(&pkt.payload[0], &trace_id, 4);
        memcpy(&pkt.payload[4], &auth_code, 4);
        pkt.payload[8] = flags;

        /* Add target hash to payload (MeshCore stores route in payload) */
        pkt.payload[9] = dest_hash;
        pkt.payload_len = 10;

        /* Path array will store SNRs as nodes forward the packet */
        pkt.path_len = 0;

        /* Encode and transmit */
        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
        int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));
        if (tx_len > 0) {
            int16_t radio_result = get_radio()->transmit(tx_buf, tx_len);
            radio_in_rx_mode = false;  /* Mark as not in RX after TX */
            get_radio()->startReceive();

            if (radio_result == RADIOLIB_ERR_NONE) {
                mesh.packets_tx++;

                /* Output JSON response with trace info */
                Serial.print("{\"status\":\"sent\",\"target\":\"0x");
                Serial.print(dest_hash, HEX);
                Serial.print("\",\"trace_id\":");
                Serial.print(trace_id);
                Serial.print(",\"hops\":");
                Serial.print(pkt.path_len);
                Serial.println("}");

                /* Note: Response will come via PAYLOAD_PATH packet */
            } else {
                Serial.print("ERR Radio TX failed: ");
                Serial.println(radio_result);
            }
        } else {
            Serial.println("ERR Packet encode failed");
        }
    } else {
        SerialOutput.print("ERR Unknown command: ");
        Serial.println(cmd);
    }
}


void handle_serial(void) {
    // Read characters from both USB Serial and BLE Serial (non-blocking like MeshCore)
    bool complete = false;
    while (serial_bridge_available() && serial_cmd_len < sizeof(serial_cmd_buf) - 1) {
        char c = serial_bridge_read();
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

    /* Handle AUTH command (always allowed) */
    if (cmd.startsWith("AUTH ")) {
        String pin = cmd.substring(5);
        pin.trim();
        security_authenticate(pin.c_str());
        return;
    }

    /* Check authentication before processing commands */
    if (!security_check_auth()) {
        if (security_is_locked()) {
            uint32_t remaining = (security.lockout_until - millis()) / 1000;
            SerialOutput.print("ERROR: Device locked for ");
            SerialOutput.print(remaining);
            Serial.println(" seconds");
        } else {
            Serial.println("ERROR: Not authenticated. Send: AUTH <pin>");
            Serial.println("View PIN on OLED: Navigate to Security screen");
        }
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
    } else if (cmd == "/mode client" || cmd == "/mode cli") {
        device_mode = MODE_CLIENT;
        config_save();
        Serial.println("Mode: CLIENT");
    } else if (cmd == "/test battery" || cmd == "/test bat") {
        Serial.println("Starting battery drain test (1 minute)...");
        struct hw_test_result result;
        hw_test_battery(&result, 60000, [](const char *status, uint8_t pct) {
            SerialOutput.print("  ["); SerialOutput.print(pct); SerialOutput.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "battery");
        Serial.println(buf);
    } else if (cmd == "/test solar") {
        Serial.println("Starting solar panel test...");
        struct hw_test_result result;
        hw_test_solar(&result, [](const char *status, uint8_t pct) {
            SerialOutput.print("  ["); SerialOutput.print(pct); SerialOutput.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "solar");
        Serial.println(buf);
    } else if (cmd == "/test radio") {
        Serial.println("Starting radio TX test...");
        struct hw_test_result result;
        hw_test_radio(&result, [](const char *status, uint8_t pct) {
            SerialOutput.print("  ["); SerialOutput.print(pct); SerialOutput.print("%] ");
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

            /* Save RTC time to NVS for persistence across reboots */
            prefs.begin("meshgrid", false);
            prefs.putBool("rtc_valid", true);
            prefs.putUInt("rtc_epoch", rtc_time.epoch_at_boot);
            prefs.end();

            Serial.println("OK Time set");
        } else {
            Serial.println("ERR Invalid time format. Usage: /time YYYY-MM-DD HH:MM:SS");
        }
    } else if (cmd == "/pin show") {
        /* Show PIN (useful when connected via BLE) */
        SerialOutput.print("PIN: ");
        Serial.println(security.pin);
        SerialOutput.print("Status: ");
        Serial.println(security.pin_enabled ? "ENABLED" : "DISABLED");
    } else if (cmd.startsWith("/pin set ")) {
        /* Set new PIN */
        String new_pin = cmd.substring(9);
        new_pin.trim();
        security_set_pin(new_pin.c_str());
    } else if (cmd == "/pin enable") {
        /* Enable PIN authentication */
        security_enable_pin();
    } else if (cmd == "/pin disable") {
        /* Disable PIN authentication */
        security_disable_pin();
    } else {
        SerialOutput.print("Unknown: ");
        Serial.println(cmd);
    }

    display_state.dirty = true;
}
