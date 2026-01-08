/**
 * meshgrid crypto implementation
 *
 * Uses Ed25519 for signatures and key exchange
 * Uses mbedtls (built into ESP32) for AES and SHA256
 */

#include "crypto.h"
#include <string.h>

/* Ed25519 library */
#include "../../lib/ed25519/ed_25519.h"

/* Platform-specific includes */
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#elif defined(ARDUINO_ARCH_NRF52)
#include <nrf_crypto.h>
#else
#include <stdlib.h>
#endif

static bool crypto_initialized = false;

void crypto_init(void) {
    if (crypto_initialized) return;
    crypto_initialized = true;
    /* Platform-specific init if needed */
}

void crypto_random(uint8_t *dest, size_t len) {
#if defined(ARDUINO_ARCH_ESP32)
    esp_fill_random(dest, len);
#elif defined(ARDUINO_ARCH_NRF52)
    /* Use Nordic RNG */
    for (size_t i = 0; i < len; i++) {
        dest[i] = (uint8_t)random();
    }
#else
    for (size_t i = 0; i < len; i++) {
        dest[i] = (uint8_t)rand();
    }
#endif
}

int crypto_generate_keypair(uint8_t *pubkey, uint8_t *privkey) {
    uint8_t seed[CRYPTO_SEED_SIZE];
    crypto_random(seed, CRYPTO_SEED_SIZE);
    ed25519_create_keypair(pubkey, privkey, seed);
    /* Clear seed from memory */
    memset(seed, 0, CRYPTO_SEED_SIZE);
    return 0;
}

void crypto_sign(uint8_t *signature, const uint8_t *message, size_t message_len,
                 const uint8_t *pubkey, const uint8_t *privkey) {
    ed25519_sign(signature, message, message_len, pubkey, privkey);
}

bool crypto_verify(const uint8_t *signature, const uint8_t *message,
                   size_t message_len, const uint8_t *pubkey) {
    return ed25519_verify(signature, message, message_len, pubkey) == 1;
}

void crypto_key_exchange(uint8_t *shared_secret, const uint8_t *our_privkey,
                         const uint8_t *their_pubkey) {
    ed25519_key_exchange(shared_secret, their_pubkey, our_privkey);
}

void crypto_sha256(uint8_t *hash, size_t hash_len, const uint8_t *data, size_t data_len) {
#if defined(ARDUINO_ARCH_ESP32)
    uint8_t full_hash[32];
    mbedtls_sha256(data, data_len, full_hash, 0);
    size_t copy_len = (hash_len < 32) ? hash_len : 32;
    memcpy(hash, full_hash, copy_len);
#else
    /* Fallback: simple hash for platforms without mbedtls */
    /* This is NOT cryptographically secure - just for compilation */
    memset(hash, 0, hash_len);
    for (size_t i = 0; i < data_len; i++) {
        hash[i % hash_len] ^= data[i];
    }
#endif
}

int crypto_encrypt(uint8_t *dest, const uint8_t *src, int src_len,
                   const uint8_t *shared_secret) {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, shared_secret, 128);

    /* Pad to block size */
    int padded_len = ((src_len + CRYPTO_AES_BLOCK_SIZE - 1) / CRYPTO_AES_BLOCK_SIZE) * CRYPTO_AES_BLOCK_SIZE;

    /* Copy and zero-pad source */
    uint8_t padded[256];  /* Max reasonable size */
    if (padded_len > (int)sizeof(padded)) padded_len = sizeof(padded);
    memset(padded, 0, padded_len);
    memcpy(padded, src, src_len);

    /* Encrypt each block (ECB mode for MeshCore compat) */
    for (int i = 0; i < padded_len; i += CRYPTO_AES_BLOCK_SIZE) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, padded + i, dest + i);
    }

    mbedtls_aes_free(&aes);
    return padded_len;
#else
    /* No encryption on unsupported platforms */
    memcpy(dest, src, src_len);
    return src_len;
#endif
}

int crypto_decrypt(uint8_t *dest, const uint8_t *src, int src_len,
                   const uint8_t *shared_secret) {
#if defined(ARDUINO_ARCH_ESP32)
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, shared_secret, 128);

    /* Decrypt each block */
    for (int i = 0; i < src_len; i += CRYPTO_AES_BLOCK_SIZE) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, src + i, dest + i);
    }

    mbedtls_aes_free(&aes);
    return src_len;
#else
    memcpy(dest, src, src_len);
    return src_len;
#endif
}

int crypto_encrypt_then_mac(uint8_t *dest, const uint8_t *src, int src_len,
                            const uint8_t *shared_secret) {
    /* Encrypt first */
    int cipher_len = crypto_encrypt(dest + CRYPTO_MAC_SIZE, src, src_len, shared_secret);

    /* Calculate HMAC-SHA256 over ciphertext (MeshCore compatible) */
#if defined(ARDUINO_ARCH_ESP32)
    uint8_t hmac[CRYPTO_SHA256_SIZE];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, shared_secret, CRYPTO_SHARED_SECRET_SIZE);
    mbedtls_md_hmac_update(&ctx, dest + CRYPTO_MAC_SIZE, cipher_len);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    /* Prepend truncated HMAC (2 bytes) */
    dest[0] = hmac[0];
    dest[1] = hmac[1];
#else
    /* Fallback: use plain SHA256 */
    uint8_t mac[CRYPTO_SHA256_SIZE];
    crypto_sha256(mac, CRYPTO_SHA256_SIZE, dest + CRYPTO_MAC_SIZE, cipher_len);
    dest[0] = mac[0];
    dest[1] = mac[1];
#endif

    return CRYPTO_MAC_SIZE + cipher_len;
}

int crypto_mac_then_decrypt(uint8_t *dest, const uint8_t *src, int src_len,
                            const uint8_t *shared_secret) {
    if (src_len < CRYPTO_MAC_SIZE + CRYPTO_AES_BLOCK_SIZE) {
        return 0;  /* Too short */
    }

    /* Extract MAC */
    uint8_t expected_mac[2] = { src[0], src[1] };

    /* Calculate HMAC-SHA256 over ciphertext (MeshCore compatible) */
#if defined(ARDUINO_ARCH_ESP32)
    uint8_t hmac[CRYPTO_SHA256_SIZE];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, shared_secret, CRYPTO_SHARED_SECRET_SIZE);
    mbedtls_md_hmac_update(&ctx, src + CRYPTO_MAC_SIZE, src_len - CRYPTO_MAC_SIZE);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    /* Verify HMAC (constant-time compare would be better) */
    if (expected_mac[0] != hmac[0] || expected_mac[1] != hmac[1]) {
        return 0;  /* MAC mismatch */
    }
#else
    /* Fallback: use plain SHA256 */
    uint8_t actual_mac[CRYPTO_SHA256_SIZE];
    crypto_sha256(actual_mac, CRYPTO_SHA256_SIZE, src + CRYPTO_MAC_SIZE, src_len - CRYPTO_MAC_SIZE);
    if (expected_mac[0] != actual_mac[0] || expected_mac[1] != actual_mac[1]) {
        return 0;  /* MAC mismatch */
    }
#endif

    /* Decrypt */
    return crypto_decrypt(dest, src + CRYPTO_MAC_SIZE, src_len - CRYPTO_MAC_SIZE, shared_secret);
}

uint8_t crypto_hash_pubkey(const uint8_t *pubkey) {
    /* MeshCore uses first byte of pubkey as hash */
    return pubkey[0];
}
