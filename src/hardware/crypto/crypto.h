/**
 * meshgrid crypto - Ed25519 keys and AES-128 encryption
 *
 * Provides MeshCore-compatible cryptography:
 * - Ed25519 key generation and signatures
 * - X25519 key exchange (via Ed25519 keys)
 * - AES-128 encryption for messages
 * - SHA256 for MAC generation
 */

#ifndef MESHGRID_CRYPTO_H
#define MESHGRID_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Key sizes (MeshCore compatible) */
#define CRYPTO_PUBKEY_SIZE 32
#define CRYPTO_PRIVKEY_SIZE 64
#define CRYPTO_SEED_SIZE 32
#define CRYPTO_SIGNATURE_SIZE 64
#define CRYPTO_SHARED_SECRET_SIZE 32

/* Cipher sizes (MeshCore compatible) */
#define CRYPTO_AES_KEY_SIZE 16
#define CRYPTO_AES_BLOCK_SIZE 16
#define CRYPTO_MAC_SIZE 2 /* MeshCore uses truncated MAC */

/* Hash sizes */
#define CRYPTO_SHA256_SIZE 32
#define CRYPTO_PATH_HASH_SIZE 1 /* MeshCore compatible */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the crypto subsystem
 * Must be called before any crypto operations
 */
void crypto_init(void);

/**
 * Generate a new Ed25519 keypair
 * @param pubkey Output: 32-byte public key
 * @param privkey Output: 64-byte private key
 * @return 0 on success, -1 on error
 */
int crypto_generate_keypair(uint8_t* pubkey, uint8_t* privkey);

/**
 * Sign a message with Ed25519
 * @param signature Output: 64-byte signature
 * @param message The message to sign
 * @param message_len Length of message
 * @param pubkey 32-byte public key
 * @param privkey 64-byte private key
 */
void crypto_sign(uint8_t* signature, const uint8_t* message, size_t message_len, const uint8_t* pubkey,
                 const uint8_t* privkey);

/**
 * Verify an Ed25519 signature
 * @param signature 64-byte signature to verify
 * @param message The original message
 * @param message_len Length of message
 * @param pubkey 32-byte public key of signer
 * @return true if signature is valid
 */
bool crypto_verify(const uint8_t* signature, const uint8_t* message, size_t message_len, const uint8_t* pubkey);

/**
 * Perform X25519 key exchange to derive shared secret
 * @param shared_secret Output: 32-byte shared secret
 * @param our_privkey Our 64-byte private key
 * @param their_pubkey Their 32-byte public key
 */
void crypto_key_exchange(uint8_t* shared_secret, const uint8_t* our_privkey, const uint8_t* their_pubkey);

/**
 * Encrypt data with AES-128
 * Uses first 16 bytes of shared_secret as key
 * @param dest Output buffer (must be >= src_len rounded up to 16)
 * @param src Data to encrypt
 * @param src_len Length of data
 * @param shared_secret 32-byte shared secret from key exchange
 * @return Length of encrypted data (multiple of 16)
 */
int crypto_encrypt(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret);

/**
 * Decrypt data with AES-128
 * @param dest Output buffer
 * @param src Encrypted data
 * @param src_len Length of encrypted data (must be multiple of 16)
 * @param shared_secret 32-byte shared secret from key exchange
 * @return Length of decrypted data
 */
int crypto_decrypt(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret);

/**
 * Encrypt then MAC (MeshCore compatible)
 * Prepends 2-byte MAC to ciphertext
 * @param dest Output: MAC + ciphertext
 * @param src Plaintext
 * @param src_len Length of plaintext
 * @param shared_secret 32-byte shared secret
 * @return Total length (MAC_SIZE + ciphertext length)
 */
int crypto_encrypt_then_mac(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret);

/**
 * Verify MAC then decrypt (MeshCore compatible)
 * @param dest Output plaintext
 * @param src MAC + ciphertext
 * @param src_len Total length
 * @param shared_secret 32-byte shared secret
 * @return Plaintext length, or 0 if MAC invalid
 */
int crypto_mac_then_decrypt(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret);

/**
 * Compute SHA256 hash
 * @param hash Output buffer
 * @param hash_len Desired output length (truncated if < 32)
 * @param data Input data
 * @param data_len Length of input
 */
void crypto_sha256(uint8_t* hash, size_t hash_len, const uint8_t* data, size_t data_len);

/**
 * Get random bytes
 * @param dest Output buffer
 * @param len Number of random bytes
 */
void crypto_random(uint8_t* dest, size_t len);

/**
 * Compute MeshCore-compatible 1-byte hash from public key
 * (Just first byte of pubkey)
 * @param pubkey 32-byte public key
 * @return 1-byte hash
 */
uint8_t crypto_hash_pubkey(const uint8_t* pubkey);

/* ========== Protocol v1 (Enhanced Security) ========== */

#define CRYPTO_V1_MAC_SIZE 16   /* Full 16-byte HMAC */
#define CRYPTO_V1_NONCE_SIZE 12 /* 96-bit nonce for CTR mode */

/**
 * Encrypt with AES-256-CTR + 16-byte HMAC (Protocol v1)
 * @param dest Output: nonce(12) + MAC(16) + ciphertext
 * @param src Plaintext
 * @param src_len Length of plaintext
 * @param shared_secret 32-byte shared secret (full key)
 * @param nonce 12-byte nonce (must be unique per message)
 * @return Total length (28 + ciphertext length), or -1 on error
 */
int crypto_encrypt_v1(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret,
                      const uint8_t* nonce);

/**
 * Verify HMAC and decrypt with AES-256-CTR (Protocol v1)
 * @param dest Output plaintext
 * @param src nonce(12) + MAC(16) + ciphertext
 * @param src_len Total length
 * @param shared_secret 32-byte shared secret (full key)
 * @return Plaintext length, or 0 if MAC invalid
 */
int crypto_decrypt_v1(uint8_t* dest, const uint8_t* src, int src_len, const uint8_t* shared_secret);

/**
 * Generate a unique nonce for v1 encryption
 * Format: timestamp(4) + random(8)
 * @param nonce Output: 12-byte nonce
 */
void crypto_generate_nonce(uint8_t* nonce);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_CRYPTO_H */
