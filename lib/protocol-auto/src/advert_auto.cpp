/**
 * Protocol Auto - Advertisement Implementation
 *
 * Automatically dispatches to v0 or v1 advertisement handlers
 */

#include "advert_auto.h"
#include <Arduino.h>

/* Protocol version availability */
#ifndef PROTOCOL_V0_ENABLED
  #define PROTOCOL_V0_ENABLED 1
#endif

/* Protocol packet structure */
#define MESHGRID_MAX_PAYLOAD_SIZE 200
#define MESHGRID_MAX_PATH_SIZE 8

struct meshgrid_packet {
    uint8_t header;
    uint8_t route_type;
    uint8_t payload_type;
    uint8_t version;
    uint16_t transport_codes[2];
    uint8_t path[MESHGRID_MAX_PATH_SIZE];
    uint8_t path_len;
    uint8_t payload[MESHGRID_MAX_PAYLOAD_SIZE];
    uint16_t payload_len;
    int16_t rssi;
    int8_t snr;
};

/* Protocol-specific advertisement headers */
#if PROTOCOL_V0_ENABLED
  #include <advert_v0.h>
#endif

/* Packet version constants */
#define PAYLOAD_VER_MESHCORE 0

/**
 * Initialize advertisement system
 */
void advert_auto_init(void) {
    /* No initialization needed for v0 protocol */
}

/**
 * Send advertisement using v0 protocol (MeshCore compatible)
 */
void advert_auto_send(uint8_t route_type) {
#if PROTOCOL_V0_ENABLED
    advert_v0_send(route_type);
#else
    #error "Protocol v0 must be enabled"
#endif
}

/**
 * Handle received advertisement (v0 protocol)
 */
void advert_auto_receive(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr) {
#if PROTOCOL_V0_ENABLED
    advert_v0_receive(pkt, rssi, snr);
#else
    #error "Protocol v0 must be enabled"
#endif
}
