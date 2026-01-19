/**
 * Protocol Version Selector Implementation
 *
 * Phase 5: Integration layer created but v1 disabled
 * - Currently always uses v0 (MeshCore)
 * - v1 infrastructure exists but returns "not supported"
 * - This ensures v0 continues working
 *
 * Phase 6 TODO: Enable v1 when both peers support it
 */

#include "protocol_selector.h"
#include "meshgrid_v1_bridge.h"
#include "../meshcore_integration.h"

/**
 * Initialize protocol selector
 */
void protocol_selector_init(void) {
    /* Initialize v1 bridge (stub for now) */
    meshgrid_v1_bridge_init();

    /* v0 (MeshCore) already initialized by meshcore_integration */
}

/**
 * Send text message (auto-select v0 or v1)
 */
int protocol_selector_send_text(uint8_t dest_hash, const char *text, size_t len) {
    /* Check if peer supports v1 */
    if (meshgrid_v1_peer_supports_v1(dest_hash)) {
        /* Try v1 first */
        uint16_t dest_hash_v1 = dest_hash;  /* Convert 1-byte to 2-byte */
        int result = meshgrid_v1_send_text(dest_hash_v1, text, len);
        if (result == 0) {
            return 0;  /* v1 succeeded */
        }
    }

    /* Fall back to v0 (MeshCore) */
    /* NOTE: v0 send is handled by existing code in messaging.cpp */
    /* This function is currently not called, but ready for Phase 6 */
    return -1;  /* Let existing v0 code handle it */
}

/**
 * Send channel message (auto-select v0 or v1)
 */
int protocol_selector_send_channel(uint8_t channel_hash, const char *text, size_t len) {
    /* Try v1 first */
    int result = meshgrid_v1_send_channel(channel_hash, text, len);
    if (result == 0) {
        return 0;  /* v1 succeeded */
    }

    /* Fall back to v0 (MeshCore) */
    return -1;  /* Let existing v0 code handle it */
}

/**
 * Process received packet (try v1 first, fall back to v0)
 */
int protocol_selector_process_packet(const uint8_t *packet, size_t len,
                                      int16_t rssi, int8_t snr) {
    /* Try v1 first */
    int result = meshgrid_v1_process_packet(packet, len, rssi, snr);
    if (result == 0) {
        return 0;  /* v1 handled it */
    }

    /* Not a v1 packet, let v0 handle it */
    /* NOTE: v0 processing happens in existing meshcore_bridge code */
    return -1;  /* Not handled by selector, use v0 */
}
