/**
 * Mesh messaging - main packet dispatcher
 */

#include "messaging.h"
#include "messaging/utils.h"
#include "messaging/receive.h"
#include "messaging/send.h"
#include "neighbors.h"
#include "utils/memory.h"
#include "utils/types.h"
#include "utils/debug.h"
#include "ui/screens.h"
#include "radio/radio_hal.h"
#include "integration/meshgrid_v1_bridge.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <mbedtls/sha256.h>

// Note: Don't include meshcore_integration.h here due to namespace conflict with 'mesh'
// MeshCore message handling is done via callbacks

extern "C" {
#include "hardware/crypto/crypto.h"
#include "utils/cobs.h"
#include "core/meshcore_bridge.h"
}

/* Externs from main.cpp - structs defined in lib/types.h */
extern struct meshgrid_state mesh;
extern uint32_t boot_time;

/* Radio accessor - use get_radio() for PhysicalLayer* */
extern struct rtc_time_t rtc_time;
extern struct display_state display_state;
extern uint32_t last_activity_time;
extern enum meshgrid_device_mode device_mode;

extern struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
extern int custom_channel_count;

extern struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
extern int public_msg_index, public_msg_count;

extern struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
extern int direct_msg_index, direct_msg_count;

extern struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[MAX_CUSTOM_CHANNELS];
extern int channel_msg_count[MAX_CUSTOM_CHANNELS];

extern struct seen_entry seen_table[];
extern uint8_t seen_idx;

extern uint32_t stat_duplicates, stat_flood_fwd, stat_flood_rx;

extern uint8_t public_channel_secret[32];
extern uint8_t public_channel_hash;

/* Functions from main.cpp */
extern void led_blink(void);

/* Radio RX state tracking (from main.cpp) */
extern bool radio_in_rx_mode;

/* ========================================================================= */
/* Main Packet Dispatcher                                                   */
/* ========================================================================= */

void process_packet(uint8_t* buf, int len, int16_t rssi, int8_t snr) {
    struct meshgrid_packet pkt;

    if (meshgrid_packet_parse(buf, len, &pkt) != 0) {
        DEBUG_INFO("[ERR] Bad packet");
        mesh.packets_dropped++;
        return;
    }

    pkt.rssi = rssi;
    pkt.snr = snr;
    pkt.rx_time = millis();
    mesh.packets_rx++;
    stat_flood_rx++;

    /* Check for duplicates */
    if (seen_check_and_add(&pkt)) {
        return; /* Already processed */
    }

    /* SECURITY: Rate limiting - extract source hash */
    uint8_t source_hash = 0;
    if (pkt.route_type == ROUTE_FLOOD && pkt.path_len > 0) {
        /* For flood packets, source is last hop in path */
        source_hash = pkt.path[pkt.path_len - 1];
    } else if (pkt.payload_type == PAYLOAD_ADVERT && pkt.payload_len >= 32) {
        /* For adverts, hash the public key */
        source_hash = crypto_hash_pubkey(&pkt.payload[0]);
    } else if ((pkt.payload_type == PAYLOAD_TXT_MSG || pkt.payload_type == PAYLOAD_GRP_TXT) && pkt.payload_len >= 2) {
        /* For messages, source hash is in payload[1] */
        source_hash = pkt.payload[1];
    }

    /* Check rate limit (skip for ACKs to prevent legitimate traffic blocking) */
    if (pkt.payload_type != PAYLOAD_ACK && source_hash != 0) {
        if (rate_limit_check(source_hash)) {
            /* SECURITY: Rate limit exceeded - drop packet */
            DEBUG_WARNF("RATE LIMIT: Dropped packet from 0x%02X (DoS protection)", source_hash);
            mesh.packets_dropped++;
            return;
        }
    }

    /* Try v1 protocol first (if packet version=1) */
    int v1_result = meshgrid_v1_process_packet(buf, len, rssi, snr);
    if (v1_result == 0) {
        /* v1 handled it successfully */
        return;
    }

    /* Fall back to v0 (MeshCore) - handles version=0 packets */
    if (pkt.payload_type == PAYLOAD_ADVERT || pkt.payload_type == PAYLOAD_TXT_MSG ||
        pkt.payload_type == PAYLOAD_GRP_TXT || pkt.payload_type == PAYLOAD_GRP_DATA) {
        /* Pass raw packet to MeshCore for signature verification and neighbor discovery */
        meshcore_bridge_handle_packet(buf, len, rssi, snr);
    }

    /* Handle by payload type */
    switch (pkt.payload_type) {
        case PAYLOAD_ADVERT:
            handle_advert(&pkt, rssi, snr);
            break;
        case PAYLOAD_TXT_MSG:
            /* Handled automatically by MeshCore via callbacks in handle_received_packet() */
            /* MeshCoreIntegration processes these packets and calls callback_store_direct_message() */
            break;
        case PAYLOAD_GRP_TXT:
        case PAYLOAD_GRP_DATA:
            /* Handled automatically by MeshCore via callbacks in handle_received_packet() */
            /* MeshCoreIntegration processes these packets and calls callback_store_channel_message() */
            break;
        case PAYLOAD_ACK:
            /* ACKs don't need forwarding in flood mode */
            DEBUG_INFOF("RX ACK %ddBm hops:%d", rssi, pkt.path_len);
            return;
        case PAYLOAD_TRACE:
            /* Handle trace packet - MeshCore compatible format */
            if (pkt.route_type == ROUTE_DIRECT && pkt.path_len < MESHGRID_MAX_PATH_SIZE) {
                /* Extract trace request info (MeshCore format) */
                uint8_t i = 0;
                uint32_t trace_id;
                memcpy(&trace_id, &pkt.payload[i], 4);
                i += 4;
                uint32_t auth_code;
                memcpy(&auth_code, &pkt.payload[i], 4);
                i += 4;
                uint8_t flags = pkt.payload[i++];
                uint8_t path_sz = flags & 0x03; /* Lower 2 bits = hash size (0 = 1 byte) */

                uint8_t len = pkt.payload_len - i;
                uint8_t offset = pkt.path_len << path_sz;

                if (offset >= len) {
                    /* TRACE has reached end of given path - we're the destination */
                    struct meshgrid_packet response;
                    memset(&response, 0, sizeof(response));

                    /* Use ROUTE_FLOOD for response since path contains SNRs, not hashes */
                    response.route_type = ROUTE_FLOOD;
                    response.payload_type = PAYLOAD_PATH;
                    response.version = PAYLOAD_VER_MESHCORE;
                    response.header =
                        MESHGRID_MAKE_HEADER(response.route_type, response.payload_type, response.version);

                    /* SECURITY FIX: Bounds check to prevent buffer overflow */
                    if (6 + pkt.path_len > MESHGRID_MAX_PAYLOAD_SIZE) {
                        /* Path too long for response packet - reject */
                        return;
                    }

                    /* Copy trace ID */
                    memcpy(&response.payload[0], &trace_id, 4);

                    /* Copy path (hop count and SNRs) */
                    response.payload[4] = pkt.path_len;
                    for (int j = 0; j < pkt.path_len && j < 32; j++) {
                        response.payload[5 + j] = pkt.path[j];
                    }

                    /* Add our SNR at the end (MeshCore format: SNR * 4) */
                    int8_t snr_value = (int8_t)(snr * 4);
                    response.payload[5 + pkt.path_len] = (uint8_t)snr_value;
                    response.payload_len = 6 + pkt.path_len;

                    /* ROUTE_FLOOD will build its own path as it propagates back */
                    response.path_len = 0;

                    /* Encode and send response */
                    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
                    int tx_len = meshgrid_packet_encode(&response, tx_buf, sizeof(tx_buf));
                    if (tx_len > 0) {
                        get_radio()->transmit(tx_buf, tx_len);
                        radio_in_rx_mode = false;
                        get_radio()->startReceive();
                        mesh.packets_tx++;

                        DEBUG_INFOF("TRACE dest reached (hops: %d)", pkt.path_len);
                    }
                    return;
                } else {
                    /* Check if we're the next hop in the path */
                    uint8_t hash_at_offset = pkt.payload[i + offset];
                    if (hash_at_offset == mesh.our_hash) {
                        /* We're on the path - append SNR and forward */
                        int8_t snr_value = (int8_t)(snr * 4);
                        pkt.path[pkt.path_len] = (uint8_t)snr_value;
                        pkt.path_len++;

                        DEBUG_INFOF("TRACE fwd (hop %d)", pkt.path_len);

                        /* Re-encode and immediately retransmit */
                        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
                        int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));
                        if (tx_len > 0 && tx_queue_add(tx_buf, tx_len, 0, 5)) {
                            mesh.packets_fwd++;
                        }
                    }
                }
            }
            return;
        case PAYLOAD_PATH:
            /* Trace response received */
            if (pkt.payload_len >= 5) {
                uint32_t trace_id;
                memcpy(&trace_id, &pkt.payload[0], 4);
                uint8_t hop_count = pkt.payload[4];

                /* Build JSON response */
                String json = "{\"type\":\"trace_response\",\"trace_id\":";
                json += String(trace_id);
                json += ",\"hops\":";
                json += String(hop_count);
                json += ",\"path\":[";

                for (int i = 0; i < hop_count && i < 32; i++) {
                    if (i > 0)
                        json += ",";
                    json += "\"0x";
                    json += String(pkt.payload[5 + i], HEX);
                    json += "\"";
                }

                json += "],\"rssi\":";
                json += String(rssi);
                json += ",\"snr\":";
                json += String(snr);

                /* Calculate RTT if we can */
                uint32_t now = millis();
                uint32_t rtt = now - trace_id;
                if (rtt < 60000) { /* Only show if < 60 seconds */
                    json += ",\"rtt_ms\":";
                    json += String(rtt);
                }

                json += "}";

                /* Send as COBS frame */
                uint8_t encoded[512];
                size_t encoded_len = cobs_encode(encoded, (const uint8_t*)json.c_str(), json.length());
                Serial.write(encoded, encoded_len);
                Serial.write((uint8_t)0); /* COBS frame delimiter */
                Serial.flush();

                DEBUG_INFOF("TRACE response: %d hops", hop_count);
            }
            return;
        default:
            break;
    }

    /* Forward if appropriate (only REPEATER forwards, CLIENT does not) */
    if (meshgrid_should_forward(&pkt, mesh.our_hash, device_mode)) {
        /* Add ourselves to path */
        meshgrid_path_append(&pkt, mesh.our_hash);

        /* Calculate delay based on path length */
        uint32_t delay_ms = meshgrid_retransmit_delay(&pkt, random_byte());

        /* Priority: longer paths get HIGHER priority (lower number) */
        uint8_t priority = (pkt.path_len > 0) ? (10 - pkt.path_len) : 10;
        if (priority < 1)
            priority = 1; /* Clamp to minimum */

        /* Re-encode packet */
        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
        int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

        if (tx_len > 0) {
            /* Add to transmission queue (non-blocking) */
            if (tx_queue_add(tx_buf, tx_len, delay_ms, priority)) {
                /* Successfully queued */
                mesh.packets_fwd++;
                stat_flood_fwd++;

                const char* type_name = "PKT";
                switch (pkt.payload_type) {
                    case PAYLOAD_ADVERT:
                        type_name = "ADV";
                        break;
                    case PAYLOAD_TXT_MSG:
                        type_name = "MSG";
                        break;
                    case PAYLOAD_GRP_TXT:
                        type_name = "GRP";
                        break;
                    case PAYLOAD_GRP_DATA:
                        type_name = "DAT";
                        break;
                }
                DEBUG_INFOF("QUEUE %s len=%d payload=%d hops:%d delay:%dms prio:%d", type_name, tx_len, pkt.payload_len,
                            pkt.path_len, delay_ms, priority);
            }
            /* If queue full, tx_queue_add() logs error */
        }
    }
}
