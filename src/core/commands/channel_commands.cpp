/**
 * Channel command implementations
 */

#include "channel_commands.h"
#include "common.h"
#include "utils/constants.h"
#include "utils/types.h"
#include "utils/memory.h"
#include <mbedtls/base64.h>

// Use C bridge to avoid namespace conflict
extern "C" {
    #include "core/meshcore_bridge.h"
}

extern "C" {
#include "hardware/crypto/crypto.h"
}

extern uint8_t public_channel_hash;
extern struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
extern int custom_channel_count;
extern void channels_save_to_nvs(void);
extern void send_group_message(const char *text);

#define send_channel_message meshcore_bridge_send_channel

void cmd_channels() {
    response_print("{\"channels\":[");
    response_print("{\"name\":\"Public\",\"hash\":\"0x");
    response_print(String(public_channel_hash, HEX));
    response_print("\",\"builtin\":true}");

    for (int i = 0; i < custom_channel_count; i++) {
        if (custom_channels[i].valid) {
            response_print(",{\"name\":\"");
            response_print(custom_channels[i].name);
            response_print("\",\"hash\":\"0x");
            response_print(String(custom_channels[i].hash, HEX));
            response_print("\",\"builtin\":false}");
        }
    }
    response_print("],\"total\":");
    response_print(1 + custom_channel_count);
    response_println("}");
}

void cmd_channel_join(const String &args) {
    int space = args.indexOf(' ');
    if (space < 0) {
        response_println("ERR Usage: CHANNEL JOIN <name> <psk_base64>");
        return;
    }

    String name = args.substring(0, space);
    String psk = args.substring(space + 1);

    if (custom_channel_count >= MAX_CUSTOM_CHANNELS) {
        response_println("ERR Maximum channels reached");
        return;
    }

    /* Decode PSK */
    uint8_t secret[32];
    memset(secret, 0, sizeof(secret));

    size_t olen = 0;
    int ret = mbedtls_base64_decode(secret, sizeof(secret), &olen,
                                     (const unsigned char*)psk.c_str(), psk.length());

    if (ret != 0 || (olen != 16 && olen != 32)) {
        response_println("ERR Invalid PSK (must be 16 or 32 bytes base64-encoded)");
        return;
    }

    /* Calculate channel hash */
    uint8_t hash_buf[32];
    crypto_sha256(hash_buf, sizeof(hash_buf), secret, olen);
    uint8_t hash = hash_buf[0];

    /* Add channel */
    custom_channels[custom_channel_count].valid = true;
    custom_channels[custom_channel_count].hash = hash;
    strncpy(custom_channels[custom_channel_count].name, name.c_str(), 16);
    custom_channels[custom_channel_count].name[16] = '\0';
    memcpy(custom_channels[custom_channel_count].secret, secret, 32);
    custom_channel_count++;

    channels_save_to_nvs();

    response_print("OK Joined channel: ");
    response_print(name);
    response_print(" (0x");
    response_print(String(hash, HEX));
    response_println(")");
}

void cmd_channel_send(const String &args) {
    int space = args.indexOf(' ');
    if (space < 0) {
        response_println("ERR Usage: CHANNEL SEND <name> <text>");
        return;
    }

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
        response_println("OK Message sent");
    } else {
        response_println("ERR Channel not found. Use CHANNELS to list or CHANNEL JOIN to add.");
    }
}
