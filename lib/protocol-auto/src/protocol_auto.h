/**
 * meshgrid Auto-Detection Protocol Wrapper
 * Automatically selects v0 or v1 based on context:
 * - Public channel: v0 (MeshCore compatibility)
 * - Custom channels: v1 (enhanced security)
 * - Direct messages: Auto-detect peer firmware version
 */

#ifndef PROTOCOL_AUTO_H
#define PROTOCOL_AUTO_H

#include <Arduino.h>
#include <stdint.h>

namespace ProtocolAuto {

/**
 * Send direct message - auto-detect protocol version
 * Checks peer firmware and uses appropriate protocol
 */
void send_text_message(uint8_t dest_hash, const char *text);

/**
 * Send channel message - auto-detect protocol version
 * Public channel: v0 (MeshCore compatibility)
 * Custom channels: v1 (enhanced security)
 */
void send_channel_message(uint8_t channel_hash, const uint8_t *channel_secret,
                          const char *text, const char *channel_name);

} // namespace ProtocolAuto

#endif /* PROTOCOL_AUTO_H */
