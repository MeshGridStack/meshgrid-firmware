/**
 * MeshCore v0 Bridge - C-compatible interface
 *
 * This header provides a C-compatible interface to MeshCore integration
 * without exposing the C++ namespace mesh, avoiding conflicts with
 * the global variable 'mesh'.
 */

#ifndef MESHCORE_BRIDGE_H
#define MESHCORE_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize MeshCore v0
 */
void meshcore_bridge_initialize(void);

/**
 * Process MeshCore packets (call in loop())
 */
void meshcore_bridge_loop(void);

/**
 * Handle received packet
 */
void meshcore_bridge_handle_packet(uint8_t* buf, int len, int16_t rssi, int8_t snr);

/**
 * Send direct text message
 */
void meshcore_bridge_send_text(uint8_t dest_hash, const char* text);

/**
 * Send channel message
 */
void meshcore_bridge_send_channel(uint8_t channel_hash, const uint8_t* channel_secret, const char* text,
                                  const char* channel_name);

/**
 * Send advertisement
 */
void meshcore_bridge_send_advert(void);

#ifdef __cplusplus
}
#endif

#endif // MESHCORE_BRIDGE_H
