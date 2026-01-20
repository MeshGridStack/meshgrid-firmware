/**
 * Configuration persistence
 */

#include "config.h"
#include "utils/debug.h"
#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "hardware/board.h"

extern "C" {
#include "network/protocol.h"
#include "hardware/crypto/crypto.h"
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
extern const struct board_config* board;

/* RTC time tracking */
struct rtc_time_t {
    bool valid;
    uint32_t epoch_at_boot;
};
extern struct rtc_time_t rtc_time;

#define MESHGRID_NODE_NAME_MAX 16

void init_public_channel(void) {
    /* Decode Base64 PSK */
    memset(public_channel_secret, 0, sizeof(public_channel_secret)); /* Zero the buffer first */

    size_t olen = 0;
    int ret = mbedtls_base64_decode(public_channel_secret, sizeof(public_channel_secret), &olen,
                                    (const unsigned char*)PUBLIC_CHANNEL_PSK, strlen(PUBLIC_CHANNEL_PSK));

    if (ret == 0 && (olen == 16 || olen == 32)) {
        /* MeshCore supports both 128-bit (16 bytes) and 256-bit (32 bytes) keys */
        /* If 16 bytes, upper 16 bytes are already zeroed (MeshCore compatible) */

        /* Calculate channel hash (first byte of SHA256 of secret) */
        uint8_t hash[32];
        crypto_sha256(hash, sizeof(hash), public_channel_secret, olen);
        public_channel_hash = hash[0];

        DEBUG_INFOF("Public channel initialized, hash: 0x%02x (%d byte key)", public_channel_hash, olen);
    } else {
        DEBUG_ERRORF("Failed to decode PUBLIC_CHANNEL_PSK (got %d bytes)", olen);
    }
}

void config_load(void) {
    prefs.begin("meshgrid", true); // Read-only
    radio_config.config_saved = prefs.getBool("saved", false);

    if (radio_config.config_saved) {
        radio_config.frequency = prefs.getFloat("freq", board->lora_defaults.frequency);
        radio_config.bandwidth = prefs.getFloat("bw", board->lora_defaults.bandwidth);
        radio_config.spreading_factor = prefs.getUChar("sf", board->lora_defaults.spreading_factor);
        radio_config.coding_rate = prefs.getUChar("cr", board->lora_defaults.coding_rate);
        radio_config.preamble_len = prefs.getUShort("preamble", board->lora_defaults.preamble_len);
        radio_config.tx_power = prefs.getChar("power", board->lora_defaults.tx_power);
        DEBUG_INFO("Loaded radio config from flash");
    } else {
        // Use board defaults
        radio_config.frequency = board->lora_defaults.frequency;
        radio_config.bandwidth = board->lora_defaults.bandwidth;
        radio_config.spreading_factor = board->lora_defaults.spreading_factor;
        radio_config.coding_rate = board->lora_defaults.coding_rate;
        radio_config.preamble_len = board->lora_defaults.preamble_len;
        radio_config.tx_power = board->lora_defaults.tx_power;
        DEBUG_INFO("Using board default radio config");
    }

    /* Load device mode */
    device_mode = (enum meshgrid_device_mode)prefs.getUChar("mode", MODE_CLIENT);

    /* Load node name if saved */
    String saved_name = prefs.getString("name", "");
    if (saved_name.length() > 0) {
        strncpy(mesh.name, saved_name.c_str(), MESHGRID_NODE_NAME_MAX);
        mesh.name[MESHGRID_NODE_NAME_MAX] = '\0';
        DEBUG_INFO("Loaded node name from flash");
    }

    /* Load RTC time if saved */
    if (prefs.getBool("rtc_valid", false)) {
        rtc_time.epoch_at_boot = prefs.getUInt("rtc_epoch", 0);
        rtc_time.valid = true;
        DEBUG_INFO("Loaded RTC time from flash");
    }

    prefs.end();
}

void config_save(void) {
    prefs.begin("meshgrid", false); // Read-write
    prefs.putBool("saved", true);
    prefs.putFloat("freq", radio_config.frequency);
    prefs.putFloat("bw", radio_config.bandwidth);
    prefs.putUChar("sf", radio_config.spreading_factor);
    prefs.putUChar("cr", radio_config.coding_rate);
    prefs.putUShort("preamble", radio_config.preamble_len);
    prefs.putChar("power", radio_config.tx_power);

    /* Save device mode */
    prefs.putUChar("mode", (uint8_t)device_mode);

    /* Save node name */
    prefs.putString("name", mesh.name);

    prefs.end();
    radio_config.config_saved = true;
    DEBUG_INFO("Saved config to flash");
}
