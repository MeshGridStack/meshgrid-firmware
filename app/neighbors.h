/**
 * Neighbor table management
 */

#ifndef MESHGRID_NEIGHBORS_H
#define MESHGRID_NEIGHBORS_H

#include <stdint.h>

extern "C" {
#include "net/protocol.h"
}

#define MAX_NEIGHBORS 32

/* Neighbor table */
extern struct meshgrid_neighbor neighbors[MAX_NEIGHBORS];
extern uint8_t neighbor_count;

/* Find neighbor by hash */
struct meshgrid_neighbor *neighbor_find(uint8_t hash);

/* Update or add neighbor */
void neighbor_update(const uint8_t *pubkey, const char *name,
                     uint32_t timestamp, int16_t rssi, int8_t snr,
                     uint8_t hops);

/* Get cached shared secret for neighbor (returns nullptr if not found/valid) */
const uint8_t *neighbor_get_shared_secret(uint8_t hash);

/* Infer node type from name */
enum meshgrid_node_type infer_node_type(const char *name);

/* Infer firmware from name */
enum meshgrid_firmware infer_firmware(const char *name);

#endif /* MESHGRID_NEIGHBORS_H */
