/**
 * Neighbor table management
 */

#include "neighbors.h"
#include <Arduino.h>
#include <string.h>
#include <Preferences.h>

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
uint16_t neighbor_count = 0;

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

        /* Initialize sequence counters for Protocol v1 */
        n->last_seq_rx = 0;
        n->next_seq_tx = 1;  /* Start at 1 (0 reserved for handshake) */

        /* Update type stats */
        switch (n->node_type) {
            case NODE_TYPE_CLIENT: stat_clients++; break;
            case NODE_TYPE_REPEATER: stat_repeaters++; break;
            case NODE_TYPE_ROOM: stat_rooms++; break;
            default: break;
        }
    }

    /* Update name if it changed (neighbor might have renamed their device) */
    if (strcmp(n->name, name) != 0) {
        strncpy(n->name, name, MESHGRID_NODE_NAME_MAX);
        n->name[MESHGRID_NODE_NAME_MAX] = '\0';

        /* Re-infer firmware and node type based on new name */
        n->firmware = infer_firmware(name);
        n->node_type = infer_node_type(name);
    }

    n->last_seen = millis();
    n->advert_timestamp = timestamp;
    n->rssi = rssi;
    n->snr = snr;
    if (hops < n->hops) n->hops = hops;  /* Track shortest path */
    last_activity_time = millis();

    /* Save neighbors to NVS when new neighbor added (batched - save every 5 new neighbors) */
    static uint8_t neighbors_since_save = 0;
    if (is_new) {
        neighbors_since_save++;
        if (neighbors_since_save >= 5) {
            neighbors_save_to_nvs();
            neighbors_since_save = 0;
        }
    }
}

const uint8_t *neighbor_get_shared_secret(uint8_t hash) {
    struct meshgrid_neighbor *n = neighbor_find(hash);
    if (n && n->secret_valid) {
        return n->shared_secret;
    }
    return nullptr;
}

void neighbors_save_to_nvs(void) {
    Preferences prefs;
    prefs.begin("neighbors", false);  // Read-write

    /* Set NVS format version */
    prefs.putUChar("version", 1);

    /* Save up to 10 most recent neighbors with valid secrets */
    uint8_t saved_count = 0;
    for (int i = 0; i < neighbor_count && saved_count < 10; i++) {
        if (neighbors[i].secret_valid) {
            char key[16];
            snprintf(key, sizeof(key), "n%d_hash", saved_count);
            prefs.putUChar(key, neighbors[i].hash);

            snprintf(key, sizeof(key), "n%d_pubkey", saved_count);
            prefs.putBytes(key, neighbors[i].pubkey, MESHGRID_PUBKEY_SIZE);

            snprintf(key, sizeof(key), "n%d_name", saved_count);
            prefs.putString(key, neighbors[i].name);

            /* NOTE: We do NOT store shared_secret in NVS for security.
             * Shared secrets are recalculated from public keys on boot.
             * This prevents physical compromise from leaking all secrets. */

            saved_count++;
        }
    }

    prefs.putUChar("count", saved_count);
    prefs.end();
}

void neighbors_load_from_nvs(void) {
    Preferences prefs;
    prefs.begin("neighbors", false);  // Read-write for version check

    /* Check NVS format version - clear if incompatible */
    uint8_t nvs_version = prefs.getUChar("version", 0);
    if (nvs_version != 1) {
        Serial.println("Incompatible neighbor NVS format, clearing...");
        prefs.clear();
        prefs.putUChar("version", 1);
        prefs.end();
        return;
    }

    uint8_t saved_count = prefs.getUChar("count", 0);
    if (saved_count > 10) saved_count = 10;  // Sanity check

    Serial.print("Loading ");
    Serial.print(saved_count);
    Serial.println(" neighbors from NVS...");

    for (uint8_t i = 0; i < saved_count; i++) {
        if (neighbor_count >= MAX_NEIGHBORS) break;

        struct meshgrid_neighbor *n = &neighbors[neighbor_count];

        char key[16];
        snprintf(key, sizeof(key), "n%d_hash", i);
        n->hash = prefs.getUChar(key, 0);
        if (n->hash == 0) continue;  // Invalid

        snprintf(key, sizeof(key), "n%d_pubkey", i);
        if (prefs.getBytes(key, n->pubkey, MESHGRID_PUBKEY_SIZE) != MESHGRID_PUBKEY_SIZE) continue;

        snprintf(key, sizeof(key), "n%d_name", i);
        String name = prefs.getString(key, "");
        if (name.length() == 0) continue;
        strncpy(n->name, name.c_str(), MESHGRID_NODE_NAME_MAX);
        n->name[MESHGRID_NODE_NAME_MAX] = '\0';

        /* Recalculate shared secret from public key (don't load from NVS for security) */
        crypto_key_exchange(n->shared_secret, mesh.privkey, n->pubkey);
        n->secret_valid = true;
        n->last_seen = millis();  // Mark as old
        n->node_type = infer_node_type(n->name);
        n->firmware = infer_firmware(n->name);

        neighbor_count++;

        Serial.print("  Restored: ");
        Serial.print(n->name);
        Serial.print(" (0x");
        Serial.print(n->hash, HEX);
        Serial.println(")");
    }

    prefs.end();
}
