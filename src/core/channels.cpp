/**
 * Channel management and persistence
 */

#include "utils/debug.h"
#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/base64.h>
#include "utils/memory.h"
#include "utils/types.h"

extern "C" {
#include "hardware/crypto/crypto.h"
}

/* Externs from main.cpp - struct in lib/types.h */
extern struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
extern int custom_channel_count;

void channels_save_to_nvs(void) {
    Preferences prefs;
    prefs.begin("channels", false);  // Read-write

    /* Count valid channels */
    uint8_t saved_count = 0;
    for (int i = 0; i < custom_channel_count && i < MAX_CUSTOM_CHANNELS; i++) {
        if (custom_channels[i].valid) {
            char key[16];
            snprintf(key, sizeof(key), "c%d_hash", saved_count);
            prefs.putUChar(key, custom_channels[i].hash);

            snprintf(key, sizeof(key), "c%d_name", saved_count);
            prefs.putString(key, custom_channels[i].name);

            snprintf(key, sizeof(key), "c%d_secret", saved_count);
            prefs.putBytes(key, custom_channels[i].secret, 32);

            saved_count++;
        }
    }

    prefs.putUChar("count", saved_count);
    prefs.end();

    DEBUG_INFOF("Saved %d channels to NVS", saved_count);
}

void channels_load_from_nvs(void) {
    Preferences prefs;
    prefs.begin("channels", true);  // Read-only

    uint8_t saved_count = prefs.getUChar("count", 0);
    if (saved_count > MAX_CUSTOM_CHANNELS) saved_count = MAX_CUSTOM_CHANNELS;

    custom_channel_count = 0;
    for (uint8_t i = 0; i < saved_count; i++) {
        if (custom_channel_count >= MAX_CUSTOM_CHANNELS) break;

        struct channel_entry *ch = &custom_channels[custom_channel_count];

        char key[16];
        snprintf(key, sizeof(key), "c%d_hash", i);
        ch->hash = prefs.getUChar(key, 0);
        if (ch->hash == 0) continue;

        snprintf(key, sizeof(key), "c%d_name", i);
        String name = prefs.getString(key, "");
        if (name.length() == 0) continue;
        strncpy(ch->name, name.c_str(), 16);
        ch->name[16] = '\0';

        snprintf(key, sizeof(key), "c%d_secret", i);
        if (prefs.getBytes(key, ch->secret, 32) != 32) continue;

        ch->valid = true;
        custom_channel_count++;

        DEBUG_INFOF("Restored channel: %s (0x%02x)", ch->name, ch->hash);
    }

    prefs.end();
}
