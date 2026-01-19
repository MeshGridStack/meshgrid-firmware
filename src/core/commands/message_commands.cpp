/**
 * Message command implementations
 */

#include "message_commands.h"
#include "common.h"
#include "utils/constants.h"
#include "utils/types.h"
#include "utils/memory.h"

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

void cmd_messages() {
    response_print("{\"messages\":[");

    int total_shown = 0;

    /* Show public channel messages */
    int start = (public_msg_count < PUBLIC_MESSAGE_BUFFER_SIZE) ? 0 : public_msg_index;
    for (int i = 0; i < public_msg_count; i++) {
        int idx = (start + i) % PUBLIC_MESSAGE_BUFFER_SIZE;
        struct message_entry *msg = &public_messages[idx];
        if (!msg->valid) continue;

        if (total_shown > 0) response_print(",");
        response_print("{");
        response_print("\"from_hash\":\"0x"); response_print(String(msg->sender_hash, HEX)); response_print("\",");
        response_print("\"from_name\":\""); print_json_string(msg->sender_name); response_print("\",");
        response_print("\"channel\":\"");
        if (msg->channel_hash == public_channel_hash) {
            response_print("public");
        } else {
            response_print("0x"); response_print(String(msg->channel_hash, HEX));
        }
        response_print("\",");
        response_print("\"protocol\":\"v"); response_print(msg->protocol_version); response_print("\",");
        response_print("\"decrypted\":"); response_print(msg->decrypted ? "true" : "false"); response_print(",");
        response_print("\"timestamp\":"); response_print(msg->timestamp); response_print(",");
        response_print("\"text\":\""); print_json_string(msg->text); response_print("\"");
        response_print("}");
        total_shown++;
    }

    /* Show direct messages */
    start = (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) ? 0 : direct_msg_index;
    for (int i = 0; i < direct_msg_count; i++) {
        int idx = (start + i) % DIRECT_MESSAGE_BUFFER_SIZE;
        struct message_entry *msg = &direct_messages[idx];
        if (!msg->valid) continue;

        if (total_shown > 0) response_print(",");
        response_print("{");
        response_print("\"from_hash\":\"0x"); response_print(String(msg->sender_hash, HEX)); response_print("\",");
        response_print("\"from_name\":\""); print_json_string(msg->sender_name); response_print("\",");
        response_print("\"channel\":\"direct\",");
        response_print("\"protocol\":\"v"); response_print(msg->protocol_version); response_print("\",");
        response_print("\"decrypted\":"); response_print(msg->decrypted ? "true" : "false"); response_print(",");
        response_print("\"timestamp\":"); response_print(msg->timestamp); response_print(",");
        response_print("\"text\":\""); print_json_string(msg->text); response_print("\"");
        response_print("}");
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

            if (total_shown > 0) response_print(",");
            response_print("{");
            response_print("\"from_hash\":\"0x"); response_print(String(msg->sender_hash, HEX)); response_print("\",");
            response_print("\"from_name\":\""); print_json_string(msg->sender_name); response_print("\",");
            response_print("\"channel\":\""); print_json_string(custom_channels[ch].name); response_print("\",");
            response_print("\"protocol\":\"v"); response_print(msg->protocol_version); response_print("\",");
            response_print("\"decrypted\":"); response_print(msg->decrypted ? "true" : "false"); response_print(",");
            response_print("\"timestamp\":"); response_print(msg->timestamp); response_print(",");
            response_print("\"text\":\""); print_json_string(msg->text); response_print("\"");
            response_print("}");
            total_shown++;
        }
    }

    response_print("],\"total\":"); response_print(total_shown);
    response_println("}");
}

void cmd_messages_clear() {
    public_msg_count = 0;
    public_msg_index = 0;
    direct_msg_count = 0;
    direct_msg_index = 0;
    for (int i = 0; i < MAX_CUSTOM_CHANNELS; i++) {
        channel_msg_count[i] = 0;
        channel_msg_index[i] = 0;
    }
    response_println("OK Messages cleared");
}
