/**
 * Protocol Auto - Advertisement Version Selection
 *
 * Automatically selects advertisement protocol version:
 * - v1: Enhanced with bloom filters for fast multi-hop discovery
 * - v0: MeshCore compatible for legacy devices
 */

#ifndef ADVERT_AUTO_H
#define ADVERT_AUTO_H

#include <stdint.h>

/**
 * Initialize advertisement system
 * Sets up bloom filters if v1 is enabled
 */
void advert_auto_init(void);

/**
 * Send advertisement with automatic version selection
 * - Uses v1 (with bloom filters) if enabled
 * - Falls back to v0 (MeshCore compatible) otherwise
 *
 * route_type: ROUTE_DIRECT (local) or ROUTE_FLOOD (network-wide)
 */
void advert_auto_send(uint8_t route_type);

/**
 * Handle received advertisement (any version)
 * Dispatches to appropriate protocol handler based on version field
 */
void advert_auto_receive(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr);

#endif // ADVERT_AUTO_H
