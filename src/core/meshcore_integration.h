/**
 * MeshCore v0 Integration Helper
 *
 * This file provides helper functions and initialization code
 * for integrating the meshcore-v0 library into meshgrid-firmware.
 */

#ifndef MESHCORE_INTEGRATION_H
#define MESHCORE_INTEGRATION_H

#include <MeshgridAdapter.h>
#include <Arduino.h>

// Forward declarations of meshgrid types
struct meshgrid_state;
struct meshgrid_neighbor;
struct message_entry;

// Callback implementations for MeshCore integration
namespace MeshCoreIntegration {

// ========================================================================
// Callback Functions
// ========================================================================

/**
     * Get shared secret for a given node hash
     * Called by MeshCore when it needs to decrypt a message
     */
const uint8_t* callback_get_shared_secret(uint8_t hash);

/**
     * Find neighbor by hash
     * Called by MeshCore when it needs neighbor information
     */
void* callback_find_neighbor(uint8_t hash);

/**
     * Update neighbor table with advertisement data
     * Called by MeshCore when it receives an advertisement
     */
void callback_update_neighbor(const uint8_t* pubkey, const char* name, uint32_t timestamp, int16_t rssi, int8_t snr,
                              uint8_t hops, uint8_t protocol_version);

/**
     * Store received direct message
     * Called by MeshCore when it receives a decrypted direct message
     */
void callback_store_direct_message(const char* sender_name, uint8_t sender_hash, const char* text, uint32_t timestamp);

/**
     * Store received channel message
     * Called by MeshCore when it receives a decrypted channel message
     */
void callback_store_channel_message(uint8_t channel_hash, const char* sender_name, const char* text,
                                    uint32_t timestamp);

/**
     * Transmit packet via radio
     * Called by MeshCore when it wants to send a packet
     */
int16_t callback_radio_transmit(uint8_t* data, size_t len);

/**
     * Start radio receive mode
     * Called by MeshCore after transmission completes
     */
int16_t callback_radio_start_receive();

/**
     * Blink LED for feedback
     * Called by MeshCore on successful transmission
     */
void callback_led_blink();

/**
     * Increment TX counter
     * Called by MeshCore after successful transmission
     */
void callback_increment_tx();

/**
     * Increment RX counter
     * Called by MeshCore after successful reception
     */
void callback_increment_rx();

// ========================================================================
// Adapter Instances (Global)
// ========================================================================

extern MeshgridRadio* radio_adapter;
extern MeshgridClock* clock_adapter;
extern MeshgridRNG* rng_adapter;
extern MeshgridRTC* rtc_adapter;
extern MeshgridPacketManager* packet_manager;
extern MeshgridTables* tables_adapter;
extern MeshgridMesh* mesh_v0;
extern MeshgridCallbacks callbacks;

// ========================================================================
// Initialization Functions
// ========================================================================

/**
     * Initialize MeshCore v0 library
     * Call this in setup() after radio initialization
     */
void initialize();

/**
     * Process MeshCore loop
     * Call this in loop() to handle packet processing
     */
void loop();

/**
     * Notify MeshCore of received packet
     * Call this when radio interrupt receives a packet
     *
     * @param buf Packet data buffer
     * @param len Packet length
     * @param rssi Received signal strength
     * @param snr Signal-to-noise ratio
     */
void handle_received_packet(uint8_t* buf, int len, int16_t rssi, int8_t snr);

/**
     * Send direct text message via MeshCore
     *
     * @param dest_hash Destination node hash (1 byte)
     * @param text Message text (null-terminated)
     */
void send_text_message(uint8_t dest_hash, const char* text);

/**
     * Send channel text message via MeshCore
     *
     * @param channel_hash Channel hash (1 byte)
     * @param channel_secret Channel secret (32 bytes)
     * @param text Message text (null-terminated)
     * @param channel_name Channel name for logging
     */
void send_channel_message(uint8_t channel_hash, const uint8_t* channel_secret, const char* text,
                          const char* channel_name);

/**
     * Send advertisement
     * Call periodically to announce node presence
     */
void send_advert();

} // namespace MeshCoreIntegration

#endif // MESHCORE_INTEGRATION_H
