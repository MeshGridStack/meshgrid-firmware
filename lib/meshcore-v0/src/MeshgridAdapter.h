/**
 * MeshGrid Adapter Layer for MeshCore Protocol v0
 *
 * This adapter bridges MeshCore's clean callback-based architecture
 * with meshgrid-firmware's existing neighbor table, message storage,
 * and radio implementations.
 */

#pragma once

#include <Mesh.h>
#include <Dispatcher.h>
#include <Identity.h>
#include <Packet.h>
#include <Arduino.h>

// Forward declarations for meshgrid types
struct meshgrid_neighbor;
struct message_entry;

/**
 * Callback structure for meshgrid integration
 *
 * These function pointers allow MeshCore to interact with meshgrid's
 * neighbor table, message storage, and other firmware components
 * without tight coupling.
 */
struct MeshgridCallbacks {
    // Neighbor management
    const uint8_t* (*get_shared_secret)(uint8_t hash);
    void* (*find_neighbor)(uint8_t hash);
    void (*update_neighbor)(const uint8_t* pubkey, const char* name,
                           uint32_t timestamp, int16_t rssi, int8_t snr,
                           uint8_t hops, uint8_t protocol_version);

    // Message storage
    void (*store_direct_message)(const char* sender_name, uint8_t sender_hash,
                                 const char* text, uint32_t timestamp);
    void (*store_channel_message)(uint8_t channel_hash, const char* sender_name,
                                  const char* text, uint32_t timestamp);

    // Channel management
    int (*find_channel_by_hash)(uint8_t hash, mesh::GroupChannel channels[], int max_matches);

    // Radio control
    int16_t (*radio_transmit)(uint8_t* data, size_t len);
    int16_t (*radio_start_receive)(void);

    // LED/UI feedback
    void (*led_blink)(void);

    // Stats
    void (*increment_tx)(void);
    void (*increment_rx)(void);
};

/**
 * Radio adapter - implements mesh::Radio interface
 * for meshgrid's radio hardware
 */
class MeshgridRadio : public mesh::Radio {
private:
    MeshgridCallbacks* callbacks;
    bool in_recv_mode;
    float last_rssi;
    float last_snr;

public:
    MeshgridRadio(MeshgridCallbacks* cb);

    void begin() override;
    int recvRaw(uint8_t* bytes, int sz) override;
    uint32_t getEstAirtimeFor(int len_bytes) override;
    float packetScore(float snr, int packet_len) override;
    bool startSendRaw(const uint8_t* bytes, int len) override;
    bool isSendComplete() override;
    void onSendFinished() override;
    bool isInRecvMode() const override;
    float getLastRSSI() const override;
    float getLastSNR() const override;

    // Called by meshgrid when packet received
    void notifyPacketReceived(float rssi, float snr);
};

/**
 * Clock adapter - implements mesh::MillisecondClock
 */
class MeshgridClock : public mesh::MillisecondClock {
public:
    unsigned long getMillis() override;
};

/**
 * RNG adapter - implements mesh::RNG
 */
class MeshgridRNG : public mesh::RNG {
public:
    void random(uint8_t* dest, size_t sz) override;
};

/**
 * RTC adapter - implements mesh::RTCClock
 */
class MeshgridRTC : public mesh::RTCClock {
public:
    uint32_t getCurrentTime() override;
    void setCurrentTime(uint32_t time) override;
};

/**
 * Packet manager adapter - implements mesh::PacketManager
 *
 * Manages a static pool of packets for memory efficiency
 */
class MeshgridPacketManager : public mesh::PacketManager {
private:
    static const int POOL_SIZE = 16;
    mesh::Packet packet_pool[POOL_SIZE];
    bool packet_used[POOL_SIZE];

    struct OutboundEntry {
        mesh::Packet* packet;
        uint8_t priority;
        uint32_t scheduled_for;
        bool valid;
    };

    static const int OUTBOUND_QUEUE_SIZE = 8;
    OutboundEntry outbound_queue[OUTBOUND_QUEUE_SIZE];

    struct InboundEntry {
        mesh::Packet* packet;
        uint32_t scheduled_for;
        bool valid;
    };

    static const int INBOUND_QUEUE_SIZE = 4;
    InboundEntry inbound_queue[INBOUND_QUEUE_SIZE];

public:
    MeshgridPacketManager();

    mesh::Packet* allocNew() override;
    void free(mesh::Packet* packet) override;
    void queueOutbound(mesh::Packet* packet, uint8_t priority, uint32_t scheduled_for) override;
    mesh::Packet* getNextOutbound(uint32_t now) override;
    int getOutboundCount(uint32_t now) const override;
    int getFreeCount() const override;
    mesh::Packet* getOutboundByIdx(int i) override;
    mesh::Packet* removeOutboundByIdx(int i) override;
    void queueInbound(mesh::Packet* packet, uint32_t scheduled_for) override;
    mesh::Packet* getNextInbound(uint32_t now) override;
};

/**
 * Tables adapter - implements mesh::MeshTables for duplicate detection
 */
class MeshgridTables : public mesh::MeshTables {
private:
    struct SeenEntry {
        uint8_t hash[8];
        uint32_t timestamp;
        bool valid;
    };

    static const int MESHCORE_SEEN_TABLE_SIZE = 128;
    SeenEntry seen_table[MESHCORE_SEEN_TABLE_SIZE];
    int seen_index;

public:
    MeshgridTables();

    bool hasSeen(const mesh::Packet* packet) override;
    void clear(const mesh::Packet* packet) override;
};

/**
 * Main mesh adapter - extends mesh::Mesh with meshgrid-specific behavior
 */
class MeshgridMesh : public mesh::Mesh {
private:
    MeshgridCallbacks* callbacks;
    MeshgridRadio* radio_adapter;  // For accessing RSSI/SNR
    uint8_t last_searched_hash;     // Track hash from searchPeersByHash for getPeerSharedSecret

protected:
    // Implement virtual methods from mesh::Mesh
    int searchPeersByHash(const uint8_t* hash) override;
    void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
    void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx,
                       const uint8_t* secret, uint8_t* data, size_t len) override;
    void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                     uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;
    int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override;
    void onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                        const mesh::GroupChannel& channel, uint8_t* data, size_t len) override;
    bool allowPacketForward(const mesh::Packet* packet) override;

    // Override from mesh::Dispatcher to track TX/RX
    void logTx(mesh::Packet* packet, int len) override;
    void logRx(mesh::Packet* packet, int len, float score) override;

public:
    MeshgridMesh(mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng,
                 mesh::RTCClock& rtc, mesh::PacketManager& mgr, mesh::MeshTables& tables,
                 MeshgridCallbacks* cb, MeshgridRadio* radio_adpt = nullptr);

    // Helper methods for meshgrid firmware
    void sendTextMessage(uint8_t dest_hash, const char* text);
    void sendChannelMessage(uint8_t channel_hash, const uint8_t* channel_secret,
                           const char* text, const char* channel_name);
    void sendAdvert();
};
