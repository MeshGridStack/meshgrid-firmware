/**
 * MeshCore v0 Bridge - Implementation
 *
 * This file can safely include both meshcore_integration.h (with namespace mesh)
 * because it doesn't declare 'extern struct meshgrid_state mesh'.
 */

#include "meshcore_bridge.h"
#include "meshcore_integration.h"

extern "C" {

void meshcore_bridge_initialize(void) {
    MeshCoreIntegration::initialize();
}

void meshcore_bridge_loop(void) {
    MeshCoreIntegration::loop();
}

void meshcore_bridge_handle_packet(uint8_t* buf, int len, int16_t rssi, int8_t snr) {
    MeshCoreIntegration::handle_received_packet(buf, len, rssi, snr);
}

void meshcore_bridge_send_text(uint8_t dest_hash, const char* text) {
    MeshCoreIntegration::send_text_message(dest_hash, text);
}

void meshcore_bridge_send_channel(uint8_t channel_hash, const uint8_t* channel_secret, const char* text,
                                  const char* channel_name) {
    MeshCoreIntegration::send_channel_message(channel_hash, channel_secret, text, channel_name);
}

void meshcore_bridge_send_advert(void) {
    MeshCoreIntegration::send_advert();
}

} // extern "C"
