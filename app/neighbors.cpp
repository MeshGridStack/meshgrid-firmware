/**
 * Neighbor table management
 */

#include "neighbors.h"
#include <Arduino.h>
#include <string.h>

extern "C" {
#include "drivers/crypto/crypto.h"
}

/* Externed from main.cpp */
extern uint32_t stat_clients;
extern uint32_t stat_repeaters;
extern uint32_t stat_rooms;
extern uint32_t last_activity_time;
extern struct meshgrid_state mesh;

/* Neighbor table */
struct meshgrid_neighbor neighbors[MAX_NEIGHBORS];
uint8_t neighbor_count = 0;

struct meshgrid_neighbor *neighbor_find(uint8_t hash) {
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbors[i].hash == hash) {
            return &neighbors[i];
        }
    }
    return nullptr;
}

enum meshgrid_node_type infer_node_type(const char *name) {
    if (strncmp(name, "rpt-", 4) == 0 || strncmp(name, "RPT", 3) == 0 ||
        strstr(name, "relay") || strstr(name, "Relay") ||
        strstr(name, "repeater") || strstr(name, "Repeater")) {
        return NODE_TYPE_REPEATER;
    }
    if (strncmp(name, "room-", 5) == 0 || strncmp(name, "Room", 4) == 0 ||
        strstr(name, "server") || strstr(name, "Server")) {
        return NODE_TYPE_ROOM;
    }
    /* Default to client for user-like names */
    return NODE_TYPE_CLIENT;
}

enum meshgrid_firmware infer_firmware(const char *name) {
    if (strncmp(name, "mg-", 3) == 0 || strncmp(name, "MG-", 3) == 0) {
        return FW_MESHGRID;
    }
    if (strncmp(name, "Meshtastic", 10) == 0 || strstr(name, "!")) {
        return FW_MESHTASTIC;
    }
    /* Most nodes are MeshCore */
    return FW_MESHCORE;
}

void neighbor_update(const uint8_t *pubkey, const char *name,
                     uint32_t timestamp, int16_t rssi, int8_t snr,
                     uint8_t hops) {
    uint8_t hash = crypto_hash_pubkey(pubkey);
    struct meshgrid_neighbor *n = neighbor_find(hash);
    bool is_new = false;

    if (n == nullptr) {
        is_new = true;
        /* Add new neighbor */
        if (neighbor_count >= MAX_NEIGHBORS) {
            /* Table full - remove oldest */
            uint32_t oldest_time = neighbors[0].last_seen;
            int oldest_idx = 0;
            for (int i = 1; i < neighbor_count; i++) {
                if (neighbors[i].last_seen < oldest_time) {
                    oldest_time = neighbors[i].last_seen;
                    oldest_idx = i;
                }
            }
            /* Decrement stat for old node type */
            switch (neighbors[oldest_idx].node_type) {
                case NODE_TYPE_CLIENT: if (stat_clients > 0) stat_clients--; break;
                case NODE_TYPE_REPEATER: if (stat_repeaters > 0) stat_repeaters--; break;
                case NODE_TYPE_ROOM: if (stat_rooms > 0) stat_rooms--; break;
                default: break;
            }
            n = &neighbors[oldest_idx];
        } else {
            n = &neighbors[neighbor_count++];
        }

        memcpy(n->pubkey, pubkey, MESHGRID_PUBKEY_SIZE);
        n->hash = hash;
        strncpy(n->name, name, MESHGRID_NODE_NAME_MAX);
        n->name[MESHGRID_NODE_NAME_MAX] = '\0';
        n->node_type = infer_node_type(name);
        n->firmware = infer_firmware(name);
        n->hops = hops;

        /* Calculate and cache shared secret (ECDH) - like MeshCore does */
        crypto_key_exchange(n->shared_secret, mesh.privkey, pubkey);
        n->secret_valid = true;

        /* Update type stats */
        switch (n->node_type) {
            case NODE_TYPE_CLIENT: stat_clients++; break;
            case NODE_TYPE_REPEATER: stat_repeaters++; break;
            case NODE_TYPE_ROOM: stat_rooms++; break;
            default: break;
        }
    }

    n->last_seen = millis();
    n->advert_timestamp = timestamp;
    n->rssi = rssi;
    n->snr = snr;
    if (hops < n->hops) n->hops = hops;  /* Track shortest path */
    last_activity_time = millis();
}

const uint8_t *neighbor_get_shared_secret(uint8_t hash) {
    struct meshgrid_neighbor *n = neighbor_find(hash);
    if (n && n->secret_valid) {
        return n->shared_secret;
    }
    return nullptr;
}
