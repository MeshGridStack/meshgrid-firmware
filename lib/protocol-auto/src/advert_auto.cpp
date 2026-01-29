/**
 * Protocol Auto - Advertisement Implementation
 *
 * Automatically dispatches to v0 or v1 advertisement handlers based on build flags:
 * - PROTOCOL_V0_ENABLED: Enable v0 (MeshCore) protocol
 * - PROTOCOL_V1_ENABLED: Enable v1 (meshgrid enhanced) protocol
 */

#include "advert_auto.h"
#include <Arduino.h>

/* Need full struct definition */
extern "C" {
#include "network/protocol.h"
}

/* Protocol version availability */
#ifndef PROTOCOL_V0_ENABLED
#    define PROTOCOL_V0_ENABLED 1 /* v0 enabled by default */
#endif

#ifndef PROTOCOL_V1_ENABLED
#    define PROTOCOL_V1_ENABLED 0 /* v1 disabled by default */
#endif

/* Protocol-specific advertisement headers */
#if PROTOCOL_V0_ENABLED
#    include "advert_v0.h"
#endif

#if PROTOCOL_V1_ENABLED
extern "C" {
#    include "../../meshgrid-v1/src/integration/meshgrid_v1_bridge.h"
}
#endif

/**
 * Initialize advertisement system
 */
void advert_auto_init(void) {
#if PROTOCOL_V1_ENABLED
    meshgrid_v1_init();
#endif
    /* v0 needs no initialization */
}

/**
 * Send advertisement (auto-selects protocol)
 */
void advert_auto_send(uint8_t route_type) {
#if PROTOCOL_V1_ENABLED
    /* Use v1 protocol if enabled */
    meshgrid_v1_send_advert(route_type);
#elif PROTOCOL_V0_ENABLED
    /* Fall back to v0 if v1 not enabled */
    advert_v0_send(route_type);
#else
#    error "At least one protocol (v0 or v1) must be enabled"
#endif
}

/**
 * Handle received advertisement (auto-detects protocol version)
 * Keeps v0 signature for backwards compatibility
 */
void advert_auto_receive(struct meshgrid_packet* pkt, int16_t rssi, int8_t snr) {
    if (pkt == NULL) {
        return;
    }

    /* Check protocol version */
    uint8_t version = pkt->version;

#if PROTOCOL_V1_ENABLED
    if (version == 1) {
        /* v1 protocol packet - encode back to buffer for v1 handler */
        uint8_t buf[256];
        int len = meshgrid_packet_encode(pkt, buf, sizeof(buf));
        if (len > 0) {
            meshgrid_v1_receive_advert(buf, len, rssi, snr);
        }
        /* TODO: Add error logging for failed encoding */
        return;
    }
#endif

#if PROTOCOL_V0_ENABLED
    if (version == 0) {
        /* v0 protocol packet - handled by MeshCore callbacks */
        /* This function exists for compatibility */
        return;
    }
#endif

    /* Unknown protocol version */
}
