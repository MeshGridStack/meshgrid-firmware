/**
 * Message receiving interface
 * Provides wrappers for protocol library receive handlers
 */

#ifndef MESSAGING_RECEIVE_H
#define MESSAGING_RECEIVE_H

#include <Arduino.h>
#include "utils/types.h"

/* Note: Don't include meshcore_integration.h here due to namespace conflict
 * Message handling is done automatically by MeshCore via callbacks */

/* Advertisement handling (not protocol-specific) */
void handle_advert(struct meshgrid_packet* pkt, int16_t rssi, int8_t snr);

/*
 * Direct message and group message handlers are now handled automatically
 * by MeshCore via callbacks. Messages are processed through
 * MeshCoreIntegration::handle_received_packet() which calls:
 * - callback_store_direct_message() for direct messages
 * - callback_store_channel_message() for channel messages
 */

#endif /* MESSAGING_RECEIVE_H */
