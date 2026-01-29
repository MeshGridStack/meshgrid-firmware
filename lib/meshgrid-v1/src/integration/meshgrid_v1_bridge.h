/**
 * meshgrid v1 Integration Bridge
 *
 * Bridges the pure C protocol library to the Arduino/C++ application
 * This is the ONLY file that accesses application state (mesh, neighbors, radio)
 */

#ifndef MESHGRID_V1_BRIDGE_H
#define MESHGRID_V1_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize v1 protocol system
 * Call this in setup()
 */
void meshgrid_v1_init(void);

/**
 * Send v1 advertisement
 *
 * @param route_type  ROUTE_DIRECT (local) or ROUTE_FLOOD (network-wide)
 */
void meshgrid_v1_send_advert(uint8_t route_type);

/**
 * Handle received v1 advertisement
 *
 * @param buf   Packet buffer
 * @param len   Packet length
 * @param rssi  Signal strength
 * @param snr   Signal-to-noise ratio
 */
void meshgrid_v1_receive_advert(const uint8_t *buf, size_t len, int16_t rssi, int8_t snr);

/**
 * Update beacon scheduling (call in loop())
 *
 * @return true if should send advertisement now
 */
bool meshgrid_v1_should_send_local(void);
bool meshgrid_v1_should_send_discovery(void);

/**
 * Mark beacon as sent
 */
void meshgrid_v1_local_sent(void);
void meshgrid_v1_discovery_sent(void);

/**
 * Trigger immediate discovery beacon (user-initiated)
 */
void meshgrid_v1_trigger_discovery(void);

/**
 * Handle v1 traceroute packet (2-byte hashes)
 * Included for completeness - typically called from messaging layer
 */
#include "../protocol/traceroute_v1.h"

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_BRIDGE_H */
