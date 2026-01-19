#include "gossip.h"
#include <stdlib.h>
#include <string.h>

void meshgrid_ota_gossip_init(struct meshgrid_ota_gossip_state *state,
                               uint32_t session_id, uint32_t total_chunks) {
    state->session_id = session_id;
    state->total_chunks = total_chunks;
    state->chunks_received = 0;
    state->last_status_broadcast = 0;
    state->last_request_time = 0;
    state->active = true;
    
    /* Allocate bitmap (1 bit per chunk) */
    uint32_t bitmap_bytes = (total_chunks + 7) / 8;
    state->chunk_bitmap = (uint8_t*)calloc(bitmap_bytes, 1);
}

void meshgrid_ota_gossip_free(struct meshgrid_ota_gossip_state *state) {
    if (state->chunk_bitmap) {
        free(state->chunk_bitmap);
        state->chunk_bitmap = NULL;
    }
    state->active = false;
}

bool meshgrid_ota_gossip_should_broadcast_status(
    struct meshgrid_ota_gossip_state *state, uint32_t now) {
    if (!state->active) return false;
    return (now - state->last_status_broadcast) >= MESHGRID_OTA_STATUS_INTERVAL;
}

int meshgrid_ota_gossip_find_missing_chunks(
    struct meshgrid_ota_gossip_state *state,
    uint16_t *chunk_list, int max_chunks) {
    int found = 0;
    for (uint32_t i = 0; i < state->total_chunks && found < max_chunks; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        if ((state->chunk_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
            chunk_list[found++] = i;
        }
    }
    return found;
}

bool meshgrid_ota_gossip_should_rebroadcast_chunk(
    struct meshgrid_ota_gossip_state *state, uint16_t chunk_index) {
    if (chunk_index >= state->total_chunks) return false;
    uint32_t byte_idx = chunk_index / 8;
    uint8_t bit_idx = chunk_index % 8;
    /* Rebroadcast if we just received it (not already in bitmap) */
    return (state->chunk_bitmap[byte_idx] & (1 << bit_idx)) == 0;
}

void meshgrid_ota_gossip_mark_chunk_received(
    struct meshgrid_ota_gossip_state *state, uint16_t chunk_index) {
    if (chunk_index >= state->total_chunks) return;
    uint32_t byte_idx = chunk_index / 8;
    uint8_t bit_idx = chunk_index % 8;
    
    /* Set bit if not already set */
    if ((state->chunk_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
        state->chunk_bitmap[byte_idx] |= (1 << bit_idx);
        state->chunks_received++;
    }
}

bool meshgrid_ota_gossip_is_complete(const struct meshgrid_ota_gossip_state *state) {
    return state->chunks_received >= state->total_chunks;
}
