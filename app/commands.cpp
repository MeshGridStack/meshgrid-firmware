/**
 * Serial command handling
 */

#include "commands.h"
#include "neighbors.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "lib/board.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/telemetry/telemetry.h"
#include "drivers/test/hw_test.h"
#include "drivers/crypto/crypto.h"
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

/* Custom channels */
#define MAX_CUSTOM_CHANNELS 50

/* Message inbox buffers (tiered) */
#define PUBLIC_MESSAGE_BUFFER_SIZE 100
#define CHANNEL_MESSAGE_BUFFER_SIZE 5
#define DIRECT_MESSAGE_BUFFER_SIZE 50

struct message_entry {
    bool valid;
    bool decrypted;
    uint8_t sender_hash;
    char sender_name[17];
    uint8_t channel_hash;
    uint32_t timestamp;
    char text[128];
};

extern struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
extern int public_msg_index, public_msg_count;

extern struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
extern int direct_msg_index, direct_msg_count;

extern struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[MAX_CUSTOM_CHANNELS];
extern int channel_msg_count[MAX_CUSTOM_CHANNELS];

extern uint8_t public_channel_hash;
struct channel_entry {
    bool valid;
    uint8_t hash;
    char name[17];
    uint8_t secret[32];
};
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
            Serial.print('.');  // Replace control chars with .
        } else if (c == '"' || c == '\\') {
            Serial.print('\\');
            Serial.print(c);
        } else {
            Serial.print(c);
        }
    }
}

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
    } else if (cmd == "MESSAGES" || cmd == "INBOX") {
        /* Show message inbox - aggregate from all buffers */
        Serial.print("{\"messages\":[");

        int total_shown = 0;

        /* Show public channel messages */
        int start = (public_msg_count < PUBLIC_MESSAGE_BUFFER_SIZE) ? 0 : public_msg_index;
        for (int i = 0; i < public_msg_count; i++) {
            int idx = (start + i) % PUBLIC_MESSAGE_BUFFER_SIZE;
            struct message_entry *msg = &public_messages[idx];
            if (!msg->valid) continue;

            if (total_shown > 0) Serial.print(",");
            Serial.print("{");
            Serial.print("\"from_hash\":\"0x"); Serial.print(String(msg->sender_hash, HEX)); Serial.print("\",");
            Serial.print("\"from_name\":\""); print_json_string(msg->sender_name); Serial.print("\",");
            Serial.print("\"channel\":\"");
            if (msg->channel_hash == public_channel_hash) {
                Serial.print("public");
            } else {
                Serial.print("0x"); Serial.print(String(msg->channel_hash, HEX));
            }
            Serial.print("\",");
            Serial.print("\"decrypted\":"); Serial.print(msg->decrypted ? "true" : "false"); Serial.print(",");
            Serial.print("\"timestamp\":"); Serial.print(msg->timestamp); Serial.print(",");
            Serial.print("\"text\":\""); print_json_string(msg->text); Serial.print("\"");
            Serial.print("}");
            total_shown++;
        }

        /* Show direct messages */
        start = (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) ? 0 : direct_msg_index;
        for (int i = 0; i < direct_msg_count; i++) {
            int idx = (start + i) % DIRECT_MESSAGE_BUFFER_SIZE;
            struct message_entry *msg = &direct_messages[idx];
            if (!msg->valid) continue;

            if (total_shown > 0) Serial.print(",");
            Serial.print("{");
            Serial.print("\"from_hash\":\"0x"); Serial.print(String(msg->sender_hash, HEX)); Serial.print("\",");
            Serial.print("\"from_name\":\""); print_json_string(msg->sender_name); Serial.print("\",");
            Serial.print("\"channel\":\"direct\",");
            Serial.print("\"decrypted\":"); Serial.print(msg->decrypted ? "true" : "false"); Serial.print(",");
            Serial.print("\"timestamp\":"); Serial.print(msg->timestamp); Serial.print(",");
            Serial.print("\"text\":\""); print_json_string(msg->text); Serial.print("\"");
            Serial.print("}");
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

                if (total_shown > 0) Serial.print(",");
                Serial.print("{");
                Serial.print("\"from_hash\":\"0x"); Serial.print(String(msg->sender_hash, HEX)); Serial.print("\",");
                Serial.print("\"from_name\":\""); print_json_string(msg->sender_name); Serial.print("\",");
                Serial.print("\"channel\":\""); print_json_string(custom_channels[ch].name); Serial.print("\",");
                Serial.print("\"decrypted\":"); Serial.print(msg->decrypted ? "true" : "false"); Serial.print(",");
                Serial.print("\"timestamp\":"); Serial.print(msg->timestamp); Serial.print(",");
                Serial.print("\"text\":\""); print_json_string(msg->text); Serial.print("\"");
                Serial.print("}");
                total_shown++;
            }
        }

        Serial.print("],\"total\":"); Serial.print(total_shown);
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
        Serial.print("{\"channels\":[");
        Serial.print("{\"name\":\"Public\",\"hash\":\"0x");
        Serial.print(String(public_channel_hash, HEX));
        Serial.print("\",\"builtin\":true}");

        for (int i = 0; i < custom_channel_count; i++) {
            if (custom_channels[i].valid) {
                Serial.print(",{\"name\":\"");
                Serial.print(custom_channels[i].name);
                Serial.print("\",\"hash\":\"0x");
                Serial.print(String(custom_channels[i].hash, HEX));
                Serial.print("\",\"builtin\":false}");
            }
        }
        Serial.print("],\"total\":");
        Serial.print(1 + custom_channel_count);
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

                    Serial.print("OK Joined channel: ");
                    Serial.print(name);
                    Serial.print(" (0x");
                    Serial.print(String(hash, HEX));
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
        /* TODO: Implement proper PAYLOAD_TRACE */
        Serial.println("ERR TRACE not implemented yet");
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

            /* Save RTC time to NVS for persistence across reboots */
            prefs.begin("meshgrid", false);
            prefs.putBool("rtc_valid", true);
            prefs.putUInt("rtc_epoch", rtc_time.epoch_at_boot);
            prefs.end();

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
