/**
 * meshgrid v1 Protocol Extensions
 *
 * V1-specific packet functions (bloom filter advertisements, 2-byte hashing)
 * Base protocol functions are in src/network/protocol.c
 */

#include "packet.h"
#include <string.h>

/*
 * Create v1 advertisement with bloom filters
 *
 * Extended MeshCore format for meshgrid v1:
 * - Uses PAYLOAD_VER_MESHGRID (version 1)
 * - Adds 32-byte bloom filter set after standard fields
 * - Compatible with v0 parsing (v0 nodes ignore extra data)
 */
int meshgrid_create_advert_with_bloom(
    struct meshgrid_packet *pkt,
    const uint8_t *pubkey,
    const char *name,
    uint32_t timestamp,
    const void *bloom_filters  /* struct meshgrid_bloom_set* */
) {
    /* First create standard MeshCore advertisement */
    meshgrid_create_advert(pkt, pubkey, name, timestamp);

    /* Upgrade to v1 protocol version */
    pkt->version = PAYLOAD_VER_MESHGRID;
    pkt->header = MESHGRID_MAKE_HEADER(pkt->route_type, pkt->payload_type, PAYLOAD_VER_MESHGRID);

    /* If no bloom filters provided, return standard advert */
    if (bloom_filters == NULL) {
        return 0;
    }

    /* Add bloom filters to end of payload */
    /* The bloom filters go after: pubkey(32) + timestamp(4) + signature(64) + app_data */
    /* We'll insert them after the flags byte but before the name */

    /* Current payload structure:
     * [0..31]   pubkey
     * [32..35]  timestamp
     * [36..99]  signature (will be filled by caller)
     * [100]     flags byte
     * [101..]   name
     */

    uint8_t flags_pos = 32 + 4 + 64;  /* Position of flags byte */
    uint8_t flags = pkt->payload[flags_pos];
    size_t name_start = flags_pos + 1;
    size_t name_len = pkt->payload_len - name_start;

    /* Make space for bloom filters (32 bytes) */
    uint8_t temp_name[MESHGRID_NODE_NAME_MAX];
    if (name_len > 0) {
        memcpy(temp_name, &pkt->payload[name_start], name_len);
    }

    /* Write bloom filters after flags byte */
    const uint8_t *bloom_bytes = (const uint8_t *)bloom_filters;
    memcpy(&pkt->payload[name_start], bloom_bytes, 32);

    /* Write name after bloom filters */
    if (name_len > 0) {
        memcpy(&pkt->payload[name_start + 32], temp_name, name_len);
    }

    pkt->payload_len += 32;

    /* Set custom flag to indicate bloom filters present (use feat1 bit) */
    pkt->payload[flags_pos] = flags | 0x20;

    return 0;
}

/*
 * Parse v1 advertisement with bloom filters
 *
 * Parses standard fields plus bloom filters if present (v1 protocol)
 * Falls back gracefully if bloom filters not present (v0 protocol)
 */
int meshgrid_parse_advert_with_bloom(
    const struct meshgrid_packet *pkt,
    uint8_t *pubkey,
    char *name,
    size_t name_max,
    uint32_t *timestamp,
    void *bloom_filters  /* struct meshgrid_bloom_set* or NULL */
) {
    if (pkt->payload_type != PAYLOAD_ADVERT) {
        return -1;
    }

    /* Minimum: pubkey + timestamp + signature + flags */
    if (pkt->payload_len < MESHGRID_PUBKEY_SIZE + 4 + MESHGRID_SIGNATURE_SIZE + 1) {
        return -1;
    }

    size_t i = 0;

    /* 1. Public key (32 bytes) */
    memcpy(pubkey, &pkt->payload[i], MESHGRID_PUBKEY_SIZE);
    i += MESHGRID_PUBKEY_SIZE;

    /* 2. Timestamp (4 bytes, little endian) */
    *timestamp = pkt->payload[i] |
                 (pkt->payload[i+1] << 8) |
                 (pkt->payload[i+2] << 16) |
                 (pkt->payload[i+3] << 24);
    i += 4;

    /* 3. Signature (64 bytes) */
    i += MESHGRID_SIGNATURE_SIZE;

    /* 4. App data - starts with flags byte */
    if (i >= pkt->payload_len) {
        return -1;
    }

    uint8_t flags = pkt->payload[i++];

    /* Skip optional lat/lon (8 bytes) */
    if (flags & 0x10) {
        i += 8;
    }

    /* Check for v1 bloom filters (using feat1 bit) */
    bool has_bloom = (flags & 0x20) && (pkt->version == PAYLOAD_VER_MESHGRID);

    if (has_bloom && bloom_filters != NULL) {
        /* Extract bloom filters (32 bytes) */
        if (i + 32 > pkt->payload_len) {
            return -1;  /* Not enough data */
        }
        memcpy(bloom_filters, &pkt->payload[i], 32);
        i += 32;
    } else if (has_bloom) {
        /* Bloom filters present but caller doesn't want them - skip */
        i += 32;
    } else if (bloom_filters != NULL) {
        /* No bloom filters in packet - clear output */
        memset(bloom_filters, 0, 32);
    }

    /* Skip optional feat2 (2 bytes) */
    if (flags & 0x40) {
        i += 2;
    }

    /* Name is remainder of packet */
    if (flags & 0x80) {
        size_t name_len = pkt->payload_len - i;

        if (name_len > MESHGRID_NODE_NAME_MAX) {
            name_len = MESHGRID_NODE_NAME_MAX;
        }
        if (name_len >= name_max) {
            name_len = name_max - 1;
        }

        if (name_len > 0) {
            memcpy(name, &pkt->payload[i], name_len);

            /* Sanitize control characters */
            size_t write_pos = 0;
            for (size_t read_pos = 0; read_pos < name_len; read_pos++) {
                if (name[read_pos] >= 32 && name[read_pos] <= 126) {
                    name[write_pos++] = name[read_pos];
                }
            }
            name_len = write_pos;
        }
        name[name_len] = '\0';
    } else {
        name[0] = '\0';
    }

    return 0;
}

/*
 * Note: meshgrid_v1_hash_pubkey() is defined in crypto.c
 * It uses SHA256 for better security
 */
