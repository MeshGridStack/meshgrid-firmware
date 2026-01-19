/**
 * Protocol Version Selector
 *
 * Automatically selects v0 (MeshCore) or v1 (meshgrid enhanced) protocol
 * based on peer capabilities.
 *
 * Strategy:
 * - If both peers support v1 → use v1 (enhanced security)
 * - Otherwise → use v0 (MeshCore compatibility)
 *
 * Default: Always v0 (until v1 is fully integrated in Phase 6)
 */

#ifndef MESHGRID_PROTOCOL_SELECTOR_H
#define MESHGRID_PROTOCOL_SELECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize protocol selector
 */
void protocol_selector_init(void);

/**
 * Send text message (auto-select v0 or v1)
 *
 * @param dest_hash  Destination node hash (1-byte)
 * @param text       Message text
 * @param len        Length of text
 *
 * @return 0 on success, -1 on error
 */
int protocol_selector_send_text(uint8_t dest_hash, const char *text, size_t len);

/**
 * Send channel message (auto-select v0 or v1)
 *
 * @param channel_hash  Channel hash (1-byte)
 * @param text          Message text
 * @param len           Length of text
 *
 * @return 0 on success, -1 on error
 */
int protocol_selector_send_channel(uint8_t channel_hash, const char *text, size_t len);

/**
 * Process received packet (try v1 first, fall back to v0)
 *
 * @param packet  Raw packet data
 * @param len     Packet length
 * @param rssi    Signal strength
 * @param snr     Signal-to-noise ratio
 *
 * @return 0 if handled, -1 if not recognized
 */
int protocol_selector_process_packet(const uint8_t *packet, size_t len,
                                      int16_t rssi, int8_t snr);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_PROTOCOL_SELECTOR_H */
