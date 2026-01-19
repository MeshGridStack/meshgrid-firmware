/**
 * meshgrid v1 OTA Manifest Handling
 *
 * OTA manifest contains metadata about firmware update:
 * - Version and size information
 * - SHA-256 hash of firmware
 * - Ed25519 signature for authenticity
 * - Session ID to coordinate distributed update
 *
 * Security:
 * - Ed25519 signature prevents malicious firmware
 * - SHA-256 hash verifies integrity after download
 * - Trusted public keys must be pre-configured
 */

#ifndef MESHGRID_V1_OTA_MANIFEST_H
#define MESHGRID_V1_OTA_MANIFEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OTA manifest constants
 */
#define MESHGRID_OTA_VERSION_MAX     32    /* Max version string length */
#define MESHGRID_OTA_HASH_SIZE       32    /* SHA-256 hash */
#define MESHGRID_OTA_SIGNATURE_SIZE  64    /* Ed25519 signature */
#define MESHGRID_OTA_PUBKEY_SIZE     32    /* Ed25519 public key */
#define MESHGRID_OTA_CHUNK_SIZE      174   /* Default chunk size (fits in packet) */
#define MESHGRID_OTA_SESSION_INVALID 0     /* Invalid session ID */

/*
 * OTA manifest structure
 */
struct meshgrid_ota_manifest {
    uint32_t session_id;                          /* Unique session ID */
    char version[MESHGRID_OTA_VERSION_MAX];       /* Version string (e.g., "1.2.3") */
    uint32_t firmware_size;                       /* Total firmware size (bytes) */
    uint16_t chunk_size;                          /* Bytes per chunk */
    uint32_t total_chunks;                        /* Number of chunks */
    uint8_t firmware_hash[MESHGRID_OTA_HASH_SIZE];/* SHA-256 of firmware */
    uint8_t signature[MESHGRID_OTA_SIGNATURE_SIZE];/* Ed25519 signature */
    uint8_t signing_pubkey[MESHGRID_OTA_PUBKEY_SIZE];/* Signer's public key */
};

/*
 * Manifest operations
 */

/**
 * Initialize empty manifest
 *
 * @param manifest  Manifest structure to initialize
 */
void meshgrid_ota_manifest_init(struct meshgrid_ota_manifest *manifest);

/**
 * Create OTA manifest (for firmware publisher)
 *
 * @param manifest       Output manifest
 * @param session_id     Unique session ID (use timestamp or random)
 * @param version        Version string (e.g., "1.2.3")
 * @param firmware       Firmware binary data
 * @param firmware_size  Size of firmware
 * @param chunk_size     Bytes per chunk (typically 174)
 * @param signing_key    Ed25519 private key (64 bytes)
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_ota_manifest_create(
    struct meshgrid_ota_manifest *manifest,
    uint32_t session_id,
    const char *version,
    const uint8_t *firmware,
    uint32_t firmware_size,
    uint16_t chunk_size,
    const uint8_t *signing_key
);

/**
 * Verify Ed25519 signature on manifest
 *
 * @param manifest      Manifest to verify
 * @param trusted_key   Trusted public key (32 bytes)
 *
 * @return true if signature is valid, false otherwise
 */
bool meshgrid_ota_manifest_verify(
    const struct meshgrid_ota_manifest *manifest,
    const uint8_t *trusted_key
);

/**
 * Check if manifest is complete and valid
 *
 * @param manifest  Manifest to check
 * @return true if valid, false if incomplete or invalid
 */
bool meshgrid_ota_manifest_is_valid(const struct meshgrid_ota_manifest *manifest);

/*
 * Serialization (for OTA_MANIFEST packets)
 */

/**
 * Encode manifest to packet payload
 *
 * Format:
 * [session_id(4)][version(32)][firmware_size(4)][chunk_size(2)]
 * [total_chunks(4)][firmware_hash(32)][signature(64)][pubkey(32)]
 *
 * Total: 174 bytes
 *
 * @param manifest  Manifest to encode
 * @param payload   Output buffer (must be >= 174 bytes)
 * @param max_len   Size of output buffer
 *
 * @return Number of bytes written, or -1 on error
 */
int meshgrid_ota_manifest_encode(
    const struct meshgrid_ota_manifest *manifest,
    uint8_t *payload,
    size_t max_len
);

/**
 * Decode manifest from packet payload
 *
 * @param payload   Input buffer (174 bytes)
 * @param len       Length of payload
 * @param manifest  Output manifest structure
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_ota_manifest_decode(
    const uint8_t *payload,
    size_t len,
    struct meshgrid_ota_manifest *manifest
);

/*
 * Utility functions
 */

/**
 * Calculate number of chunks for given firmware size
 *
 * @param firmware_size  Total firmware size (bytes)
 * @param chunk_size     Bytes per chunk
 *
 * @return Number of chunks needed
 */
uint32_t meshgrid_ota_calc_total_chunks(uint32_t firmware_size, uint16_t chunk_size);

/**
 * Get chunk offset in firmware for given chunk index
 *
 * @param chunk_index  Chunk index (0-based)
 * @param chunk_size   Bytes per chunk
 *
 * @return Byte offset in firmware
 */
uint32_t meshgrid_ota_chunk_offset(uint16_t chunk_index, uint16_t chunk_size);

/**
 * Get chunk data length for given chunk index
 *
 * Last chunk may be shorter than chunk_size if firmware doesn't align.
 *
 * @param chunk_index    Chunk index (0-based)
 * @param chunk_size     Bytes per chunk
 * @param firmware_size  Total firmware size
 *
 * @return Number of bytes in this chunk
 */
uint16_t meshgrid_ota_chunk_length(
    uint16_t chunk_index,
    uint16_t chunk_size,
    uint32_t firmware_size
);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_OTA_MANIFEST_H */
