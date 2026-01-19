/**
 * meshgrid v1 OTA Manifest Implementation
 */

#include "manifest.h"
#include <string.h>
#include <mbedtls/sha256.h>

/* Ed25519 from lib/meshcore-v0 */
#include "../../../meshcore-v0/lib/ed25519/ed_25519.h"

/*
 * Initialization
 */

void meshgrid_ota_manifest_init(struct meshgrid_ota_manifest *manifest) {
    memset(manifest, 0, sizeof(struct meshgrid_ota_manifest));
    manifest->session_id = MESHGRID_OTA_SESSION_INVALID;
}

/*
 * Manifest creation
 */

int meshgrid_ota_manifest_create(
    struct meshgrid_ota_manifest *manifest,
    uint32_t session_id,
    const char *version,
    const uint8_t *firmware,
    uint32_t firmware_size,
    uint16_t chunk_size,
    const uint8_t *signing_key
) {
    /* Initialize */
    meshgrid_ota_manifest_init(manifest);

    /* Set fields */
    manifest->session_id = session_id;
    strncpy(manifest->version, version, MESHGRID_OTA_VERSION_MAX - 1);
    manifest->version[MESHGRID_OTA_VERSION_MAX - 1] = '\0';
    manifest->firmware_size = firmware_size;
    manifest->chunk_size = chunk_size;
    manifest->total_chunks = meshgrid_ota_calc_total_chunks(firmware_size, chunk_size);

    /* Compute SHA-256 hash of firmware */
    mbedtls_sha256(firmware, firmware_size, manifest->firmware_hash, 0);

    /* Extract public key from signing key (Ed25519 format: [seed(32)][pubkey(32)]) */
    memcpy(manifest->signing_pubkey, signing_key + 32, 32);

    /* Sign manifest (sign everything except signature itself) */
    /* Message to sign: session_id + version + size + chunks + hash + pubkey */
    uint8_t message[174];
    int msg_len = 0;

    /* session_id (4 bytes) */
    message[msg_len++] = (session_id >> 24) & 0xFF;
    message[msg_len++] = (session_id >> 16) & 0xFF;
    message[msg_len++] = (session_id >> 8) & 0xFF;
    message[msg_len++] = session_id & 0xFF;

    /* version (32 bytes) */
    memcpy(&message[msg_len], manifest->version, 32);
    msg_len += 32;

    /* firmware_size (4 bytes) */
    message[msg_len++] = (firmware_size >> 24) & 0xFF;
    message[msg_len++] = (firmware_size >> 16) & 0xFF;
    message[msg_len++] = (firmware_size >> 8) & 0xFF;
    message[msg_len++] = firmware_size & 0xFF;

    /* chunk_size (2 bytes) */
    message[msg_len++] = (chunk_size >> 8) & 0xFF;
    message[msg_len++] = chunk_size & 0xFF;

    /* total_chunks (4 bytes) */
    message[msg_len++] = (manifest->total_chunks >> 24) & 0xFF;
    message[msg_len++] = (manifest->total_chunks >> 16) & 0xFF;
    message[msg_len++] = (manifest->total_chunks >> 8) & 0xFF;
    message[msg_len++] = manifest->total_chunks & 0xFF;

    /* firmware_hash (32 bytes) */
    memcpy(&message[msg_len], manifest->firmware_hash, 32);
    msg_len += 32;

    /* signing_pubkey (32 bytes) */
    memcpy(&message[msg_len], manifest->signing_pubkey, 32);
    msg_len += 32;

    /* Sign message */
    ed_25519_sign(manifest->signature, message, msg_len, signing_key);

    return 0;
}

/*
 * Verification
 */

bool meshgrid_ota_manifest_verify(
    const struct meshgrid_ota_manifest *manifest,
    const uint8_t *trusted_key
) {
    /* Build message to verify (same as signing) */
    uint8_t message[174];
    int msg_len = 0;

    /* session_id */
    message[msg_len++] = (manifest->session_id >> 24) & 0xFF;
    message[msg_len++] = (manifest->session_id >> 16) & 0xFF;
    message[msg_len++] = (manifest->session_id >> 8) & 0xFF;
    message[msg_len++] = manifest->session_id & 0xFF;

    /* version */
    memcpy(&message[msg_len], manifest->version, 32);
    msg_len += 32;

    /* firmware_size */
    message[msg_len++] = (manifest->firmware_size >> 24) & 0xFF;
    message[msg_len++] = (manifest->firmware_size >> 16) & 0xFF;
    message[msg_len++] = (manifest->firmware_size >> 8) & 0xFF;
    message[msg_len++] = manifest->firmware_size & 0xFF;

    /* chunk_size */
    message[msg_len++] = (manifest->chunk_size >> 8) & 0xFF;
    message[msg_len++] = manifest->chunk_size & 0xFF;

    /* total_chunks */
    message[msg_len++] = (manifest->total_chunks >> 24) & 0xFF;
    message[msg_len++] = (manifest->total_chunks >> 16) & 0xFF;
    message[msg_len++] = (manifest->total_chunks >> 8) & 0xFF;
    message[msg_len++] = manifest->total_chunks & 0xFF;

    /* firmware_hash */
    memcpy(&message[msg_len], manifest->firmware_hash, 32);
    msg_len += 32;

    /* signing_pubkey */
    memcpy(&message[msg_len], manifest->signing_pubkey, 32);
    msg_len += 32;

    /* Verify signature using either signing_pubkey or trusted_key */
    /* If trusted_key is provided, verify signing_pubkey matches */
    if (trusted_key != NULL) {
        if (memcmp(manifest->signing_pubkey, trusted_key, 32) != 0) {
            return false;  /* Wrong signer */
        }
    }

    /* Verify Ed25519 signature */
    return ed_25519_verify(manifest->signature, message, msg_len, manifest->signing_pubkey);
}

bool meshgrid_ota_manifest_is_valid(const struct meshgrid_ota_manifest *manifest) {
    /* Check required fields */
    if (manifest->session_id == MESHGRID_OTA_SESSION_INVALID) {
        return false;
    }

    if (manifest->firmware_size == 0) {
        return false;
    }

    if (manifest->chunk_size == 0) {
        return false;
    }

    if (manifest->total_chunks == 0) {
        return false;
    }

    return true;
}

/*
 * Serialization
 */

int meshgrid_ota_manifest_encode(
    const struct meshgrid_ota_manifest *manifest,
    uint8_t *payload,
    size_t max_len
) {
    if (max_len < 174) {
        return -1;  /* Buffer too small */
    }

    int pos = 0;

    /* session_id (4 bytes) */
    payload[pos++] = (manifest->session_id >> 24) & 0xFF;
    payload[pos++] = (manifest->session_id >> 16) & 0xFF;
    payload[pos++] = (manifest->session_id >> 8) & 0xFF;
    payload[pos++] = manifest->session_id & 0xFF;

    /* version (32 bytes) */
    memcpy(&payload[pos], manifest->version, 32);
    pos += 32;

    /* firmware_size (4 bytes) */
    payload[pos++] = (manifest->firmware_size >> 24) & 0xFF;
    payload[pos++] = (manifest->firmware_size >> 16) & 0xFF;
    payload[pos++] = (manifest->firmware_size >> 8) & 0xFF;
    payload[pos++] = manifest->firmware_size & 0xFF;

    /* chunk_size (2 bytes) */
    payload[pos++] = (manifest->chunk_size >> 8) & 0xFF;
    payload[pos++] = manifest->chunk_size & 0xFF;

    /* total_chunks (4 bytes) */
    payload[pos++] = (manifest->total_chunks >> 24) & 0xFF;
    payload[pos++] = (manifest->total_chunks >> 16) & 0xFF;
    payload[pos++] = (manifest->total_chunks >> 8) & 0xFF;
    payload[pos++] = manifest->total_chunks & 0xFF;

    /* firmware_hash (32 bytes) */
    memcpy(&payload[pos], manifest->firmware_hash, 32);
    pos += 32;

    /* signature (64 bytes) */
    memcpy(&payload[pos], manifest->signature, 64);
    pos += 64;

    /* signing_pubkey (32 bytes) */
    memcpy(&payload[pos], manifest->signing_pubkey, 32);
    pos += 32;

    return pos;  /* 174 bytes */
}

int meshgrid_ota_manifest_decode(
    const uint8_t *payload,
    size_t len,
    struct meshgrid_ota_manifest *manifest
) {
    if (len < 174) {
        return -1;  /* Payload too small */
    }

    int pos = 0;

    /* session_id (4 bytes) */
    manifest->session_id = ((uint32_t)payload[pos] << 24) |
                           ((uint32_t)payload[pos + 1] << 16) |
                           ((uint32_t)payload[pos + 2] << 8) |
                           payload[pos + 3];
    pos += 4;

    /* version (32 bytes) */
    memcpy(manifest->version, &payload[pos], 32);
    manifest->version[31] = '\0';  /* Ensure null-terminated */
    pos += 32;

    /* firmware_size (4 bytes) */
    manifest->firmware_size = ((uint32_t)payload[pos] << 24) |
                              ((uint32_t)payload[pos + 1] << 16) |
                              ((uint32_t)payload[pos + 2] << 8) |
                              payload[pos + 3];
    pos += 4;

    /* chunk_size (2 bytes) */
    manifest->chunk_size = ((uint16_t)payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    /* total_chunks (4 bytes) */
    manifest->total_chunks = ((uint32_t)payload[pos] << 24) |
                             ((uint32_t)payload[pos + 1] << 16) |
                             ((uint32_t)payload[pos + 2] << 8) |
                             payload[pos + 3];
    pos += 4;

    /* firmware_hash (32 bytes) */
    memcpy(manifest->firmware_hash, &payload[pos], 32);
    pos += 32;

    /* signature (64 bytes) */
    memcpy(manifest->signature, &payload[pos], 64);
    pos += 64;

    /* signing_pubkey (32 bytes) */
    memcpy(manifest->signing_pubkey, &payload[pos], 32);
    pos += 32;

    return 0;
}

/*
 * Utility functions
 */

uint32_t meshgrid_ota_calc_total_chunks(uint32_t firmware_size, uint16_t chunk_size) {
    if (chunk_size == 0) {
        return 0;
    }
    return (firmware_size + chunk_size - 1) / chunk_size;  /* Round up */
}

uint32_t meshgrid_ota_chunk_offset(uint16_t chunk_index, uint16_t chunk_size) {
    return (uint32_t)chunk_index * chunk_size;
}

uint16_t meshgrid_ota_chunk_length(
    uint16_t chunk_index,
    uint16_t chunk_size,
    uint32_t firmware_size
) {
    uint32_t offset = meshgrid_ota_chunk_offset(chunk_index, chunk_size);
    uint32_t remaining = firmware_size - offset;

    if (remaining >= chunk_size) {
        return chunk_size;
    } else {
        return (uint16_t)remaining;
    }
}
