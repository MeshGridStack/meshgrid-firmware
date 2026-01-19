/**
 * meshgrid v1 Epidemic Gossip for OTA
 *
 * Chunks spread through network like an epidemic:
 * - Nodes broadcast STATUS (which chunks they have)
 * - Nodes REQUEST missing chunks from neighbors
 * - Nodes re-broadcast received chunks
 * - Eventually all nodes have all chunks
 */

#ifndef MESHGRID_V1_OTA_GOSSIP_H
#define MESHGRID_V1_OTA_GOSSIP_H

#include <stdint.h>
#include <stdbool.h>
#include "manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHGRID_OTA_STATUS_INTERVAL   30000   /* Broadcast status every 30s */
#define MESHGRID_OTA_REQUEST_INTERVAL  10000   /* Request chunks every 10s */
#define MESHGRID_OTA_MAX_CHUNK_REQUESTS 5      /* Request up to 5 chunks at once */

/* Gossip state for OTA session */
struct meshgrid_ota_gossip_state {
    uint32_t session_id;
    uint8_t *chunk_bitmap;         /* Bitmap of received chunks (dynamically allocated) */
    uint32_t chunks_received;
    uint32_t total_chunks;
    uint32_t last_status_broadcast;
    uint32_t last_request_time;
    bool active;
};

void meshgrid_ota_gossip_init(struct meshgrid_ota_gossip_state *state,
                               uint32_t session_id, uint32_t total_chunks);
void meshgrid_ota_gossip_free(struct meshgrid_ota_gossip_state *state);
bool meshgrid_ota_gossip_should_broadcast_status(
    struct meshgrid_ota_gossip_state *state, uint32_t now);
int meshgrid_ota_gossip_find_missing_chunks(
    struct meshgrid_ota_gossip_state *state,
    uint16_t *chunk_list, int max_chunks);
bool meshgrid_ota_gossip_should_rebroadcast_chunk(
    struct meshgrid_ota_gossip_state *state, uint16_t chunk_index);
void meshgrid_ota_gossip_mark_chunk_received(
    struct meshgrid_ota_gossip_state *state, uint16_t chunk_index);
bool meshgrid_ota_gossip_is_complete(const struct meshgrid_ota_gossip_state *state);

#ifdef __cplusplus
}
#endif

#endif
