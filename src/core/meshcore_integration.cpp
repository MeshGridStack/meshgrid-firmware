/**
 * MeshCore v0 Integration Implementation
 */

// Use accessor functions to avoid namespace conflict with 'mesh'
extern "C" {
#include "network/protocol.h"
#include "hardware/crypto/crypto.h"
#include "core/mesh_accessor.h"

// Radio functions from radio_api.cpp
int16_t radio_transmit(uint8_t* data, size_t len);
int16_t radio_start_receive(void);
}

// External globals from main.cpp
extern bool radio_in_rx_mode;
extern void led_blink(void);

// Now safe to include headers that bring in namespace mesh
#include "meshcore_integration.h"
#include "neighbors.h"
#include "messaging.h"
#include "utils/debug.h"
#include "utils/types.h"
#include "utils/memory.h"
#include "../radio/radio_hal.h"

// Message storage (from main.cpp) - declare after includes so constants are defined
extern struct message_entry direct_messages[];
extern int direct_msg_index, direct_msg_count;
extern struct message_entry public_messages[];
extern int public_msg_index, public_msg_count;
extern struct message_entry channel_messages[][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[];
extern int channel_msg_count[];
extern uint8_t public_channel_hash;
extern uint8_t public_channel_secret[32];
extern struct channel_entry custom_channels[];
extern int custom_channel_count;

namespace MeshCoreIntegration {

// ========================================================================
// Forward Declarations
// ========================================================================

int callback_find_channel_by_hash(uint8_t hash, mesh::GroupChannel channels[], int max_matches);

// ========================================================================
// Global Adapter Instances
// ========================================================================

MeshgridRadio* radio_adapter = nullptr;
MeshgridClock* clock_adapter = nullptr;
MeshgridRNG* rng_adapter = nullptr;
MeshgridRTC* rtc_adapter = nullptr;
MeshgridPacketManager* packet_manager = nullptr;
MeshgridTables* tables_adapter = nullptr;
MeshgridMesh* mesh_v0 = nullptr;

MeshgridCallbacks callbacks = {.get_shared_secret = callback_get_shared_secret,
                               .find_neighbor = callback_find_neighbor,
                               .update_neighbor = callback_update_neighbor,
                               .store_direct_message = callback_store_direct_message,
                               .store_channel_message = callback_store_channel_message,
                               .find_channel_by_hash = callback_find_channel_by_hash,
                               .radio_transmit = callback_radio_transmit,
                               .radio_start_receive = callback_radio_start_receive,
                               .led_blink = callback_led_blink,
                               .increment_tx = callback_increment_tx,
                               .increment_rx = callback_increment_rx};

// ========================================================================
// Callback Implementations
// ========================================================================

const uint8_t* callback_get_shared_secret(uint8_t hash) {
    return neighbor_get_shared_secret(hash);
}

void* callback_find_neighbor(uint8_t hash) {
    return neighbor_find(hash);
}

void callback_update_neighbor(const uint8_t* pubkey, const char* name, uint32_t timestamp, int16_t rssi, int8_t snr,
                              uint8_t hops, uint8_t protocol_version) {
    DEBUG_INFOF("[MeshCore] callback_update_neighbor: name=%s, rssi=%d, snr=%d, hops=%d", name, rssi, snr, hops);
    neighbor_update(pubkey, name, timestamp, rssi, snr, hops, protocol_version);
}

void callback_store_direct_message(const char* sender_name, uint8_t sender_hash, const char* text, uint32_t timestamp) {
    int idx = direct_msg_index;
    direct_messages[idx].valid = true;
    direct_messages[idx].decrypted = true;
    direct_messages[idx].sender_hash = sender_hash;
    direct_messages[idx].channel_hash = 0;
    direct_messages[idx].protocol_version = 0;
    direct_messages[idx].timestamp = timestamp;

    strncpy(direct_messages[idx].sender_name, sender_name, 16);
    direct_messages[idx].sender_name[16] = '\0';

    strncpy(direct_messages[idx].text, text, 127);
    direct_messages[idx].text[127] = '\0';

    direct_msg_index = (direct_msg_index + 1) % DIRECT_MESSAGE_BUFFER_SIZE;
    if (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE)
        direct_msg_count++;

    DEBUG_INFOF("RX MSG v0 from %s: %s", sender_name, text);
}

void callback_store_channel_message(uint8_t channel_hash, const char* sender_name, const char* text,
                                    uint32_t timestamp) {
    // Check if this is the public channel
    if (channel_hash == public_channel_hash) {
        // Store in public messages buffer
        int idx = public_msg_index;
        public_messages[idx].valid = true;
        public_messages[idx].decrypted = true;
        public_messages[idx].sender_hash = 0;
        public_messages[idx].channel_hash = channel_hash;
        public_messages[idx].protocol_version = 0;
        public_messages[idx].timestamp = timestamp;

        strncpy(public_messages[idx].sender_name, sender_name, 16);
        public_messages[idx].sender_name[16] = '\0';

        strncpy(public_messages[idx].text, text, 127);
        public_messages[idx].text[127] = '\0';

        public_msg_index = (public_msg_index + 1) % PUBLIC_MESSAGE_BUFFER_SIZE;
        if (public_msg_count < PUBLIC_MESSAGE_BUFFER_SIZE)
            public_msg_count++;

        DEBUG_INFOF("RX GRP v0 [Public] %s: %s", sender_name, text);
    } else {
        // Find custom channel by hash
        int channel_idx = -1;
        for (int i = 0; i < custom_channel_count; i++) {
            if (custom_channels[i].valid && custom_channels[i].hash == channel_hash) {
                channel_idx = i;
                break;
            }
        }

        if (channel_idx >= 0) {
            // Store in custom channel messages buffer
            int idx = channel_msg_index[channel_idx];
            channel_messages[channel_idx][idx].valid = true;
            channel_messages[channel_idx][idx].decrypted = true;
            channel_messages[channel_idx][idx].sender_hash = 0;
            channel_messages[channel_idx][idx].channel_hash = channel_hash;
            channel_messages[channel_idx][idx].protocol_version = 0;
            channel_messages[channel_idx][idx].timestamp = timestamp;

            strncpy(channel_messages[channel_idx][idx].sender_name, sender_name, 16);
            channel_messages[channel_idx][idx].sender_name[16] = '\0';

            strncpy(channel_messages[channel_idx][idx].text, text, 127);
            channel_messages[channel_idx][idx].text[127] = '\0';

            channel_msg_index[channel_idx] = (channel_msg_index[channel_idx] + 1) % CHANNEL_MESSAGE_BUFFER_SIZE;
            if (channel_msg_count[channel_idx] < CHANNEL_MESSAGE_BUFFER_SIZE)
                channel_msg_count[channel_idx]++;

            DEBUG_INFOF("RX GRP v0 [%s] %s: %s", custom_channels[channel_idx].name, sender_name, text);
        } else {
            DEBUG_WARNF("RX GRP v0: Unknown channel 0x%02x, message dropped", channel_hash);
        }
    }
}

int16_t callback_radio_transmit(uint8_t* data, size_t len) {
    // Use existing radio API
    return radio_transmit(data, len);
}

int16_t callback_radio_start_receive() {
    return radio_start_receive();
}

void callback_led_blink() {
    ::led_blink();
}

void callback_increment_tx() {
    mesh_increment_tx();
}

void callback_increment_rx() {
    mesh_increment_rx();
}

int callback_find_channel_by_hash(uint8_t hash, mesh::GroupChannel channels[], int max_matches) {
    int found = 0;

    // Check public channel
    if (public_channel_hash == hash && found < max_matches) {
        channels[found].hash[0] = public_channel_hash;
        memcpy(channels[found].secret, public_channel_secret, 32);
        DEBUG_INFO("[MeshCore] callback_find_channel_by_hash: found PUBLIC channel");
        found++;
    }

    // Check custom channels
    for (int i = 0; i < custom_channel_count && found < max_matches; i++) {
        if (custom_channels[i].valid && custom_channels[i].hash == hash) {
            channels[found].hash[0] = custom_channels[i].hash;
            memcpy(channels[found].secret, custom_channels[i].secret, 32);
            DEBUG_INFOF("[MeshCore] callback_find_channel_by_hash: found CUSTOM channel '%s'", custom_channels[i].name);
            found++;
        }
    }

    return found;
}

// ========================================================================
// Initialization
// ========================================================================

void initialize() {
    DEBUG_INFO("Initializing MeshCore v0...");

    // Create adapter instances
    radio_adapter = new MeshgridRadio(&callbacks);
    clock_adapter = new MeshgridClock();
    rng_adapter = new MeshgridRNG();
    rtc_adapter = new MeshgridRTC();
    packet_manager = new MeshgridPacketManager();
    tables_adapter = new MeshgridTables();

    // Create mesh instance
    mesh_v0 = new MeshgridMesh(*radio_adapter, *clock_adapter, *rng_adapter, *rtc_adapter, *packet_manager,
                               *tables_adapter, &callbacks, radio_adapter);

    // Initialize mesh
    mesh_v0->begin();

    // Load identity from meshgrid state
    mesh_v0->self_id.readFrom(mesh_get_privkey(), MESHGRID_PRIVKEY_SIZE);

    DEBUG_INFOF("MeshCore v0 initialized: name=%s, hash=0x%02x", mesh_get_name(), mesh_get_privkey()[0]);
    DEBUG_INFO("MeshCore v0 initialization complete");
}

void loop() {
    if (mesh_v0) {
        mesh_v0->loop();
    }
}

void handle_received_packet(uint8_t* buf, int len, int16_t rssi, int8_t snr) {
    if (!mesh_v0 || !radio_adapter)
        return;

    // Notify radio adapter of reception
    radio_adapter->notifyPacketReceived((float)rssi, (float)snr);

    // Parse packet
    mesh::Packet* pkt = packet_manager->allocNew();
    if (!pkt) {
        DEBUG_WARN("MeshCore: Packet pool exhausted");
        return;
    }

    if (!pkt->readFrom(buf, len)) {
        packet_manager->free(pkt);
        DEBUG_WARN("MeshCore: Failed to parse packet");
        return;
    }

    // Set metadata
    pkt->_snr = (int8_t)(snr * 4.0f); // MeshCore uses SNR*4

    DEBUG_INFOF("[MeshCore] RX packet: len=%d, type=0x%02x, rssi=%d, snr=%d", len, pkt->getPayloadType(), rssi, snr);

    // Process packet through MeshCore
    // The Dispatcher will handle it via the loop() call
    packet_manager->queueInbound(pkt, millis());
}

// ========================================================================
// Public API
// ========================================================================

void send_text_message(uint8_t dest_hash, const char* text) {
    if (mesh_v0) {
        mesh_v0->sendTextMessage(dest_hash, text);
    }
}

void send_channel_message(uint8_t channel_hash, const uint8_t* channel_secret, const char* text,
                          const char* channel_name) {
    if (mesh_v0) {
        mesh_v0->sendChannelMessage(channel_hash, channel_secret, text, channel_name);
    }
}

void send_advert() {
    if (mesh_v0) {
        // Create advertisement with MeshCore protocol format
        // Format: [flags(1)][optional fields][name (NOT null-terminated)]
        uint8_t app_data[32];
        int i = 0;

        // Flags byte: 0x80 (name) | 0x20 (feat1) | 0x01 (chat/client)
        // Use feat1 field to signal v1 capability instead of bit 0x08
        // This fixes MeshCore app compatibility (type must be 0-4, not 9)
#if PROTOCOL_V1_ENABLED
        uint8_t flags = 0x80 | 0x20 | 0x01; // 0xA1 = name + feat1 + chat
        app_data[i++] = flags;

        // feat1 field (2 bytes): bit 0 = v1 capable
        app_data[i++] = 0x01; // v1 capability bit
        app_data[i++] = 0x00; // reserved

        DEBUG_INFOF("[MeshCore] Creating advert with name: %s (flags=0x%02x, feat1=0x0001, v1=yes, len=%d)",
                    mesh_get_name(), flags, i);
#else
        uint8_t flags = 0x80 | 0x01; // 0x81 = name + chat (no v1)
        app_data[i++] = flags;

        DEBUG_INFOF("[MeshCore] Creating advert with name: %s (flags=0x%02x, v1=no, len=%d)", mesh_get_name(), flags,
                    i);
#endif

        // Name (remainder, NOT null-terminated per MeshCore protocol)
        const char* name = mesh_get_name();
        int name_len = strlen(name);
        if (name_len > 16)
            name_len = 16;
        memcpy(&app_data[i], name, name_len);
        i += name_len;

        mesh::Packet* pkt = mesh_v0->createAdvert(mesh_v0->self_id, app_data, i);
        if (pkt) {
            DEBUG_INFO("[MeshCore] Advert packet created, sending...");
            mesh_v0->sendFlood(pkt, (uint32_t)0);
        } else {
            DEBUG_WARN("[MeshCore] Failed to create advert packet!");
        }
    }
}

} // namespace MeshCoreIntegration
