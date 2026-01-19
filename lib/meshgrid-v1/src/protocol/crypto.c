/**
 * meshgrid v1 Cryptography Implementation
 *
 * Uses mbedtls for:
 * - AES-256-GCM (hardware accelerated on ESP32)
 * - HMAC-SHA256 (hardware accelerated on ESP32)
 *
 * Platform support:
 * - ESP32/ESP32-S3: Hardware acceleration
 * - nRF52840: Software implementation
 * - RP2040: Software implementation
 */

#include "crypto.h"
#include <string.h>

/* mbedtls headers */
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

/* Platform-specific random number generator */
#if defined(ARDUINO_ARCH_ESP32)
    #include <Arduino.h>
    #define GET_RANDOM_BYTE() (esp_random() & 0xFF)
#elif defined(ARDUINO_ARCH_NRF52)
    #include <Arduino.h>
    extern uint32_t nrf52_random(void);
    #define GET_RANDOM_BYTE() (nrf52_random() & 0xFF)
#else
    #include <stdlib.h>
    #define GET_RANDOM_BYTE() (rand() & 0xFF)
#endif

/*
 * AES-256-GCM Authenticated Encryption
 */

int meshgrid_v1_aes_gcm_encrypt(
    const uint8_t *key,
    const uint8_t *nonce,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *plaintext,
    size_t pt_len,
    uint8_t *ciphertext,
    uint8_t *tag
) {
    mbedtls_gcm_context gcm;
    int ret;

    /* Initialize GCM context */
    mbedtls_gcm_init(&gcm);

    /* Set key (AES-256) */
    ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }

    /* Encrypt and authenticate */
    ret = mbedtls_gcm_crypt_and_tag(
        &gcm,
        MBEDTLS_GCM_ENCRYPT,
        pt_len,
        nonce,
        MESHGRID_V1_NONCE_SIZE,
        aad,
        aad_len,
        plaintext,
        ciphertext,
        MESHGRID_V1_TAG_SIZE,
        tag
    );

    /* Clean up */
    mbedtls_gcm_free(&gcm);

    return (ret == 0) ? 0 : -1;
}

int meshgrid_v1_aes_gcm_decrypt(
    const uint8_t *key,
    const uint8_t *nonce,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *ciphertext,
    size_t ct_len,
    const uint8_t *tag,
    uint8_t *plaintext
) {
    mbedtls_gcm_context gcm;
    int ret;

    /* Initialize GCM context */
    mbedtls_gcm_init(&gcm);

    /* Set key (AES-256) */
    ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        mbedtls_gcm_free(&gcm);
        return -1;
    }

    /* Decrypt and verify */
    ret = mbedtls_gcm_auth_decrypt(
        &gcm,
        ct_len,
        nonce,
        MESHGRID_V1_NONCE_SIZE,
        aad,
        aad_len,
        tag,
        MESHGRID_V1_TAG_SIZE,
        ciphertext,
        plaintext
    );

    /* Clean up */
    mbedtls_gcm_free(&gcm);

    /* Return -1 on authentication failure */
    return (ret == 0) ? 0 : -1;
}

/*
 * HMAC-SHA256 Message Authentication
 */

int meshgrid_v1_hmac_sha256(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    uint8_t *mac
) {
    const mbedtls_md_info_t *md_info;
    uint8_t full_mac[32];  /* Full SHA256 output */
    int ret;

    /* Get SHA256 MD info */
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        return -1;
    }

    /* Compute HMAC-SHA256 */
    ret = mbedtls_md_hmac(md_info, key, key_len, data, data_len, full_mac);
    if (ret != 0) {
        return -1;
    }

    /* Truncate to 16 bytes */
    memcpy(mac, full_mac, MESHGRID_V1_MAC_SIZE);

    /* Clear full MAC from memory */
    memset(full_mac, 0, sizeof(full_mac));

    return 0;
}

bool meshgrid_v1_hmac_verify(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    const uint8_t *expected_mac
) {
    uint8_t computed_mac[MESHGRID_V1_MAC_SIZE];

    /* Compute MAC */
    if (meshgrid_v1_hmac_sha256(key, key_len, data, data_len, computed_mac) != 0) {
        return false;
    }

    /* Constant-time compare */
    bool valid = meshgrid_v1_constant_time_compare(
        computed_mac,
        expected_mac,
        MESHGRID_V1_MAC_SIZE
    );

    /* Clear computed MAC from memory */
    memset(computed_mac, 0, sizeof(computed_mac));

    return valid;
}

/*
 * Nonce Generation
 */

int meshgrid_v1_generate_nonce(uint8_t *nonce, uint64_t timestamp) {
    /* First 8 bytes: timestamp (big-endian) */
    nonce[0] = (timestamp >> 56) & 0xFF;
    nonce[1] = (timestamp >> 48) & 0xFF;
    nonce[2] = (timestamp >> 40) & 0xFF;
    nonce[3] = (timestamp >> 32) & 0xFF;
    nonce[4] = (timestamp >> 24) & 0xFF;
    nonce[5] = (timestamp >> 16) & 0xFF;
    nonce[6] = (timestamp >> 8) & 0xFF;
    nonce[7] = timestamp & 0xFF;

    /* Last 4 bytes: random */
    nonce[8] = GET_RANDOM_BYTE();
    nonce[9] = GET_RANDOM_BYTE();
    nonce[10] = GET_RANDOM_BYTE();
    nonce[11] = GET_RANDOM_BYTE();

    return 0;
}

/*
 * Sequence Number Management
 */

void meshgrid_v1_peer_init(
    struct meshgrid_v1_peer_state *peer,
    uint16_t node_hash,
    const uint8_t *shared_secret
) {
    peer->node_hash = node_hash;
    memcpy(peer->shared_secret, shared_secret, 32);
    peer->last_seq_rx = 0;
    peer->next_seq_tx = 1;  /* Start at 1 (0 is reserved) */
    peer->secret_valid = true;
}

uint32_t meshgrid_v1_peer_next_seq_tx(struct meshgrid_v1_peer_state *peer) {
    uint32_t seq = peer->next_seq_tx;
    peer->next_seq_tx++;

    /* Handle wraparound (extremely rare) */
    if (peer->next_seq_tx == 0) {
        peer->next_seq_tx = 1;  /* Skip 0 */
    }

    return seq;
}

bool meshgrid_v1_peer_verify_seq_rx(
    struct meshgrid_v1_peer_state *peer,
    uint32_t seq
) {
    /* Reject sequence number 0 (reserved/invalid) */
    if (seq == 0) {
        return false;
    }

    /* Reject if sequence <= last seen (replay attack) */
    if (seq <= peer->last_seq_rx) {
        return false;
    }

    /* Accept and update */
    peer->last_seq_rx = seq;
    return true;
}

void meshgrid_v1_peer_reset_seq(struct meshgrid_v1_peer_state *peer) {
    peer->last_seq_rx = 0;
    peer->next_seq_tx = 1;
}

/*
 * 2-Byte Node Hash
 */

uint16_t meshgrid_v1_hash_pubkey(const uint8_t *pubkey) {
    uint8_t hash[32];
    uint16_t result;

    /* Compute SHA256 of public key */
    mbedtls_sha256(pubkey, 32, hash, 0);  /* 0 = SHA256, not SHA224 */

    /* Take first 2 bytes as hash */
    result = ((uint16_t)hash[0] << 8) | hash[1];

    return result;
}

/*
 * Constant-Time Comparison (timing attack prevention)
 */

bool meshgrid_v1_constant_time_compare(
    const uint8_t *a,
    const uint8_t *b,
    size_t len
) {
    uint8_t diff = 0;

    /* XOR all bytes and accumulate differences */
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }

    /* Return true only if diff is 0 (all bytes matched) */
    return (diff == 0);
}
