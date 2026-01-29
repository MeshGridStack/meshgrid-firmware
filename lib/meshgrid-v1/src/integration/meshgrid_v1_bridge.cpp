/**
 * meshgrid v1 Advertisement Bridge
 *
 * Bridges v1 protocol library to application for advertisements.
 * This lives in lib/meshgrid-v1 to keep v1 stuff in v1.
 */

#include "meshgrid_v1_bridge.h"
#include <Arduino.h>
#include <string.h>

extern "C" {
#include "../discovery/bloom.h"
#include "../discovery/beacon.h"
#include "../discovery/trickle.h"
#include "../protocol/packet.h"

    /* External application state (from main.cpp) */
    /* Note: meshgrid_state and meshgrid_neighbor are defined in packet.h */
    extern struct meshgrid_state mesh;
    extern struct meshgrid_neighbor neighbors[];
    extern int neighbor_count;

    /* Time */
    struct rtc_time_t {
        bool valid;
        uint32_t epoch_at_boot;
    };
    extern struct rtc_time_t rtc_time;

    /* Radio */
    int16_t radio_transmit(uint8_t* data, size_t len);
}

/* V1 protocol state */
static struct meshgrid_bloom_set v1_bloom_filters;
static struct meshgrid_beacon_schedule v1_beacon_schedule;
static bool v1_advert_initialized = false;

/* Helper to get current Unix timestamp */
static uint32_t get_current_timestamp(void) {
    if (rtc_time.valid) {
        return rtc_time.epoch_at_boot + (millis() / 1000);
    }
    return millis() / 1000;
}

/**
 * Initialize v1 advertisement system
 */
void meshgrid_v1_init(void) {
    if (v1_advert_initialized) {
        return;
    }

    /* Initialize bloom filters */
    meshgrid_bloom_init(&v1_bloom_filters);

    /* Initialize beacon scheduling */
    meshgrid_beacon_init(&v1_beacon_schedule, millis());

    v1_advert_initialized = true;
}

/**
 * Send v1 advertisement with bloom filters
 */
void meshgrid_v1_send_advert(uint8_t route_type) {
    if (!v1_advert_initialized) {
        meshgrid_v1_init();
    }

    /* Rebuild bloom filters from neighbor table */
    meshgrid_bloom_clear(&v1_bloom_filters);

    /* Add direct neighbors to level 0 */
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbors[i].hops == 0) {
            uint16_t hash_v1 = meshgrid_v1_hash_pubkey(neighbors[i].pubkey);
            meshgrid_bloom_add(&v1_bloom_filters, 0, hash_v1);
        }
    }

    /* Add ourselves to level 0 */
    uint16_t our_hash_v1 = meshgrid_v1_hash_pubkey(mesh.pubkey);
    meshgrid_bloom_add_self(&v1_bloom_filters, our_hash_v1);

    /* Create v1 advertisement packet with bloom filters */
    struct meshgrid_packet pkt;
    uint32_t timestamp = get_current_timestamp();

    int result = meshgrid_create_advert_with_bloom(
        &pkt,
        mesh.pubkey,
        mesh.name,
        timestamp,
        &v1_bloom_filters
    );

    if (result != 0) {
        return;  /* Failed to create */
    }

    /* Set route type */
    pkt.route_type = route_type;
    pkt.header = MESHGRID_MAKE_HEADER(route_type, PAYLOAD_ADVERT, PAYLOAD_VER_MESHGRID);

    /* Encode and transmit */
    uint8_t tx_buf[256];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        radio_transmit(tx_buf, tx_len);
        mesh.packets_tx++;
    }
}

/**
 * Handle received v1 advertisement
 */
void meshgrid_v1_receive_advert(const uint8_t *buf, size_t len, int16_t rssi, int8_t snr) {
    if (!v1_advert_initialized) {
        meshgrid_v1_init();
    }

    /* Parse packet */
    struct meshgrid_packet pkt;
    if (meshgrid_packet_parse(buf, len, &pkt) != 0) {
        return;  /* Parse failed */
    }

    /* Parse advertisement with bloom filters */
    uint8_t pubkey[32];
    char name[17];
    uint32_t timestamp;
    struct meshgrid_bloom_set received_bloom;

    int result = meshgrid_parse_advert_with_bloom(
        &pkt,
        pubkey,
        name,
        sizeof(name),
        &timestamp,
        &received_bloom
    );

    if (result != 0) {
        return;  /* Parse failed */
    }

    /* Check if packet has bloom filters (v1 protocol) */
    if (pkt.version == PAYLOAD_VER_MESHGRID && !meshgrid_bloom_is_empty(&received_bloom)) {
        /* Attenuate and merge bloom filters */
        struct meshgrid_bloom_set attenuated;
        meshgrid_bloom_copy(&attenuated, &received_bloom);

        /* Attenuate (shift levels) since this came from neighbor */
        uint16_t our_hash_v1 = meshgrid_v1_hash_pubkey(mesh.pubkey);
        meshgrid_bloom_attenuate(&attenuated, our_hash_v1);

        /* Merge into our global bloom filters */
        meshgrid_bloom_merge(&v1_bloom_filters, &attenuated);
    }

    /* Neighbor handling is done by the main application */
}

/**
 * Check if should send local beacon
 */
bool meshgrid_v1_should_send_local(void) {
    if (!v1_advert_initialized) {
        return false;
    }
    return meshgrid_beacon_should_send_local(&v1_beacon_schedule, millis());
}

/**
 * Check if should send discovery beacon
 */
bool meshgrid_v1_should_send_discovery(void) {
    if (!v1_advert_initialized) {
        return false;
    }
    return meshgrid_beacon_should_send_discovery(&v1_beacon_schedule, millis());
}

/**
 * Mark local beacon as sent
 */
void meshgrid_v1_local_sent(void) {
    if (v1_advert_initialized) {
        meshgrid_beacon_local_sent(&v1_beacon_schedule, millis());
    }
}

/**
 * Mark discovery beacon as sent
 */
void meshgrid_v1_discovery_sent(void) {
    if (v1_advert_initialized) {
        meshgrid_beacon_discovery_sent(&v1_beacon_schedule, millis());
    }
}

/**
 * Trigger immediate discovery beacon
 */
void meshgrid_v1_trigger_discovery(void) {
    if (v1_advert_initialized) {
        meshgrid_beacon_trigger_discovery(&v1_beacon_schedule);
    }
}
