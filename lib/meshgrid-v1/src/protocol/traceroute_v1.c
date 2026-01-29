/**
 * meshgrid v1 Traceroute Implementation
 */

#include "traceroute_v1.h"
#include <string.h>

/*
 * Handle v1 traceroute packet (2-byte hashes)
 *
 * Format similar to v0 but uses:
 * - PAYLOAD_VER_MESHGRID (version 1)
 * - 2-byte node hashes instead of 1-byte
 * - path_sz = 1 (2 bytes per hash)
 */
bool meshgrid_handle_trace_v1(
    struct meshgrid_packet *pkt,
    uint16_t our_hash_v1,
    int16_t rssi,
    int8_t snr,
    struct meshgrid_v1_trace_callbacks *callbacks
) {
    if (pkt->route_type != ROUTE_DIRECT || pkt->path_len >= MESHGRID_MAX_PATH_SIZE) {
        return false;
    }

    /* Only handle v1 trace packets */
    if (pkt->version != PAYLOAD_VER_MESHGRID) {
        return false;
    }

    /* Extract trace request info */
    uint8_t i = 0;
    uint32_t trace_id;
    memcpy(&trace_id, &pkt->payload[i], 4);
    i += 4;
    uint32_t auth_code;
    memcpy(&auth_code, &pkt->payload[i], 4);
    i += 4;
    uint8_t flags = pkt->payload[i++];
    uint8_t path_sz = (flags & 0x03); /* 1 = 2-byte hashes for v1 */

    /* Check if we're the destination (2-byte hash at offset 9-10) */
    uint16_t dest_hash = (uint16_t)pkt->payload[9] | ((uint16_t)pkt->payload[10] << 8);
    bool we_are_dest = (dest_hash == our_hash_v1);

    uint8_t len = pkt->payload_len - i;
    uint8_t offset = pkt->path_len << path_sz;

    if (we_are_dest || offset >= len) {
        /* TRACE has reached destination - send response */
        struct meshgrid_packet response;
        memset(&response, 0, sizeof(response));

        /* Use ROUTE_FLOOD for response */
        response.route_type = ROUTE_FLOOD;
        response.payload_type = PAYLOAD_PATH;
        response.version = PAYLOAD_VER_MESHGRID;  /* v1 response */
        response.header = MESHGRID_MAKE_HEADER(response.route_type, response.payload_type, response.version);

        /* Bounds check */
        if (6 + pkt->path_len > MESHGRID_MAX_PAYLOAD_SIZE) {
            return true;  /* Handled (dropped) */
        }

        /* Copy trace ID */
        memcpy(&response.payload[0], &trace_id, 4);

        /* Copy path (hop count and SNRs) */
        response.payload[4] = pkt->path_len;
        for (int j = 0; j < pkt->path_len && j < 32; j++) {
            response.payload[5 + j] = pkt->path[j];
        }

        /* Add our SNR at the end (SNR * 4) */
        int8_t snr_value = (int8_t)(snr * 4);
        response.payload[5 + pkt->path_len] = (uint8_t)snr_value;
        response.payload_len = 6 + pkt->path_len;

        /* ROUTE_FLOOD will build its own path */
        response.path_len = 0;

        /* Encode and transmit via callback */
        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
        int tx_len = meshgrid_packet_encode(&response, tx_buf, sizeof(tx_buf));
        if (tx_len > 0 && callbacks && callbacks->transmit) {
            callbacks->transmit(tx_buf, tx_len);
            if (callbacks->inc_packets_tx) {
                callbacks->inc_packets_tx();
            }
        }
        return true;  /* Handled */
    } else {
        /* Check if we're the next hop (2-byte hash) */
        uint16_t hash_at_offset = (uint16_t)pkt->payload[i + offset] |
                                  ((uint16_t)pkt->payload[i + offset + 1] << 8);
        if (hash_at_offset == our_hash_v1) {
            /* We're on the path - append SNR and forward */
            int8_t snr_value = (int8_t)(snr * 4);
            pkt->path[pkt->path_len] = (uint8_t)snr_value;
            pkt->path_len++;

            /* Re-encode and queue via callback */
            uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
            int tx_len = meshgrid_packet_encode(pkt, tx_buf, sizeof(tx_buf));
            if (tx_len > 0 && callbacks && callbacks->queue_add) {
                if (callbacks->queue_add(tx_buf, tx_len, 0, 5)) {
                    if (callbacks->inc_packets_fwd) {
                        callbacks->inc_packets_fwd();
                    }
                }
            }
            return true;  /* Handled */
        }
    }

    return false;  /* Not handled */
}
