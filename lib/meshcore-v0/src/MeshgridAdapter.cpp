/**
 * MeshGrid Adapter Layer Implementation
 */

#include "MeshgridAdapter.h"
#include <Arduino.h>
#include <string.h>

extern "C" {
    void debug_printf(int level, const char *fmt, ...);
    // Forward declarations for channel management
    struct channel_entry;
}

// ============================================================================
// MeshgridRadio Implementation
// ============================================================================

MeshgridRadio::MeshgridRadio(MeshgridCallbacks* cb)
    : callbacks(cb), in_recv_mode(true), last_rssi(0), last_snr(0) {
}

void MeshgridRadio::begin() {
    in_recv_mode = true;
}

int MeshgridRadio::recvRaw(uint8_t* bytes, int sz) {
    // Reception is handled externally by meshgrid's radio interrupt
    // This method should not be called directly
    return 0;
}

uint32_t MeshgridRadio::getEstAirtimeFor(int len_bytes) {
    // Rough estimate for LoRa airtime (SF11, BW250)
    // ~100 bytes/sec, so 10ms per byte
    return len_bytes * 10;
}

float MeshgridRadio::packetScore(float snr, int packet_len) {
    // Score based on SNR (higher is better)
    // Range: 0.0 (worst) to 1.0 (best)
    return (snr + 10.0f) / 20.0f;
}

bool MeshgridRadio::startSendRaw(const uint8_t* bytes, int len) {
    debug_printf(2, "[MeshCore] startSendRaw: len=%d", len);
    if (callbacks && callbacks->radio_transmit) {
        int16_t result = callbacks->radio_transmit((uint8_t*)bytes, len);
        in_recv_mode = false;
        debug_printf(2, "[MeshCore] TX result=%d", result);
        return (result >= 0);
    }
    debug_printf(0, "[MeshCore] ERROR: No TX callback");
    return false;
}

bool MeshgridRadio::isSendComplete() {
    // Transmission is synchronous in meshgrid, so always complete
    return true;
}

void MeshgridRadio::onSendFinished() {
    if (callbacks && callbacks->radio_start_receive) {
        callbacks->radio_start_receive();
    }
    in_recv_mode = true;
}

bool MeshgridRadio::isInRecvMode() const {
    return in_recv_mode;
}

float MeshgridRadio::getLastRSSI() const {
    return last_rssi;
}

float MeshgridRadio::getLastSNR() const {
    return last_snr;
}

void MeshgridRadio::notifyPacketReceived(float rssi, float snr) {
    last_rssi = rssi;
    last_snr = snr;
}

// ============================================================================
// MeshgridClock Implementation
// ============================================================================

unsigned long MeshgridClock::getMillis() {
    return millis();
}

// ============================================================================
// MeshgridRNG Implementation
// ============================================================================

#ifdef ARDUINO_ARCH_ESP32
extern "C" void esp_fill_random(void* buf, size_t len);
#endif

void MeshgridRNG::random(uint8_t* dest, size_t sz) {
#ifdef ARDUINO_ARCH_ESP32
    // Use hardware RNG on ESP32
    esp_fill_random(dest, sz);
#else
    // Fallback to Arduino's random()
    for (size_t i = 0; i < sz; i++) {
        dest[i] = (uint8_t)random(256);
    }
#endif
}

// ============================================================================
// MeshgridRTC Implementation
// ============================================================================

uint32_t MeshgridRTC::getCurrentTime() {
    // Use uptime in seconds as fallback
    // meshgrid can override this with NTP time if available
    return millis() / 1000;
}

void MeshgridRTC::setCurrentTime(uint32_t time) {
    // Not implemented - meshgrid handles time separately
}

// ============================================================================
// MeshgridPacketManager Implementation
// ============================================================================

MeshgridPacketManager::MeshgridPacketManager() {
    // Initialize pool
    for (int i = 0; i < POOL_SIZE; i++) {
        packet_used[i] = false;
    }

    // Initialize queues
    for (int i = 0; i < OUTBOUND_QUEUE_SIZE; i++) {
        outbound_queue[i].valid = false;
    }
    for (int i = 0; i < INBOUND_QUEUE_SIZE; i++) {
        inbound_queue[i].valid = false;
    }
}

mesh::Packet* MeshgridPacketManager::allocNew() {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!packet_used[i]) {
            packet_used[i] = true;
            return &packet_pool[i];
        }
    }
    return nullptr;  // Pool exhausted
}

void MeshgridPacketManager::free(mesh::Packet* packet) {
    if (!packet) return;

    for (int i = 0; i < POOL_SIZE; i++) {
        if (&packet_pool[i] == packet) {
            packet_used[i] = false;
            return;
        }
    }
}

void MeshgridPacketManager::queueOutbound(mesh::Packet* packet, uint8_t priority, uint32_t scheduled_for) {
    debug_printf(3, "[MeshCore] queueOutbound: pri=%d, sched=%lu", priority, scheduled_for);

    // Find empty slot
    for (int i = 0; i < OUTBOUND_QUEUE_SIZE; i++) {
        if (!outbound_queue[i].valid) {
            outbound_queue[i].packet = packet;
            outbound_queue[i].priority = priority;
            outbound_queue[i].scheduled_for = scheduled_for;
            outbound_queue[i].valid = true;
            debug_printf(3, "[MeshCore] Queued in slot %d", i);
            return;
        }
    }
    // Queue full - drop oldest packet
    free(outbound_queue[0].packet);
    for (int i = 0; i < OUTBOUND_QUEUE_SIZE - 1; i++) {
        outbound_queue[i] = outbound_queue[i + 1];
    }
    outbound_queue[OUTBOUND_QUEUE_SIZE - 1].packet = packet;
    outbound_queue[OUTBOUND_QUEUE_SIZE - 1].priority = priority;
    outbound_queue[OUTBOUND_QUEUE_SIZE - 1].scheduled_for = scheduled_for;
    outbound_queue[OUTBOUND_QUEUE_SIZE - 1].valid = true;
}

mesh::Packet* MeshgridPacketManager::getNextOutbound(uint32_t now) {
    int best_idx = -1;
    uint8_t best_priority = 255;

    // Find highest priority packet that's ready to send
    for (int i = 0; i < OUTBOUND_QUEUE_SIZE; i++) {
        if (outbound_queue[i].valid &&
            outbound_queue[i].scheduled_for <= now &&
            outbound_queue[i].priority < best_priority) {
            best_priority = outbound_queue[i].priority;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        mesh::Packet* pkt = outbound_queue[best_idx].packet;
        outbound_queue[best_idx].valid = false;
        debug_printf(3, "[MeshCore] getNextOutbound: idx=%d, pri=%d, now=%lu", best_idx, best_priority, now);
        return pkt;
    }

    debug_printf(3, "[MeshCore] getNextOutbound: no packets ready, now=%lu", now);
    return nullptr;
}

int MeshgridPacketManager::getOutboundCount(uint32_t now) const {
    int count = 0;
    for (int i = 0; i < OUTBOUND_QUEUE_SIZE; i++) {
        if (outbound_queue[i].valid && outbound_queue[i].scheduled_for <= now) {
            count++;
        }
    }
    return count;
}

int MeshgridPacketManager::getFreeCount() const {
    int count = 0;
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!packet_used[i]) count++;
    }
    return count;
}

mesh::Packet* MeshgridPacketManager::getOutboundByIdx(int i) {
    if (i >= 0 && i < OUTBOUND_QUEUE_SIZE && outbound_queue[i].valid) {
        return outbound_queue[i].packet;
    }
    return nullptr;
}

mesh::Packet* MeshgridPacketManager::removeOutboundByIdx(int i) {
    if (i >= 0 && i < OUTBOUND_QUEUE_SIZE && outbound_queue[i].valid) {
        mesh::Packet* pkt = outbound_queue[i].packet;
        outbound_queue[i].valid = false;
        return pkt;
    }
    return nullptr;
}

void MeshgridPacketManager::queueInbound(mesh::Packet* packet, uint32_t scheduled_for) {
    // Find empty slot
    for (int i = 0; i < INBOUND_QUEUE_SIZE; i++) {
        if (!inbound_queue[i].valid) {
            inbound_queue[i].packet = packet;
            inbound_queue[i].scheduled_for = scheduled_for;
            inbound_queue[i].valid = true;
            return;
        }
    }
    // Queue full - drop oldest
    free(inbound_queue[0].packet);
    for (int i = 0; i < INBOUND_QUEUE_SIZE - 1; i++) {
        inbound_queue[i] = inbound_queue[i + 1];
    }
    inbound_queue[INBOUND_QUEUE_SIZE - 1].packet = packet;
    inbound_queue[INBOUND_QUEUE_SIZE - 1].scheduled_for = scheduled_for;
    inbound_queue[INBOUND_QUEUE_SIZE - 1].valid = true;
}

mesh::Packet* MeshgridPacketManager::getNextInbound(uint32_t now) {
    for (int i = 0; i < INBOUND_QUEUE_SIZE; i++) {
        if (inbound_queue[i].valid && inbound_queue[i].scheduled_for <= now) {
            mesh::Packet* pkt = inbound_queue[i].packet;
            inbound_queue[i].valid = false;
            return pkt;
        }
    }
    return nullptr;
}

// ============================================================================
// MeshgridTables Implementation
// ============================================================================

MeshgridTables::MeshgridTables() : seen_index(0) {
    for (int i = 0; i < MESHCORE_SEEN_TABLE_SIZE; i++) {
        seen_table[i].valid = false;
    }
}

bool MeshgridTables::hasSeen(const mesh::Packet* packet) {
    if (!packet) return true;

    // Calculate packet hash
    uint8_t hash[8];
    packet->calculatePacketHash(hash);

    uint32_t now = millis();

    // Check if we've seen this packet recently (within 60 seconds)
    for (int i = 0; i < MESHCORE_SEEN_TABLE_SIZE; i++) {
        if (seen_table[i].valid &&
            (now - seen_table[i].timestamp) < 60000 &&
            memcmp(seen_table[i].hash, hash, 8) == 0) {
            return true;  // Seen before
        }
    }

    // Not seen - add to table
    seen_table[seen_index].valid = true;
    memcpy(seen_table[seen_index].hash, hash, 8);
    seen_table[seen_index].timestamp = now;
    seen_index = (seen_index + 1) % MESHCORE_SEEN_TABLE_SIZE;

    return false;  // Not seen
}

void MeshgridTables::clear(const mesh::Packet* packet) {
    if (!packet) return;

    uint8_t hash[8];
    packet->calculatePacketHash(hash);

    // Remove from seen table
    for (int i = 0; i < MESHCORE_SEEN_TABLE_SIZE; i++) {
        if (seen_table[i].valid && memcmp(seen_table[i].hash, hash, 8) == 0) {
            seen_table[i].valid = false;
            return;
        }
    }
}

// ============================================================================
// MeshgridMesh Implementation
// ============================================================================

MeshgridMesh::MeshgridMesh(mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng,
                           mesh::RTCClock& rtc, mesh::PacketManager& mgr, mesh::MeshTables& tables,
                           MeshgridCallbacks* cb, MeshgridRadio* radio_adpt)
    : mesh::Mesh(radio, ms, rng, rtc, mgr, tables), callbacks(cb), radio_adapter(radio_adpt) {
}

int MeshgridMesh::searchPeersByHash(const uint8_t* hash) {
    if (!callbacks || !callbacks->find_neighbor) return 0;

    // Store hash for use in getPeerSharedSecret
    last_searched_hash = hash[0];

    void* neighbor = callbacks->find_neighbor(hash[0]);
    return neighbor ? 1 : 0;
}

void MeshgridMesh::getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) {
    if (!callbacks || !callbacks->get_shared_secret) return;

    // Get the shared secret using the hash from searchPeersByHash
    const uint8_t* secret = callbacks->get_shared_secret(last_searched_hash);
    if (secret) {
        memcpy(dest_secret, secret, 32);  // Copy 32-byte shared secret
        debug_printf(1, "[MeshCore] getPeerSharedSecret: copied secret for hash=0x%02x", last_searched_hash);
    } else {
        debug_printf(1, "[MeshCore] getPeerSharedSecret: no secret found for hash=0x%02x", last_searched_hash);
    }
}

void MeshgridMesh::onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx,
                                  const uint8_t* secret, uint8_t* data, size_t len) {
    if (!callbacks) return;

    // Extract sender hash from packet payload
    if (packet->payload_len < 2) return;
    uint8_t sender_hash = packet->payload[1];

    // Get sender name
    char sender_name[17] = "Unknown";
    if (callbacks->find_neighbor) {
        void* neighbor = callbacks->find_neighbor(sender_hash);
        if (neighbor) {
            // Assuming neighbor struct has name field at offset 33
            // This is fragile - would be better with proper struct access
            strncpy(sender_name, (char*)neighbor + 33, 16);
            sender_name[16] = '\0';
        }
    }

    // data format: [timestamp(4)][txt_type(1)][text]
    if (len < 5) return;
    const char* text = (const char*)&data[5];
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    // Store message via callback
    if (type == 2 && callbacks->store_direct_message) {  // PAYLOAD_TYPE_TXT_MSG
        callbacks->store_direct_message(sender_name, sender_hash, text, timestamp);
    }
}

void MeshgridMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                                uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
    static uint32_t advert_recv_count = 0;
    advert_recv_count++;

    debug_printf(1, "[MeshCore] onAdvertRecv CALLED #%lu, app_data_len=%d",
                 advert_recv_count, app_data_len);

    if (!callbacks || !callbacks->update_neighbor) {
        debug_printf(1, "[MeshCore] onAdvertRecv: callbacks NULL or update_neighbor NULL!");
        return;
    }

    // Extract name from app_data (MeshCore format)
    // app_data format: [flags(1)][optional fields][name (remainder, NOT null-terminated)]
    char name[17] = {0};
    if (app_data_len > 0) {
        uint8_t flags = app_data[0];
        int i = 1;  // Skip flags byte

        // Debug: print app_data hex dump
        char hex_dump[100];
        int hex_pos = 0;
        for (int j = 0; j < app_data_len && j < 20 && hex_pos < 90; j++) {
            hex_pos += snprintf(&hex_dump[hex_pos], 90 - hex_pos, "%02x ", app_data[j]);
        }
        debug_printf(1, "[MeshCore] app_data[%d]: %s", app_data_len, hex_dump);

        // Skip optional fields based on flags
        if (flags & 0x10) i += 8;  // ADV_LATLON_MASK - lat/lon (8 bytes)
        if (flags & 0x20) i += 2;  // ADV_FEAT1_MASK - feature 1 (2 bytes)
        if (flags & 0x40) i += 2;  // ADV_FEAT2_MASK - feature 2 (2 bytes)

        debug_printf(1, "[MeshCore] flags=0x%02x, name_offset=%d", flags, i);

        // Name is remainder of app_data (if name bit is set)
        if ((flags & 0x80) && i < app_data_len) {  // ADV_NAME_MASK
            int name_len = app_data_len - i;
            if (name_len > 16) name_len = 16;
            memcpy(name, &app_data[i], name_len);
            name[name_len] = '\0';  // Add null terminator for C string
        }
    }

    // Calculate hops from path length
    uint8_t hops = packet->path_len;

    // Update neighbor table
    // Get actual RSSI/SNR from radio adapter
    int16_t rssi = radio_adapter ? (int16_t)radio_adapter->getLastRSSI() : -120;
    int8_t snr = packet->_snr / 4; // Convert from SNR*4 to SNR

    debug_printf(1, "[MeshCore] onAdvertRecv: name='%s', rssi=%d, snr=%d, hops=%d, hash=0x%02x",
                 name, rssi, snr, hops, id.pub_key[0]);

    callbacks->update_neighbor(id.pub_key, name, timestamp, rssi, snr, hops, 0);
}

int MeshgridMesh::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
    debug_printf(1, "[MeshCore] searchChannelsByHash: looking for hash=0x%02x", hash[0]);

    // Use callback to search for channels by hash
    // This avoids needing to include application headers in the library
    if (callbacks && callbacks->find_channel_by_hash) {
        return callbacks->find_channel_by_hash(hash[0], channels, max_matches);
    }

    debug_printf(1, "[MeshCore] searchChannelsByHash: no callback, returning 0");
    return 0;
}

void MeshgridMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                                   const mesh::GroupChannel& channel, uint8_t* data, size_t len) {
    debug_printf(1, "[MeshCore] onGroupDataRecv CALLED: type=%d, channel_hash=0x%02x, len=%d",
                 type, channel.hash[0], len);

    if (!callbacks || !callbacks->store_channel_message) {
        debug_printf(1, "[MeshCore] onGroupDataRecv: callbacks NULL!");
        return;
    }

    // data format: [timestamp(4)][txt_type(1)]["sender: text"]
    if (len < 5) {
        debug_printf(1, "[MeshCore] onGroupDataRecv: len too short (%d < 5)", len);
        return;
    }
    const char* msg_text = (const char*)&data[5];
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    // Extract sender name from "sender: message" format
    char sender_name[17] = {0};
    const char* colon = strchr(msg_text, ':');
    if (colon) {
        int name_len = colon - msg_text;
        if (name_len > 16) name_len = 16;
        memcpy(sender_name, msg_text, name_len);
        sender_name[name_len] = '\0';

        // Skip to actual message text
        msg_text = colon + 2;  // Skip ": "
    }

    debug_printf(1, "[MeshCore] onGroupDataRecv: sender='%s', text='%s'", sender_name, msg_text);

    // Store channel message
    callbacks->store_channel_message(channel.hash[0], sender_name, msg_text, timestamp);
}

bool MeshgridMesh::allowPacketForward(const mesh::Packet* packet) {
    // Only forward if device mode is REPEATER
    // This should be configurable via callbacks
    return false;  // Default: don't forward
}

void MeshgridMesh::logTx(mesh::Packet* packet, int len) {
    // Increment TX counter when packet is successfully transmitted
    if (callbacks && callbacks->increment_tx) {
        callbacks->increment_tx();
    }
}

void MeshgridMesh::logRx(mesh::Packet* packet, int len, float score) {
    // Increment RX counter when packet is received
    if (callbacks && callbacks->increment_rx) {
        callbacks->increment_rx();
    }
}

void MeshgridMesh::sendTextMessage(uint8_t dest_hash, const char* text) {
    // Find neighbor to get shared secret
    if (!callbacks || !callbacks->get_shared_secret) return;

    const uint8_t* secret = callbacks->get_shared_secret(dest_hash);
    if (!secret) return;

    // Build identity for destination (just hash for now)
    mesh::Identity dest;
    dest.pub_key[0] = dest_hash;

    // Create datagram
    uint8_t data[256];
    int i = 0;

    // Timestamp (4 bytes)
    uint32_t timestamp = millis() / 1000;
    memcpy(&data[i], &timestamp, 4);
    i += 4;

    // Text type (1 byte)
    data[i++] = 0;

    // Message text
    int text_len = strlen(text);
    memcpy(&data[i], text, text_len);
    i += text_len;

    // Create packet
    mesh::Packet* pkt = createDatagram(2, dest, secret, data, i);
    if (pkt) {
        sendFlood(pkt, (uint32_t)0);

        if (callbacks->increment_tx) callbacks->increment_tx();
        if (callbacks->led_blink) callbacks->led_blink();
    }
}

void MeshgridMesh::sendChannelMessage(uint8_t channel_hash, const uint8_t* channel_secret,
                                     const char* text, const char* channel_name) {
    // Create group channel
    mesh::GroupChannel channel;
    channel.hash[0] = channel_hash;
    memcpy(channel.secret, channel_secret, 32);

    // Build data with sender prefix
    uint8_t data[256];
    int i = 0;

    // Timestamp (4 bytes)
    uint32_t timestamp = millis() / 1000;
    memcpy(&data[i], &timestamp, 4);
    i += 4;

    // Text type (1 byte)
    data[i++] = 0;

    // Message with sender prefix: "sender_name: text"
    // Get our name from self_id
    sprintf((char*)&data[i], "%s: %s", "meshgrid", text);  // TODO: use actual name
    i += strlen((char*)&data[i]);

    // Create packet
    mesh::Packet* pkt = createGroupDatagram(5, channel, data, i);
    if (pkt) {
        sendFlood(pkt, (uint32_t)0);

        if (callbacks->increment_tx) callbacks->increment_tx();
        if (callbacks->led_blink) callbacks->led_blink();
    }
}

void MeshgridMesh::sendAdvert() {
    // TODO: Implement advertisement sending
    // This requires accessing self_id and building advert packet
}
