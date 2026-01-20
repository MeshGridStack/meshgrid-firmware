/**
 * Network command implementations
 */

#include "network_commands.h"
#include "common.h"
#include "core/neighbors.h"
#include "utils/constants.h"
#include "utils/debug.h"
#include "radio/radio_hal.h"
#include <RadioLib.h>

extern "C" {
#include "network/protocol.h"
}

// Use C bridge to avoid namespace conflict
extern "C" {
#include "core/meshcore_bridge.h"
#include "core/integration/meshgrid_v1_bridge.h"
#include "../../../lib/meshgrid-v1/src/protocol/crypto.h"
}

extern struct meshgrid_state mesh;
extern bool radio_in_rx_mode;
extern void send_advertisement(uint8_t route);
extern void send_group_message(const char* text);
extern uint32_t get_uptime_secs(void);

/* Global to track last send result */
static bool last_send_used_v1 = false;
static int last_v1_result = -999;

/* Auto-select v0 or v1 protocol based on peer capability */
static inline void send_text_message(uint8_t dest_hash, const char* text) {
    /* Check if peer supports v1 */
    bool supports_v1 = meshgrid_v1_peer_supports_v1(dest_hash);
    DEBUG_INFOF("[SEND] Checking peer 0x%02x supports_v1=%d", dest_hash, supports_v1);

    last_send_used_v1 = false;
    last_v1_result = -999;

    if (supports_v1) {
        /* Try v1 - compute 2-byte hash from pubkey */
        uint16_t dest_hash_v1 = 0;
        for (int i = 0; i < neighbor_count; i++) {
            if (neighbors[i].hash == dest_hash) {
                dest_hash_v1 = meshgrid_v1_hash_pubkey(neighbors[i].pubkey);
                DEBUG_INFOF("[SEND] Using v1 protocol: dest_hash=0x%02x, dest_hash_v1=0x%04x", dest_hash, dest_hash_v1);
                break;
            }
        }

        if (dest_hash_v1 != 0) {
            last_v1_result = meshgrid_v1_send_text(dest_hash_v1, text, strlen(text));
            if (last_v1_result == 0) {
                DEBUG_INFO("[SEND] v1 send succeeded");
                last_send_used_v1 = true;
                return; /* v1 succeeded */
            }
        }
        DEBUG_WARN("[SEND] v1 send failed, falling back to v0");
    }

    /* Fall back to v0 */
    DEBUG_INFOF("[SEND] Using v0 protocol for dest=0x%02x", dest_hash);
    meshcore_bridge_send_text(dest_hash, text);
}

void cmd_neighbors() {
    response_print("[");
    for (int i = 0; i < neighbor_count; i++) {
        if (i > 0)
            response_print(",");
        response_print("{\"node_hash\":");
        response_print(neighbors[i].hash);
        response_print(",\"protocol_version\":");
        response_print(neighbors[i].protocol_version);
        response_print(",\"name\":\"");

        /* Escape name for JSON - replace control chars and quotes */
        for (int k = 0; neighbors[i].name[k] != '\0'; k++) {
            char c = neighbors[i].name[k];
            if (c == '"' || c == '\\') {
                response_print("\\");
                response_print(c);
            } else if (c < 32 || c > 126) {
                response_print("."); /* Replace control chars with dot */
            } else {
                response_print(c);
            }
        }

        response_print("\",\"public_key\":[");
        for (int j = 0; j < MESHGRID_PUBKEY_SIZE; j++) {
            if (j > 0)
                response_print(",");
            response_print(neighbors[i].pubkey[j]);
        }
        response_print("],\"rssi\":");
        response_print(neighbors[i].rssi);
        response_print(",\"snr\":");
        response_print(neighbors[i].snr);
        response_print(",\"last_seen_secs\":");
        response_print((millis() - neighbors[i].last_seen) / 1000);
        response_print(",\"firmware\":\"");
        switch (neighbors[i].firmware) {
            case FW_MESHGRID:
                response_print("meshgrid");
                break;
            case FW_MESHCORE:
                response_print("meshcore");
                break;
            case FW_MESHTASTIC:
                response_print("meshtastic");
                break;
            default:
                response_print("other");
                break;
        }
        response_print("\"}");
    }
    response_println("]");
}

void cmd_advert_local() {
    response_println("OK");
    send_advertisement(ROUTE_DIRECT);
}

void cmd_advert_flood() {
    response_println("OK");
    send_advertisement(ROUTE_FLOOD);
}

void cmd_advert() {
    /* Default: send flood advertisement */
    cmd_advert_flood();
}

void cmd_send(const String& args) {
    DEBUG_INFOF("[CMD] cmd_send called with args: '%s'", args.c_str());
    String cmd_args = args; /* Make mutable copy */
    cmd_args.trim();

    /* Try to parse as direct message: check if first word is a known neighbor */
    int spaceIdx = cmd_args.indexOf(' ');
    bool is_direct = false;
    uint8_t dest_hash = 0;

    if (spaceIdx > 0 && spaceIdx < cmd_args.length() - 1) {
        String maybe_dest = cmd_args.substring(0, spaceIdx);

        /* Check if it's a hex hash */
        if (maybe_dest.startsWith("0x")) {
            dest_hash = (uint8_t)strtol(maybe_dest.c_str() + 2, NULL, 16);
            is_direct = true;
        } else {
            /* Check if it matches a neighbor name */
            for (int i = 0; i < neighbor_count; i++) {
                if (strcmp(neighbors[i].name, maybe_dest.c_str()) == 0) {
                    dest_hash = neighbors[i].hash;
                    is_direct = true;
                    DEBUG_INFOF("[CMD] Found neighbor '%s' with hash 0x%02x", maybe_dest.c_str(), dest_hash);
                    break;
                }
            }
        }
    }

    DEBUG_INFOF("[CMD] is_direct=%d, dest_hash=0x%02x", is_direct, dest_hash);

    if (is_direct) {
        /* Direct message to specific neighbor */
        String message = cmd_args.substring(spaceIdx + 1);
        message.trim();

        if (message.length() > 0 && message.length() <= 150) {
            DEBUG_INFOF("[CMD] Calling send_text_message(0x%02x, '%s')", dest_hash, message.c_str());

            /* Check protocol support before sending */
            struct meshgrid_neighbor* n = neighbor_find(dest_hash);
            bool supports_v1 = meshgrid_v1_peer_supports_v1(dest_hash);

            send_text_message(dest_hash, message.c_str());

            /* Show detailed protocol info */
            char resp[100];
            if (n) {
                snprintf(resp, sizeof(resp), "OK proto_ver=%d supports_v1=%d secret_valid=%d used_v1=%d v1_result=%d",
                         n->protocol_version, supports_v1, n->secret_valid, last_send_used_v1, last_v1_result);
            } else {
                snprintf(resp, sizeof(resp), "OK neighbor_not_found");
            }
            response_println(resp);
        } else {
            response_println("ERR Message too long");
        }
    } else {
        /* Broadcast to public channel */
        if (cmd_args.length() > 0 && cmd_args.length() <= 150) {
            send_group_message(cmd_args.c_str());
            response_println("OK");
        } else {
            response_println("ERR Message too long or empty");
        }
    }
}

void cmd_send_group(const String& message) {
    String msg = message;
    msg.trim();
    if (msg.length() > 0 && msg.length() <= 200) {
        send_group_message(msg.c_str());
        response_println("OK");
    } else {
        response_println("ERR Message too long or empty");
    }
}

void cmd_trace(const String& target) {
    String tgt = target;
    tgt.trim();

    if (tgt.length() == 0) {
        response_println("ERR Usage: TRACE <name|hash>");
        return;
    }

    uint8_t dest_hash = 0;
    bool found = false;

    /* Check if it's a hex hash */
    if (tgt.startsWith("0x")) {
        dest_hash = (uint8_t)strtol(tgt.c_str() + 2, NULL, 16);
        found = true;
    } else {
        /* Check if it matches a neighbor name */
        for (int i = 0; i < neighbor_count; i++) {
            if (strcmp(neighbors[i].name, tgt.c_str()) == 0) {
                dest_hash = neighbors[i].hash;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        response_println("ERR Target not found in neighbor table");
        return;
    }

    /* Create trace packet */
    struct meshgrid_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.route_type = ROUTE_DIRECT;
    pkt.payload_type = PAYLOAD_TRACE;
    pkt.version = PAYLOAD_VER_MESHCORE;
    pkt.header = MESHGRID_MAKE_HEADER(pkt.route_type, pkt.payload_type, pkt.version);

    uint32_t trace_id = millis();
    uint32_t auth_code = get_uptime_secs();
    uint8_t flags = 0x00;

    memcpy(&pkt.payload[0], &trace_id, 4);
    memcpy(&pkt.payload[4], &auth_code, 4);
    pkt.payload[8] = flags;
    pkt.payload[9] = dest_hash;
    pkt.payload_len = 10;
    pkt.path_len = 0;

    /* Encode and transmit */
    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));
    if (tx_len > 0) {
        int16_t radio_result = get_radio()->transmit(tx_buf, tx_len);
        radio_in_rx_mode = false;
        get_radio()->startReceive();

        if (radio_result == RADIOLIB_ERR_NONE) {
            mesh.packets_tx++;

            response_print("{\"status\":\"sent\",\"target\":\"0x");
            response_print(dest_hash, HEX);
            response_print("\",\"trace_id\":");
            response_print(trace_id);
            response_print(",\"hops\":");
            response_print(pkt.path_len);
            response_println("}");
        } else {
            response_print("ERR Radio TX failed: ");
            response_println(radio_result);
        }
    } else {
        response_println("ERR Packet encode failed");
    }
}
