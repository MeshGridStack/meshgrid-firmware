/**
 * Neighbor table management
 */

#ifndef MESHGRID_NEIGHBORS_H
#define MESHGRID_NEIGHBORS_H

#include <stdint.h>
#include "utils/memory.h"

extern "C" {
#include "network/protocol.h"
}

/* Neighbor table */
extern struct meshgrid_neighbor neighbors[MAX_NEIGHBORS];
extern uint16_t neighbor_count;  /* uint16_t to support 512 neighbors */

/* Find neighbor by hash */
struct meshgrid_neighbor *neighbor_find(uint8_t hash);

/* Update or add neighbor */
void neighbor_update(const uint8_t *pubkey, const char *name,
                     uint32_t timestamp, int16_t rssi, int8_t snr,
                     uint8_t hops, uint8_t protocol_version);

/* Get cached shared secret for neighbor (returns nullptr if not found/valid) */
const uint8_t *neighbor_get_shared_secret(uint8_t hash);

/* Infer node type from name */
enum meshgrid_node_type infer_node_type(const char *name);

/* Infer firmware from name */
enum meshgrid_firmware infer_firmware(const char *name);

/* Save neighbors to NVS (up to 10 with valid secrets) */
void neighbors_save_to_nvs(void);

/* Load neighbors from NVS on boot */
void neighbors_load_from_nvs(void);

/* Prune stale neighbors (not seen for NEIGHBOR_TIMEOUT) */
void neighbors_prune_stale(void);

#endif /* MESHGRID_NEIGHBORS_H */
