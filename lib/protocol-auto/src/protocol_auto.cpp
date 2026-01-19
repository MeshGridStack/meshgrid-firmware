/**
 * meshgrid Auto-Detection Protocol Wrapper Implementation
 * Redirects to MeshCore integration
 */

#include "protocol_auto.h"
#include <Arduino.h>

// Use C bridge to avoid namespace conflicts
extern "C" {
    void meshcore_bridge_send_text(uint8_t dest_hash, const char* text);
    void meshcore_bridge_send_channel(uint8_t channel_hash, const uint8_t* channel_secret,
                                      const char* text, const char* channel_name);
}

namespace ProtocolAuto {

/*
 * Send direct message using MeshCore v0
 */
void send_text_message(uint8_t dest_hash, const char *text) {
    /* Use MeshCore integration via C bridge */
    meshcore_bridge_send_text(dest_hash, text);
}

/*
 * Send channel message using MeshCore v0
 */
void send_channel_message(uint8_t channel_hash, const uint8_t *channel_secret,
                          const char *text, const char *channel_name) {
    /* Use MeshCore integration via C bridge */
    meshcore_bridge_send_channel(channel_hash, channel_secret, text, channel_name);
}

} // namespace ProtocolAuto
