/**
 * meshgrid v1 Protocol Bridge
 *
 * Connects lib/meshgrid-v1 protocol to application.
 *
 * v1 Protocol Features:
 * - AES-256-GCM encryption
 * - 16-byte HMAC authentication
 * - 2-byte node hashes
 * - Replay protection (sequence numbers)
 * - Bloom filter discovery
 */

#ifndef MESHGRID_V1_BRIDGE_H
#define MESHGRID_V1_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize v1 protocol bridge
 *
 * Must be called after identity is loaded.
 */
void meshgrid_v1_bridge_init(void);

/**
 * Send text message using v1 protocol
 *
 * Uses AES-GCM encryption with sequence numbers.
 *
 * @param dest_hash  2-byte destination hash
 * @param text       Message text
 * @param len        Length of text
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_v1_send_text(uint16_t dest_hash, const char *text, size_t len);

/**
 * Send channel message using v1 protocol
 *
 * @param channel_hash  1-byte channel hash
 * @param text          Message text
 * @param len           Length of text
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_v1_send_channel(uint8_t channel_hash, const char *text, size_t len);

/**
 * Process received v1 packet
 *
 * Called by radio RX handler when v1 packet detected.
 *
 * @param packet  Raw packet data
 * @param len     Packet length
 * @param rssi    Signal strength
 * @param snr     Signal-to-noise ratio
 *
 * @return 0 if handled, -1 if not a v1 packet
 */
int meshgrid_v1_process_packet(const uint8_t *packet, size_t len,
                                int16_t rssi, int8_t snr);

/**
 * Check if neighbor supports v1 protocol
 *
 * @param hash  Node hash (1-byte or 2-byte)
 * @return true if supports v1, false otherwise
 */
bool meshgrid_v1_peer_supports_v1(uint8_t hash);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_BRIDGE_H */
