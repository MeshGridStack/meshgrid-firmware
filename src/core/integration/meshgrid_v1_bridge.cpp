/**
 * meshgrid v1 Protocol Bridge Implementation
 *
 * Phase 6: v1 messaging enabled
 * Connects lib/meshgrid-v1 protocol to application.
 */

#include "meshgrid_v1_bridge.h"
#include "../neighbors.h"
#include "../messaging.h"
#include "utils/debug.h"
#include "utils/types.h"
#include <Arduino.h>
#include <string.h>

extern "C" {
    /* v1 protocol headers */
    #include "../../../lib/meshgrid-v1/src/protocol/crypto.h"
    #include "../../../lib/meshgrid-v1/src/protocol/packet.h"
    #include "../../../lib/meshgrid-v1/src/discovery/bloom.h"
    #include "../../../lib/meshgrid-v1/src/discovery/trickle.h"

    /* Radio function */
    int16_t radio_transmit(uint8_t* data, size_t len);
}

/* External from main.cpp */
extern struct meshgrid_state mesh;
extern struct rtc_time_t rtc_time;

/* Helper to get current Unix timestamp */
static inline uint32_t get_current_timestamp(void) {
    if (rtc_time.valid) {
        return rtc_time.epoch_at_boot + (millis() / 1000);
    }
    return millis() / 1000;  /* Fallback to uptime */
}

/* External message storage */
extern struct message_entry direct_messages[];
extern int direct_msg_index;
extern int direct_msg_count;
#define DIRECT_MESSAGE_BUFFER_SIZE 10

/* v1 protocol state */
static bool v1_initialized = false;

/**
 * Initialize v1 protocol bridge
 */
void meshgrid_v1_bridge_init(void) {
    v1_initialized = true;
    DEBUG_INFO("[v1] Bridge initialized");
}

/**
 * Check if neighbor supports v1 protocol
 */
bool meshgrid_v1_peer_supports_v1(uint8_t hash) {
    struct meshgrid_neighbor *neighbor = neighbor_find(hash);
    if (neighbor == nullptr) {
        DEBUG_WARNF("[v1] peer_supports_v1: neighbor 0x%02x not found", hash);
        return false;
    }
    DEBUG_INFOF("[v1] peer_supports_v1: hash=0x%02x, protocol_version=%d, supports=%d",
                hash, neighbor->protocol_version, neighbor->protocol_version >= 1);
    return (neighbor->protocol_version >= 1);
}

/**
 * Send text message using v1 protocol
 */
int meshgrid_v1_send_text(uint16_t dest_hash_v1, const char *text, size_t len) {
    DEBUG_INFOF("[v1] meshgrid_v1_send_text called: dest=0x%04x, len=%d", dest_hash_v1, len);

    if (!v1_initialized) {
        DEBUG_INFO("[v1] Auto-initializing v1 bridge");
        meshgrid_v1_bridge_init();  /* Auto-initialize on first use */
    }

    /* Find neighbor by scanning for matching v1 hash */
    struct meshgrid_neighbor *neighbor = nullptr;
    for (int i = 0; i < neighbor_count; i++) {
        uint16_t n_hash_v1 = meshgrid_v1_hash_pubkey(neighbors[i].pubkey);
        if (n_hash_v1 == dest_hash_v1) {
            neighbor = &neighbors[i];
            break;
        }
    }
    if (!neighbor) {
        DEBUG_WARNF("[v1] Neighbor with v1_hash=0x%04x not found", dest_hash_v1);
        return -1;
    }
    if (!neighbor->secret_valid) {
        DEBUG_WARNF("[v1] No shared secret for hash 0x%04x (secret_valid=false)", dest_hash_v1);
        return -1;
    }

    DEBUG_INFOF("[v1] Found neighbor, secret_valid=true, proceeding with v1 send", "");

    /* Get next sequence number */
    uint32_t sequence = neighbor->next_seq_tx++;
    if (neighbor->next_seq_tx == 0) neighbor->next_seq_tx = 1;  /* Skip 0 */

    /* Generate nonce */
    uint8_t nonce[12];
    meshgrid_v1_generate_nonce(nonce, millis());

    /* Build plaintext: [dest_hash(2)][src_hash(2)][sequence(4)][timestamp(4)][text] */
    uint8_t plaintext[200];
    int pt_pos = 0;

    uint16_t src_hash_v1 = meshgrid_v1_hash_pubkey(mesh.pubkey);
    uint32_t timestamp = get_current_timestamp();

    plaintext[pt_pos++] = (dest_hash_v1 >> 8) & 0xFF;
    plaintext[pt_pos++] = dest_hash_v1 & 0xFF;
    plaintext[pt_pos++] = (src_hash_v1 >> 8) & 0xFF;
    plaintext[pt_pos++] = src_hash_v1 & 0xFF;
    plaintext[pt_pos++] = (sequence >> 24) & 0xFF;
    plaintext[pt_pos++] = (sequence >> 16) & 0xFF;
    plaintext[pt_pos++] = (sequence >> 8) & 0xFF;
    plaintext[pt_pos++] = sequence & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 24) & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 16) & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 8) & 0xFF;
    plaintext[pt_pos++] = timestamp & 0xFF;

    if (len > sizeof(plaintext) - pt_pos) {
        len = sizeof(plaintext) - pt_pos;
    }
    memcpy(&plaintext[pt_pos], text, len);
    pt_pos += len;

    /* Encrypt with AES-GCM */
    uint8_t ciphertext[200];
    uint8_t tag[16];
    if (meshgrid_v1_aes_gcm_encrypt(neighbor->shared_secret, nonce, nullptr, 0,
                                     plaintext, pt_pos, ciphertext, tag) != 0) {
        DEBUG_WARN("[v1] Encryption failed");
        return -1;
    }

    /* Build packet: [header][nonce(12)][ciphertext][tag(16)] */
    uint8_t packet[255];
    int pkt_pos = 0;

    /* Header: route=DIRECT, type=TXT_MSG, version=1 */
    packet[pkt_pos++] = MESHGRID_MAKE_HEADER(ROUTE_DIRECT, PAYLOAD_TXT_MSG, 1);

    /* Nonce */
    memcpy(&packet[pkt_pos], nonce, 12);
    pkt_pos += 12;

    /* Ciphertext */
    memcpy(&packet[pkt_pos], ciphertext, pt_pos);
    pkt_pos += pt_pos;

    /* Tag */
    memcpy(&packet[pkt_pos], tag, 16);
    pkt_pos += 16;

    /* Transmit */
    DEBUG_INFOF("[v1] Sending text to 0x%04x, seq=%lu, len=%d", dest_hash_v1, sequence, pkt_pos);
    int result = radio_transmit(packet, pkt_pos);

    if (result == 0) {
        mesh.packets_tx++;
        return 0;
    }

    return -1;
}

/**
 * Send channel message using v1 protocol
 */
int meshgrid_v1_send_channel(uint8_t channel_hash, const char *text, size_t len) {
    DEBUG_INFOF("[v1] meshgrid_v1_send_channel: channel=0x%02x, len=%d", channel_hash, len);

    if (!v1_initialized) {
        meshgrid_v1_bridge_init();
    }

    /* Find channel to get secret */
    extern struct channel_entry custom_channels[];
    extern int custom_channel_count;

    struct channel_entry *channel = nullptr;
    for (int i = 0; i < custom_channel_count; i++) {
        if (custom_channels[i].valid && custom_channels[i].hash == channel_hash) {
            channel = &custom_channels[i];
            break;
        }
    }

    if (!channel) {
        DEBUG_WARNF("[v1] Channel 0x%02x not found", channel_hash);
        return -1;
    }

    /* Generate nonce */
    uint8_t nonce[12];
    meshgrid_v1_generate_nonce(nonce, millis());

    /* Build plaintext: [channel_hash(1)][src_hash(2)][timestamp(4)][text] */
    uint8_t plaintext[200];
    int pt_pos = 0;

    uint16_t src_hash_v1 = meshgrid_v1_hash_pubkey(mesh.pubkey);
    uint32_t timestamp = get_current_timestamp();

    plaintext[pt_pos++] = channel_hash;
    plaintext[pt_pos++] = (src_hash_v1 >> 8) & 0xFF;
    plaintext[pt_pos++] = src_hash_v1 & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 24) & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 16) & 0xFF;
    plaintext[pt_pos++] = (timestamp >> 8) & 0xFF;
    plaintext[pt_pos++] = timestamp & 0xFF;

    if (len > sizeof(plaintext) - pt_pos) {
        len = sizeof(plaintext) - pt_pos;
    }
    memcpy(&plaintext[pt_pos], text, len);
    pt_pos += len;

    /* Encrypt with channel secret */
    uint8_t ciphertext[200];
    uint8_t tag[16];
    if (meshgrid_v1_aes_gcm_encrypt(channel->secret, nonce, nullptr, 0,
                                     plaintext, pt_pos, ciphertext, tag) != 0) {
        DEBUG_WARN("[v1] Channel encryption failed");
        return -1;
    }

    /* Build packet: [header][nonce(12)][ciphertext][tag(16)] */
    uint8_t packet[255];
    int pkt_pos = 0;

    /* Header: route=FLOOD, type=GRP_MSG, version=1 */
    packet[pkt_pos++] = MESHGRID_MAKE_HEADER(ROUTE_FLOOD, PAYLOAD_GRP_TXT, 1);

    /* Nonce */
    memcpy(&packet[pkt_pos], nonce, 12);
    pkt_pos += 12;

    /* Ciphertext */
    memcpy(&packet[pkt_pos], ciphertext, pt_pos);
    pkt_pos += pt_pos;

    /* Tag */
    memcpy(&packet[pkt_pos], tag, 16);
    pkt_pos += 16;

    /* Transmit */
    DEBUG_INFOF("[v1] Sending channel msg to 0x%02x, len=%d", channel_hash, pkt_pos);
    int result = radio_transmit(packet, pkt_pos);

    if (result == 0) {
        mesh.packets_tx++;
        return 0;
    }

    return -1;
}

/**
 * Process received v1 packet
 */
int meshgrid_v1_process_packet(const uint8_t *packet, size_t len,
                                int16_t rssi, int8_t snr) {
    if (!v1_initialized) {
        meshgrid_v1_bridge_init();  /* Auto-initialize on first use */
    }

    if (len < 1) {
        return -1;
    }

    /* Parse header */
    uint8_t header = packet[0];
    uint8_t version = MESHGRID_GET_VERSION(header);
    uint8_t payload_type = MESHGRID_GET_TYPE(header);
    uint8_t route_type = MESHGRID_GET_ROUTE(header);

    /* Check if this is a v1 packet */
    if (version != 1) {
        return -1;  /* Not v1, let v0 handle it */
    }

    /* Handle v1 text messages and group messages */
    if (payload_type != PAYLOAD_TXT_MSG && payload_type != PAYLOAD_GRP_TXT) {
        DEBUG_INFOF("[v1] Ignoring packet type=%d (not TXT_MSG or GRP_MSG)", payload_type);
        return -1;
    }

    /* Parse v1 packet: [header][nonce(12)][ciphertext][tag(16)] */
    if (len < 1 + 12 + 8 + 16) {  /* header + nonce + min_payload + tag */
        DEBUG_WARN("[v1] Packet too short");
        return -1;
    }

    int pos = 1;
    const uint8_t *nonce = &packet[pos];
    pos += 12;

    int ciphertext_len = len - pos - 16;
    if (ciphertext_len <= 0) {
        return -1;
    }

    const uint8_t *ciphertext = &packet[pos];
    pos += ciphertext_len;

    const uint8_t *tag = &packet[pos];

    uint8_t plaintext[200];
    bool decrypted = false;
    struct meshgrid_neighbor *sender = nullptr;
    uint8_t channel_hash = 0;

    /* Try to decrypt based on packet type */
    if (payload_type == PAYLOAD_TXT_MSG) {
        /* Direct message - decrypt with neighbor secrets */
        DEBUG_INFOF("[v1] RX: Direct message, trying to decrypt with %d neighbors", neighbor_count);
        int tried = 0;
        for (int i = 0; i < neighbor_count && !decrypted; i++) {
            if (!neighbors[i].secret_valid) {
                DEBUG_INFOF("[v1] RX: Skip neighbor %d (hash=0x%02x) - secret_valid=false", i, neighbors[i].hash);
                continue;
            }
            if (neighbors[i].protocol_version < 1) {
                DEBUG_INFOF("[v1] RX: Skip neighbor %d (hash=0x%02x) - protocol_version=%d",
                           i, neighbors[i].hash, neighbors[i].protocol_version);
                continue;
            }

            DEBUG_INFOF("[v1] RX: Trying neighbor %d (hash=0x%02x, name=%s)", i, neighbors[i].hash, neighbors[i].name);
            tried++;
            if (meshgrid_v1_aes_gcm_decrypt(neighbors[i].shared_secret, nonce, nullptr, 0,
                                             ciphertext, ciphertext_len, tag, plaintext) == 0) {
                decrypted = true;
                sender = &neighbors[i];
                DEBUG_INFOF("[v1] RX: Successfully decrypted with neighbor 0x%02x", sender->hash);
            }
        }

        if (!decrypted) {
            DEBUG_WARNF("[v1] Decryption failed (tried %d neighbors, no matching secret)", tried);
            return -1;
        }
    } else if (payload_type == PAYLOAD_GRP_TXT) {
        /* Channel message - decrypt with channel secrets */
        extern struct channel_entry custom_channels[];
        extern int custom_channel_count;

        DEBUG_INFOF("[v1] RX: Channel message, trying %d channels", custom_channel_count);
        for (int i = 0; i < custom_channel_count && !decrypted; i++) {
            if (!custom_channels[i].valid) continue;

            DEBUG_INFOF("[v1] RX: Trying channel %d (hash=0x%02x, name=%s)",
                       i, custom_channels[i].hash, custom_channels[i].name);

            if (meshgrid_v1_aes_gcm_decrypt(custom_channels[i].secret, nonce, nullptr, 0,
                                             ciphertext, ciphertext_len, tag, plaintext) == 0) {
                decrypted = true;
                channel_hash = custom_channels[i].hash;
                DEBUG_INFOF("[v1] RX: Successfully decrypted channel message on 0x%02x", channel_hash);
            }
        }

        if (!decrypted) {
            DEBUG_WARN("[v1] Channel decryption failed (no matching channel secret)");
            return -1;
        }
    }

    /* Parse decrypted payload based on type */
    pos = 0;
    char text_buf[128];
    uint16_t src_hash = 0;
    char sender_name[17] = "unknown";

    if (payload_type == PAYLOAD_TXT_MSG) {
        /* Direct message: [dest_hash(2)][src_hash(2)][sequence(4)][timestamp(4)][text] */
        if (ciphertext_len < 12) {
            return -1;
        }

        uint16_t dest_hash = ((uint16_t)plaintext[pos] << 8) | plaintext[pos + 1];
        pos += 2;
        src_hash = ((uint16_t)plaintext[pos] << 8) | plaintext[pos + 1];
        pos += 2;
        uint32_t sequence = ((uint32_t)plaintext[pos] << 24) |
                            ((uint32_t)plaintext[pos + 1] << 16) |
                            ((uint32_t)plaintext[pos + 2] << 8) |
                            plaintext[pos + 3];
        pos += 4;
        uint32_t timestamp = ((uint32_t)plaintext[pos] << 24) |
                             ((uint32_t)plaintext[pos + 1] << 16) |
                             ((uint32_t)plaintext[pos + 2] << 8) |
                             plaintext[pos + 3];
        pos += 4;

        /* Verify sequence number (replay protection) */
        if (sequence <= sender->last_seq_rx) {
            DEBUG_WARNF("[v1] Replay detected: seq=%lu <= last=%lu", sequence, sender->last_seq_rx);
            return -1;
        }
        sender->last_seq_rx = sequence;

        /* Extract text */
        int text_len = ciphertext_len - pos;
        if (text_len > 127) text_len = 127;
        memcpy(text_buf, &plaintext[pos], text_len);
        text_buf[text_len] = '\0';

        strncpy(sender_name, sender->name, 16);
        sender_name[16] = '\0';

        /* Store in direct messages */
        int idx = direct_msg_index;
        direct_messages[idx].valid = true;
        direct_messages[idx].decrypted = true;
        direct_messages[idx].timestamp = timestamp;  /* Use sender's timestamp */
        direct_messages[idx].sender_hash = sender->hash;
        direct_messages[idx].channel_hash = 0;  /* 0 = direct message */
        direct_messages[idx].protocol_version = 1;  /* v1 protocol */
        strncpy(direct_messages[idx].sender_name, sender_name, 16);
        direct_messages[idx].sender_name[16] = '\0';
        strncpy(direct_messages[idx].text, text_buf, 127);
        direct_messages[idx].text[127] = '\0';

        direct_msg_index = (direct_msg_index + 1) % DIRECT_MESSAGE_BUFFER_SIZE;
        if (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) direct_msg_count++;

    } else if (payload_type == PAYLOAD_GRP_TXT) {
        /* Channel message: [channel_hash(1)][src_hash(2)][timestamp(4)][text] */
        if (ciphertext_len < 7) {
            return -1;
        }

        uint8_t msg_channel_hash = plaintext[pos++];
        src_hash = ((uint16_t)plaintext[pos] << 8) | plaintext[pos + 1];
        pos += 2;
        uint32_t timestamp = ((uint32_t)plaintext[pos] << 24) |
                             ((uint32_t)plaintext[pos + 1] << 16) |
                             ((uint32_t)plaintext[pos + 2] << 8) |
                             plaintext[pos + 3];
        pos += 4;

        /* Find sender by v1 hash */
        for (int i = 0; i < neighbor_count; i++) {
            uint16_t n_hash_v1 = meshgrid_v1_hash_pubkey(neighbors[i].pubkey);
            if (n_hash_v1 == src_hash) {
                sender = &neighbors[i];
                strncpy(sender_name, sender->name, 16);
                sender_name[16] = '\0';
                break;
            }
        }

        /* Extract text */
        int text_len = ciphertext_len - pos;
        if (text_len > 127) text_len = 127;
        memcpy(text_buf, &plaintext[pos], text_len);
        text_buf[text_len] = '\0';

        /* Store in channel messages */
        extern struct message_entry channel_messages[][CHANNEL_MESSAGE_BUFFER_SIZE];
        extern int channel_msg_count[];
        extern int channel_msg_index[];

        /* Find channel index */
        extern struct channel_entry custom_channels[];
        extern int custom_channel_count;
        int ch_idx = -1;
        for (int i = 0; i < custom_channel_count; i++) {
            if (custom_channels[i].valid && custom_channels[i].hash == channel_hash) {
                ch_idx = i;
                break;
            }
        }

        if (ch_idx >= 0) {
            int idx = channel_msg_index[ch_idx];
            channel_messages[ch_idx][idx].valid = true;
            channel_messages[ch_idx][idx].decrypted = true;
            channel_messages[ch_idx][idx].timestamp = timestamp;  /* Use sender's timestamp */
            channel_messages[ch_idx][idx].sender_hash = sender ? sender->hash : (src_hash & 0xFF);
            channel_messages[ch_idx][idx].channel_hash = channel_hash;
            channel_messages[ch_idx][idx].protocol_version = 1;  /* v1 protocol */
            strncpy(channel_messages[ch_idx][idx].sender_name, sender_name, 16);
            channel_messages[ch_idx][idx].sender_name[16] = '\0';
            strncpy(channel_messages[ch_idx][idx].text, text_buf, 127);
            channel_messages[ch_idx][idx].text[127] = '\0';

            channel_msg_index[ch_idx] = (channel_msg_index[ch_idx] + 1) % CHANNEL_MESSAGE_BUFFER_SIZE;
            if (channel_msg_count[ch_idx] < CHANNEL_MESSAGE_BUFFER_SIZE) channel_msg_count[ch_idx]++;
        }
    }

    DEBUG_INFOF("[v1] RX: Message processed successfully (type=%d)", payload_type);
    return 0;
}
