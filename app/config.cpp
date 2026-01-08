/**
 * Configuration persistence
 */

#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "lib/board.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/crypto/crypto.h"
}

/* Public channel (MeshCore compatible) */
#define PUBLIC_CHANNEL_PSK "izOH6cXN6mrJ5e26oRXNcg=="
extern uint8_t public_channel_secret[32];
extern uint8_t public_channel_hash;

/* Externs from main.cpp */
extern Preferences prefs;
extern struct meshgrid_state mesh;
extern struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
} radio_config;

extern enum meshgrid_device_mode device_mode;
extern bool log_enabled;
extern String log_buffer[];
extern int log_index, log_count;

extern const struct board_config *board;

#define LOG_BUFFER_SIZE 50
#define MESHGRID_NODE_NAME_MAX 16


void init_public_channel(void) {
    /* Decode Base64 PSK */
    size_t olen = 0;
    int ret = mbedtls_base64_decode(public_channel_secret, sizeof(public_channel_secret),
                                     &olen, (const unsigned char*)PUBLIC_CHANNEL_PSK,
                                     strlen(PUBLIC_CHANNEL_PSK));

    if (ret == 0 && olen == 32) {
        /* Calculate channel hash (first byte of SHA256 of secret) */
        uint8_t hash[32];
        crypto_sha256(hash, sizeof(hash), public_channel_secret, 32);
        public_channel_hash = hash[0];

        Serial.print("Public channel initialized, hash: 0x");
        Serial.println(public_channel_hash, HEX);
    } else {
        Serial.println("ERROR: Failed to decode PUBLIC_CHANNEL_PSK");
    }
}

void config_load(void) {
    prefs.begin("meshgrid", true);  // Read-only
    radio_config.config_saved = prefs.getBool("saved", false);

    if (radio_config.config_saved) {
        radio_config.frequency = prefs.getFloat("freq", board->lora_defaults.frequency);
        radio_config.bandwidth = prefs.getFloat("bw", board->lora_defaults.bandwidth);
        radio_config.spreading_factor = prefs.getUChar("sf", board->lora_defaults.spreading_factor);
        radio_config.coding_rate = prefs.getUChar("cr", board->lora_defaults.coding_rate);
        radio_config.preamble_len = prefs.getUShort("preamble", board->lora_defaults.preamble_len);
        radio_config.tx_power = prefs.getChar("power", board->lora_defaults.tx_power);
        Serial.println("Loaded radio config from flash");
    } else {
        // Use board defaults
        radio_config.frequency = board->lora_defaults.frequency;
        radio_config.bandwidth = board->lora_defaults.bandwidth;
        radio_config.spreading_factor = board->lora_defaults.spreading_factor;
        radio_config.coding_rate = board->lora_defaults.coding_rate;
        radio_config.preamble_len = board->lora_defaults.preamble_len;
        radio_config.tx_power = board->lora_defaults.tx_power;
        Serial.println("Using board default radio config");
    }

    /* Load device mode */
    device_mode = (enum meshgrid_device_mode)prefs.getUChar("mode", MODE_CLIENT);

    /* Load logging state */
    log_enabled = prefs.getBool("log_en", false);

    /* Load node name if saved */
    String saved_name = prefs.getString("name", "");
    if (saved_name.length() > 0) {
        strncpy(mesh.name, saved_name.c_str(), MESHGRID_NODE_NAME_MAX);
        mesh.name[MESHGRID_NODE_NAME_MAX] = '\0';
        Serial.println("Loaded node name from flash");
    }

    prefs.end();
}

void config_save(void) {
    prefs.begin("meshgrid", false);  // Read-write
    prefs.putBool("saved", true);
    prefs.putFloat("freq", radio_config.frequency);
    prefs.putFloat("bw", radio_config.bandwidth);
    prefs.putUChar("sf", radio_config.spreading_factor);
    prefs.putUChar("cr", radio_config.coding_rate);
    prefs.putUShort("preamble", radio_config.preamble_len);
    prefs.putChar("power", radio_config.tx_power);

    /* Save device mode */
    prefs.putUChar("mode", (uint8_t)device_mode);

    /* Save logging state */
    prefs.putBool("log_en", log_enabled);

    /* Save node name */
    prefs.putString("name", mesh.name);

    /* Save recent log entries (last 20 to limit NVS usage) */
    int save_count = (log_count > 20) ? 20 : log_count;
    prefs.putUChar("log_cnt", save_count);
    if (save_count > 0) {
        int start = (log_count < LOG_BUFFER_SIZE) ? 0 : log_index;
        /* Save most recent entries */
        int saved = 0;
        for (int i = log_count - save_count; i < log_count; i++) {
            int idx = (start + i) % LOG_BUFFER_SIZE;
            char key[16];
            snprintf(key, sizeof(key), "log%d", saved);
            prefs.putString(key, log_buffer[idx]);
            saved++;
        }
    }

    prefs.end();
    radio_config.config_saved = true;
    Serial.println("Saved config to flash");
}
