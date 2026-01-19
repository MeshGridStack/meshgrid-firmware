/**
 * Advertisement v0 - MeshCore Integration Adapter Implementation
 */

#include "advert_v0.h"

// Use C bridge to avoid namespace conflicts
extern "C" {
    void meshcore_bridge_send_advert(void);
}

/**
 * Send v0 advertisement via MeshCore
 */
void advert_v0_send(uint8_t route_type) {
    // Use MeshCore's advertisement sending via C bridge
    meshcore_bridge_send_advert();
}

/**
 * Handle received v0 advertisement via MeshCore
 * Note: MeshCore automatically processes advertisements via callbacks
 * This function is kept for compatibility but most work is done by MeshCore
 */
void advert_v0_receive(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr) {
    // Advertisement handling is done automatically by MeshCore via callbacks
    // when MeshCoreIntegration::handle_received_packet() is called
    // This function exists for compatibility with the existing architecture
}
