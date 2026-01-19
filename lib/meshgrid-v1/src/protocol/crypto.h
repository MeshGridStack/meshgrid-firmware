/**
 * meshgrid v1 Cryptography
 *
 * Enhanced security compared to v0 (MeshCore):
 * - AES-256-GCM authenticated encryption (vs AES-128 ECB)
 * - 16-byte HMAC-SHA256 (vs 2-byte MAC)
 * - 12-byte nonces (vs no nonce)
 * - 4-byte sequence numbers for replay protection
 * - 2-byte node hashes (vs 1-byte)
 */

#ifndef MESHGRID_V1_CRYPTO_H
#define MESHGRID_V1_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Crypto constants (v1)
 */
#define MESHGRID_V1_KEY_SIZE           32   /* AES-256 key */
#define MESHGRID_V1_NONCE_SIZE         12   /* GCM nonce */
#define MESHGRID_V1_TAG_SIZE           16   /* GCM authentication tag */
#define MESHGRID_V1_MAC_SIZE           16   /* HMAC-SHA256 (truncated to 16 bytes) */
#define MESHGRID_V1_HASH_SIZE          2    /* 2-byte node hash */
#define MESHGRID_V1_SEQUENCE_SIZE      4    /* 4-byte sequence number */

/*
 * Peer crypto state (for sequence number tracking)
 */
struct meshgrid_v1_peer_state {
    uint16_t node_hash;                     /* 2-byte node hash */
    uint8_t shared_secret[32];              /* ECDH shared secret */
    uint32_t last_seq_rx;                   /* Last received sequence */
    uint32_t next_seq_tx;                   /* Next sequence to send */
    bool secret_valid;                      /* True if shared_secret is valid */
};

/*
 * AES-256-GCM Authenticated Encryption
 */

/**
 * Encrypt data using AES-256-GCM
 *
 * @param key        32-byte encryption key (shared secret or channel PSK)
 * @param nonce      12-byte nonce (must be unique per message with same key)
 * @param aad        Additional authenticated data (can be NULL if aad_len=0)
 * @param aad_len    Length of AAD
 * @param plaintext  Data to encrypt
 * @param pt_len     Length of plaintext
 * @param ciphertext Output buffer for ciphertext (must be >= pt_len)
 * @param tag        Output buffer for 16-byte authentication tag
 *
 * @return 0 on success, -1 on error
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
);

/**
 * Decrypt data using AES-256-GCM
 *
 * @param key        32-byte encryption key (shared secret or channel PSK)
 * @param nonce      12-byte nonce (same as used for encryption)
 * @param aad        Additional authenticated data (can be NULL if aad_len=0)
 * @param aad_len    Length of AAD
 * @param ciphertext Data to decrypt
 * @param ct_len     Length of ciphertext
 * @param tag        16-byte authentication tag
 * @param plaintext  Output buffer for plaintext (must be >= ct_len)
 *
 * @return 0 on success (authenticated), -1 on error or authentication failure
 */
int meshgrid_v1_aes_gcm_decrypt(
    const uint8_t *key,
    const uint8_t *nonce,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t *ciphertext,
    size_t ct_len,
    const uint8_t *tag,
    uint8_t *plaintext
);

/*
 * HMAC-SHA256 Message Authentication
 */

/**
 * Compute HMAC-SHA256 (16-byte output)
 *
 * Note: Full SHA256 output is 32 bytes, but we truncate to 16 bytes
 * for compatibility with packet size constraints. This still provides
 * strong authentication (2^128 security vs v0's 2^16).
 *
 * @param key      Shared secret or channel PSK
 * @param key_len  Length of key (typically 32 bytes)
 * @param data     Data to authenticate
 * @param data_len Length of data
 * @param mac      Output buffer for 16-byte MAC
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_v1_hmac_sha256(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    uint8_t *mac
);

/**
 * Verify HMAC-SHA256 (constant-time comparison)
 *
 * @param key         Shared secret or channel PSK
 * @param key_len     Length of key
 * @param data        Data to verify
 * @param data_len    Length of data
 * @param expected_mac Expected 16-byte MAC
 *
 * @return true if MAC is valid, false otherwise
 */
bool meshgrid_v1_hmac_verify(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    const uint8_t *expected_mac
);

/*
 * Nonce Generation
 */

/**
 * Generate a unique nonce for AES-GCM
 *
 * Format: [timestamp (8 bytes)] [random (4 bytes)]
 * This ensures nonces are unique even if random number generator repeats.
 *
 * @param nonce  Output buffer for 12-byte nonce
 * @param timestamp Current timestamp (e.g., millis() or RTC time)
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_v1_generate_nonce(uint8_t *nonce, uint64_t timestamp);

/*
 * Sequence Number Management (Replay Protection)
 */

/**
 * Initialize peer state for sequence tracking
 *
 * @param peer        Peer state structure
 * @param node_hash   2-byte node hash
 * @param shared_secret 32-byte ECDH shared secret
 */
void meshgrid_v1_peer_init(
    struct meshgrid_v1_peer_state *peer,
    uint16_t node_hash,
    const uint8_t *shared_secret
);

/**
 * Get next sequence number for sending
 *
 * @param peer  Peer state
 * @return Next sequence number (automatically incremented)
 */
uint32_t meshgrid_v1_peer_next_seq_tx(struct meshgrid_v1_peer_state *peer);

/**
 * Verify received sequence number (replay protection)
 *
 * @param peer  Peer state
 * @param seq   Received sequence number
 *
 * @return true if sequence is valid (not replayed), false if rejected
 *
 * Note: This updates last_seq_rx if valid, so call only once per packet!
 */
bool meshgrid_v1_peer_verify_seq_rx(
    struct meshgrid_v1_peer_state *peer,
    uint32_t seq
);

/**
 * Reset sequence numbers (after key rotation or peer reconnect)
 *
 * @param peer  Peer state
 */
void meshgrid_v1_peer_reset_seq(struct meshgrid_v1_peer_state *peer);

/*
 * 2-Byte Node Hash (Enhanced vs v0's 1-byte)
 */

/**
 * Compute 2-byte hash from public key
 *
 * Uses SHA256(pubkey) and takes first 2 bytes.
 * Collision probability: ~0.3% at 300 nodes (vs 50% at 17 nodes for v0)
 *
 * @param pubkey  32-byte Ed25519/X25519 public key
 * @return 2-byte hash as uint16_t
 */
uint16_t meshgrid_v1_hash_pubkey(const uint8_t *pubkey);

/*
 * Utility: Constant-Time Comparison (prevent timing attacks)
 */

/**
 * Compare two buffers in constant time
 *
 * @param a    First buffer
 * @param b    Second buffer
 * @param len  Length to compare
 *
 * @return true if buffers are equal, false otherwise
 */
bool meshgrid_v1_constant_time_compare(
    const uint8_t *a,
    const uint8_t *b,
    size_t len
);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_CRYPTO_H */
