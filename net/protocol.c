/**
 * meshgrid protocol - MeshCore-compatible packet handling
 */

#include "protocol.h"
#include <string.h>
#include <stddef.h>

/*
 * Compute 1-byte hash from public key (MeshCore compatible)
 * MeshCore uses first byte of pubkey as hash
 */
uint8_t meshgrid_hash_pubkey(const uint8_t *pubkey)
{
    return pubkey[0];
}

/*
 * Simple CRC-like hash for packet deduplication
 * Uses payload type + payload content
 */
void meshgrid_packet_hash(const struct meshgrid_packet *pkt, uint8_t *hash)
{
    /* Simple hash: XOR of payload type and payload bytes */
    uint8_t h = pkt->payload_type;
    for (uint16_t i = 0; i < pkt->payload_len; i++) {
        h ^= pkt->payload[i];
        h = (h << 1) | (h >> 7); /* rotate left */
    }
    hash[0] = h;
}

/*
 * Encode packet to wire format (MeshCore compatible)
 *
 * Wire format:
 *   [0]     header
 *   [1-4]   transport_codes (only if HAS_TRANSPORT)
 *   [N]     path_len
 *   [N+1..] path (path_len bytes)
 *   [...]   payload (remaining bytes)
 */
int meshgrid_packet_encode(const struct meshgrid_packet *pkt, uint8_t *buf, size_t buf_len)
{
    size_t i = 0;

    /* Header */
    buf[i++] = pkt->header;

    /* Transport codes (if present) */
    if (MESHGRID_HAS_TRANSPORT(pkt->route_type)) {
        if (i + 4 > buf_len) return -1;
        buf[i++] = pkt->transport_codes[0] & 0xFF;
        buf[i++] = (pkt->transport_codes[0] >> 8) & 0xFF;
        buf[i++] = pkt->transport_codes[1] & 0xFF;
        buf[i++] = (pkt->transport_codes[1] >> 8) & 0xFF;
    }

    /* Path length */
    if (i + 1 > buf_len) return -1;
    buf[i++] = pkt->path_len;

    /* Path */
    if (i + pkt->path_len > buf_len) return -1;
    memcpy(&buf[i], pkt->path, pkt->path_len);
    i += pkt->path_len;

    /* Payload */
    if (i + pkt->payload_len > buf_len) return -1;
    memcpy(&buf[i], pkt->payload, pkt->payload_len);
    i += pkt->payload_len;

    return (int)i;
}

/*
 * Parse packet from wire format (MeshCore compatible)
 */
int meshgrid_packet_parse(const uint8_t *buf, size_t len, struct meshgrid_packet *pkt)
{
    if (len < 2) return -1;  /* Minimum: header + path_len */

    size_t i = 0;

    /* Header */
    pkt->header = buf[i++];
    pkt->route_type = MESHGRID_GET_ROUTE(pkt->header);
    pkt->payload_type = MESHGRID_GET_TYPE(pkt->header);
    pkt->version = MESHGRID_GET_VERSION(pkt->header);

    /* Transport codes (if present) */
    if (MESHGRID_HAS_TRANSPORT(pkt->route_type)) {
        if (i + 4 > len) return -1;
        pkt->transport_codes[0] = buf[i] | (buf[i+1] << 8);
        i += 2;
        pkt->transport_codes[1] = buf[i] | (buf[i+1] << 8);
        i += 2;
    } else {
        pkt->transport_codes[0] = 0;
        pkt->transport_codes[1] = 0;
    }

    /* Path length */
    if (i >= len) return -1;
    pkt->path_len = buf[i++];
    if (pkt->path_len > MESHGRID_MAX_PATH_SIZE) return -1;

    /* Path */
    if (i + pkt->path_len > len) return -1;
    memcpy(pkt->path, &buf[i], pkt->path_len);
    i += pkt->path_len;

    /* Payload (remaining bytes) */
    if (i > len) return -1;
    pkt->payload_len = len - i;
    if (pkt->payload_len > MESHGRID_MAX_PAYLOAD_SIZE) return -1;
    memcpy(pkt->payload, &buf[i], pkt->payload_len);

    return 0;
}

/*
 * Should we forward this packet?
 *
 * Returns true if:
 * - Packet is flood routed
 * - We're not already in the path
 * - Packet hasn't been seen recently (caller should check seen table)
 */
bool meshgrid_should_forward(const struct meshgrid_packet *pkt, uint8_t our_hash)
{
    /* Only flood packets should be forwarded by repeaters */
    if (!MESHGRID_IS_FLOOD(pkt->route_type)) {
        return false;
    }

    /* Don't forward if we're already in the path */
    for (uint8_t i = 0; i < pkt->path_len; i++) {
        if (pkt->path[i] == our_hash) {
            return false;
        }
    }

    return true;
}

/*
 * Calculate retransmit delay
 *
 * Longer paths get priority (shorter delay) to let packets travel further
 * before being re-broadcast. This reduces collisions.
 *
 * Based on MeshCore's algorithm:
 *   base_delay * (1.0 + path_len * 0.1) + random_jitter
 */
uint32_t meshgrid_retransmit_delay(const struct meshgrid_packet *pkt, uint32_t random_byte)
{
    uint32_t base = MESHGRID_RETRANSMIT_BASE_MS;

    /* Increase delay based on path length (longer path = higher priority = shorter delay) */
    /* Invert: shorter path = longer delay to let long-distance packets through first */
    uint32_t path_factor = (MESHGRID_MAX_PATH_SIZE - pkt->path_len) * 10;

    /* Add randomness to avoid synchronized transmissions */
    uint32_t jitter = (random_byte * MESHGRID_RETRANSMIT_BASE_MS) / 256;

    uint32_t delay = base + path_factor + jitter;

    if (delay > MESHGRID_RETRANSMIT_MAX_MS) {
        delay = MESHGRID_RETRANSMIT_MAX_MS;
    }

    return delay;
}

/*
 * Add our hash to the path (for flood routing)
 */
int meshgrid_path_append(struct meshgrid_packet *pkt, uint8_t our_hash)
{
    if (pkt->path_len >= MESHGRID_MAX_PATH_SIZE) {
        return -1;
    }

    pkt->path[pkt->path_len++] = our_hash;
    return 0;
}

/*
 * Create advertisement packet
 */
int meshgrid_create_advert(struct meshgrid_packet *pkt, const uint8_t *pubkey,
                           const char *name, uint32_t timestamp)
{
    pkt->header = MESHGRID_MAKE_HEADER(ROUTE_FLOOD, PAYLOAD_ADVERT, PAYLOAD_VER_MESHCORE);
    pkt->route_type = ROUTE_FLOOD;
    pkt->payload_type = PAYLOAD_ADVERT;
    pkt->version = PAYLOAD_VER_MESHCORE;
    pkt->transport_codes[0] = 0;
    pkt->transport_codes[1] = 0;
    pkt->path_len = 0;

    /* Payload: pubkey(32) + timestamp(4) + name_len(1) + name */
    size_t i = 0;

    /* Public key */
    memcpy(&pkt->payload[i], pubkey, MESHGRID_PUBKEY_SIZE);
    i += MESHGRID_PUBKEY_SIZE;

    /* Timestamp (little endian) */
    pkt->payload[i++] = timestamp & 0xFF;
    pkt->payload[i++] = (timestamp >> 8) & 0xFF;
    pkt->payload[i++] = (timestamp >> 16) & 0xFF;
    pkt->payload[i++] = (timestamp >> 24) & 0xFF;

    /* Name */
    uint8_t name_len = strlen(name);
    if (name_len > MESHGRID_NODE_NAME_MAX) {
        name_len = MESHGRID_NODE_NAME_MAX;
    }
    pkt->payload[i++] = name_len;
    memcpy(&pkt->payload[i], name, name_len);
    i += name_len;

    pkt->payload_len = i;

    return 0;
}

/*
 * Parse advertisement payload
 */
int meshgrid_parse_advert(const struct meshgrid_packet *pkt,
                          uint8_t *pubkey, char *name, size_t name_max,
                          uint32_t *timestamp)
{
    if (pkt->payload_type != PAYLOAD_ADVERT) {
        return -1;
    }

    if (pkt->payload_len < MESHGRID_PUBKEY_SIZE + 4 + 1) {
        return -1;  /* Too short */
    }

    size_t i = 0;

    /* Public key */
    memcpy(pubkey, &pkt->payload[i], MESHGRID_PUBKEY_SIZE);
    i += MESHGRID_PUBKEY_SIZE;

    /* Timestamp */
    *timestamp = pkt->payload[i] |
                 (pkt->payload[i+1] << 8) |
                 (pkt->payload[i+2] << 16) |
                 (pkt->payload[i+3] << 24);
    i += 4;

    /* Name */
    uint8_t name_len = pkt->payload[i++];
    if (name_len > MESHGRID_NODE_NAME_MAX || i + name_len > pkt->payload_len) {
        return -1;
    }

    if (name_len >= name_max) {
        name_len = name_max - 1;
    }
    memcpy(name, &pkt->payload[i], name_len);
    name[name_len] = '\0';

    return 0;
}
