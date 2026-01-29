/**
 * meshgrid v1 Traceroute (2-byte hashes)
 *
 * Enhanced traceroute for v1 protocol using 2-byte node hashes
 * instead of 1-byte hashes for better collision resistance.
 */

#ifndef MESHGRID_V1_TRACEROUTE_H
#define MESHGRID_V1_TRACEROUTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../protocol/packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Traceroute callbacks (same as v0) */
struct meshgrid_v1_trace_callbacks {
    int (*transmit)(const uint8_t *buf, size_t len);
    bool (*queue_add)(const uint8_t *buf, int len, uint32_t delay_ms, uint8_t priority);
    void (*inc_packets_tx)(void);
    void (*inc_packets_fwd)(void);
};

/**
 * Handle v1 traceroute packet (2-byte hashes)
 *
 * @param pkt Packet to handle
 * @param our_hash_v1 Our 2-byte node hash
 * @param rssi Signal strength
 * @param snr Signal-to-noise ratio
 * @param callbacks Application callbacks
 * @return true if handled, false otherwise
 */
bool meshgrid_handle_trace_v1(
    struct meshgrid_packet *pkt,
    uint16_t our_hash_v1,
    int16_t rssi,
    int8_t snr,
    struct meshgrid_v1_trace_callbacks *callbacks
);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_TRACEROUTE_H */
