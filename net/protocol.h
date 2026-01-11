/**
 * meshgrid protocol - MeshCore-compatible packet format
 *
 * CRITICAL: Fully compatible with MeshCore for interoperability.
 * Uses same format: 1-byte hash, 2-byte MAC, AES encryption.
 *
 * Future: Version 1+ packets can use enhanced crypto between
 * meshgrid devices, falling back to MeshCore format for interop.
 */

#ifndef MESHGRID_PROTOCOL_H
#define MESHGRID_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * MeshCore-compatible constants
 */
#define MESHGRID_PUBKEY_SIZE       32   /* Ed25519/X25519 public key */
#define MESHGRID_PRIVKEY_SIZE      64   /* Ed25519 private key (seed+pub) */
#define MESHGRID_SIGNATURE_SIZE    64   /* Ed25519 signature */
#define MESHGRID_SHARED_SECRET_SIZE 32  /* X25519 shared secret */

/* MeshCore crypto (compatibility mode) */
#define MESHCORE_CIPHER_KEY_SIZE   16   /* AES-128 key */
#define MESHCORE_CIPHER_BLOCK_SIZE 16   /* AES block size */
#define MESHCORE_MAC_SIZE          2    /* 2-byte MAC (weak, but compatible) */
#define MESHCORE_HASH_SIZE         1    /* 1-byte node hash */

/* meshgrid enhanced crypto (v1+, meshgrid-to-meshgrid only) */
#define MESHGRID_CHACHA_KEY_SIZE   32   /* ChaCha20 key */
#define MESHGRID_CHACHA_NONCE_SIZE 12   /* ChaCha20 nonce */
#define MESHGRID_POLY1305_TAG_SIZE 16   /* Poly1305 MAC */
#define MESHGRID_HASH_SIZE         2    /* 2-byte node hash (enhanced mode) */

/*
 * Packet limits (MeshCore compatible)
 */
#define MESHGRID_MAX_PACKET_SIZE   255
#define MESHGRID_MAX_PAYLOAD_SIZE  184  /* MeshCore MAX_PACKET_PAYLOAD */
#define MESHGRID_MAX_PATH_SIZE     64   /* MeshCore MAX_PATH_SIZE */
#define MESHGRID_MAX_ADVERT_DATA   32   /* MeshCore MAX_ADVERT_DATA_SIZE */
#define MESHGRID_NODE_NAME_MAX     16

/*
 * Timing constants (milliseconds)
 */
#define MESHGRID_ADVERT_INTERVAL_MS     (12 * 60 * 60 * 1000)  /* 12 hours (flood) */
#define MESHGRID_LOCAL_ADVERT_MS        (2 * 60 * 1000)       /* 2 minutes (local) */
#define MESHGRID_NEIGHBOR_TIMEOUT_MS    (15 * 60 * 1000)
#define MESHGRID_RETRANSMIT_BASE_MS     100
#define MESHGRID_RETRANSMIT_MAX_MS      5000
#define MESHGRID_DUPLICATE_WINDOW_MS    (60 * 1000)

/*
 * Packet header byte layout (MeshCore compatible):
 *   bits 0-1: Route type (4 values)
 *   bits 2-5: Payload type (16 values)
 *   bits 6-7: Payload version (4 values)
 */
#define MESHGRID_HDR_ROUTE_MASK    0x03
#define MESHGRID_HDR_TYPE_SHIFT    2
#define MESHGRID_HDR_TYPE_MASK     0x0F
#define MESHGRID_HDR_VER_SHIFT     6
#define MESHGRID_HDR_VER_MASK      0x03

/*
 * Route types (MeshCore compatible)
 */
enum meshgrid_route_type {
    ROUTE_TRANSPORT_FLOOD = 0, /* Flood + transport codes */
    ROUTE_FLOOD = 1,           /* Flood, builds path */
    ROUTE_DIRECT = 2,          /* Direct using path */
    ROUTE_TRANSPORT_DIRECT = 3,/* Direct + transport codes */
};

/*
 * Payload types (MeshCore compatible)
 */
enum meshgrid_payload_type {
    PAYLOAD_REQ = 0,           /* Request (encrypted) */
    PAYLOAD_RESPONSE = 1,      /* Response (encrypted) */
    PAYLOAD_TXT_MSG = 2,       /* Text message (encrypted) */
    PAYLOAD_ACK = 3,           /* Simple ACK */
    PAYLOAD_ADVERT = 4,        /* Node advertisement */
    PAYLOAD_GRP_TXT = 5,       /* Group text (encrypted) */
    PAYLOAD_GRP_DATA = 6,      /* Group data (encrypted) */
    PAYLOAD_ANON_REQ = 7,      /* Anonymous request */
    PAYLOAD_PATH = 8,          /* Path return */
    PAYLOAD_TRACE = 9,         /* Trace route */
    PAYLOAD_MULTIPART = 10,    /* Multipart packet */
    PAYLOAD_CONTROL = 11,      /* Control/discovery */
    /* 12-14 reserved */
    PAYLOAD_RAW_CUSTOM = 15,   /* Custom raw data */
};

/*
 * Payload version (MeshCore compatible)
 */
enum meshgrid_payload_version {
    PAYLOAD_VER_MESHCORE = 0,  /* MeshCore v1: 1-byte hash, 2-byte MAC */
    PAYLOAD_VER_MESHGRID = 1,  /* meshgrid v1: 2-byte hash, 16-byte MAC */
    /* 2-3 reserved for future */
};

/*
 * Device modes
 */
enum meshgrid_device_mode {
    MODE_CLIENT = 0,           /* Full client */
    MODE_REPEATER = 1,         /* Forward only */
    MODE_ROOM = 2,             /* Room server */
};

/*
 * Helper macros
 */
#define MESHGRID_MAKE_HEADER(route, type, ver) \
    (((route) & MESHGRID_HDR_ROUTE_MASK) | \
     (((type) & MESHGRID_HDR_TYPE_MASK) << MESHGRID_HDR_TYPE_SHIFT) | \
     (((ver) & MESHGRID_HDR_VER_MASK) << MESHGRID_HDR_VER_SHIFT))

#define MESHGRID_GET_ROUTE(hdr)    ((hdr) & MESHGRID_HDR_ROUTE_MASK)
#define MESHGRID_GET_TYPE(hdr)     (((hdr) >> MESHGRID_HDR_TYPE_SHIFT) & MESHGRID_HDR_TYPE_MASK)
#define MESHGRID_GET_VERSION(hdr)  (((hdr) >> MESHGRID_HDR_VER_SHIFT) & MESHGRID_HDR_VER_MASK)

#define MESHGRID_IS_FLOOD(route)   ((route) == ROUTE_FLOOD || (route) == ROUTE_TRANSPORT_FLOOD)
#define MESHGRID_IS_DIRECT(route)  ((route) == ROUTE_DIRECT || (route) == ROUTE_TRANSPORT_DIRECT)
#define MESHGRID_HAS_TRANSPORT(route) ((route) == ROUTE_TRANSPORT_FLOOD || (route) == ROUTE_TRANSPORT_DIRECT)

/*
 * Parsed packet structure
 */
struct meshgrid_packet {
    /* Header */
    uint8_t header;
    uint8_t route_type;
    uint8_t payload_type;
    uint8_t version;

    /* Transport codes (if HAS_TRANSPORT) */
    uint16_t transport_codes[2];

    /* Path */
    uint8_t path[MESHGRID_MAX_PATH_SIZE];
    uint8_t path_len;

    /* Payload */
    uint8_t payload[MESHGRID_MAX_PAYLOAD_SIZE];
    uint16_t payload_len;

    /* RX metadata */
    int16_t rssi;
    int8_t snr;
    uint32_t rx_time;
};

/*
 * Node types (inferred from advert data or name prefix)
 */
enum meshgrid_node_type {
    NODE_TYPE_UNKNOWN = 0,
    NODE_TYPE_CLIENT = 1,
    NODE_TYPE_REPEATER = 2,
    NODE_TYPE_ROOM = 3,
};

/*
 * Firmware types
 */
enum meshgrid_firmware {
    FW_UNKNOWN = 0,
    FW_MESHCORE = 1,
    FW_MESHGRID = 2,
    FW_MESHTASTIC = 3,
};

/*
 * Neighbor info
 */
struct meshgrid_neighbor {
    uint8_t pubkey[MESHGRID_PUBKEY_SIZE];
    uint8_t hash;                      /* 1-byte hash (MeshCore compat) */
    char name[MESHGRID_NODE_NAME_MAX + 1];
    uint32_t last_seen;
    uint32_t advert_timestamp;
    int16_t rssi;
    int8_t snr;
    enum meshgrid_node_type node_type; /* Inferred node type */
    enum meshgrid_firmware firmware;   /* Detected firmware */
    uint8_t hops;                      /* Hop count when first seen */
    uint8_t shared_secret[32];         /* Cached ECDH shared secret */
    bool secret_valid;                 /* True if shared_secret is cached */

    /* Protocol v1 state (for meshgrid-to-meshgrid communication) */
    uint32_t last_seq_rx;              /* Last received sequence number */
    uint32_t next_seq_tx;              /* Next sequence number to send */
};

/*
 * Mesh state
 */
struct meshgrid_state {
    /* Our identity */
    uint8_t privkey[MESHGRID_PRIVKEY_SIZE];
    uint8_t pubkey[MESHGRID_PUBKEY_SIZE];
    uint8_t our_hash;                  /* 1-byte (MeshCore compat) */
    char name[MESHGRID_NODE_NAME_MAX + 1];

    /* Device mode */
    enum meshgrid_device_mode mode;

    /* Stats */
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t packets_fwd;
    uint32_t packets_dropped;
    uint32_t uptime_secs;

    /* Telemetry */
    uint16_t battery_mv;
    uint8_t battery_pct;
    int16_t temp_deci_c;
};

/*
 * Function prototypes
 */

/* Compute 1-byte hash from public key (MeshCore compatible) */
uint8_t meshgrid_hash_pubkey(const uint8_t *pubkey);

/* Compute packet hash for deduplication */
void meshgrid_packet_hash(const struct meshgrid_packet *pkt, uint8_t *hash);

/* Encode packet to wire format */
int meshgrid_packet_encode(const struct meshgrid_packet *pkt, uint8_t *buf, size_t buf_len);

/* Parse packet from wire format */
int meshgrid_packet_parse(const uint8_t *buf, size_t len, struct meshgrid_packet *pkt);

/* Should we forward this packet? */
bool meshgrid_should_forward(const struct meshgrid_packet *pkt, uint8_t our_hash, enum meshgrid_device_mode mode);

/* Calculate retransmit delay based on path length */
uint32_t meshgrid_retransmit_delay(const struct meshgrid_packet *pkt, uint32_t random_byte);

/* Add our hash to the path */
int meshgrid_path_append(struct meshgrid_packet *pkt, uint8_t our_hash);

/* Create advertisement packet */
int meshgrid_create_advert(struct meshgrid_packet *pkt, const uint8_t *pubkey,
                           const char *name, uint32_t timestamp);

/* Parse advertisement payload */
int meshgrid_parse_advert(const struct meshgrid_packet *pkt,
                          uint8_t *pubkey, char *name, size_t name_max,
                          uint32_t *timestamp);

#endif /* MESHGRID_PROTOCOL_H */
