/**
 * meshgrid v1 Bloom Filters
 *
 * Attenuated multi-level bloom filters for efficient multi-hop neighbor discovery.
 *
 * Design:
 * - 4 levels of bloom filters (64 bits each = 8 bytes)
 * - Level 0: Direct neighbors (0-1 hops)
 * - Level 1: Near neighbors (2-3 hops)
 * - Level 2: Mid-range neighbors (4-6 hops)
 * - Level 3: Distant neighbors (7+ hops)
 *
 * As advertisements propagate, bloom filters are "attenuated" (shifted down levels)
 * to reflect increasing distance from source nodes.
 *
 * Benefits vs v0:
 * - v0: 12 hours per hop discovery (slow)
 * - v1: Minutes to discover 5+ hop neighbors (fast)
 */

#ifndef MESHGRID_V1_BLOOM_H
#define MESHGRID_V1_BLOOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bloom filter constants
 */
#define MESHGRID_BLOOM_LEVELS      4     /* 4 distance levels */
#define MESHGRID_BLOOM_LEVEL_BITS  64    /* 64 bits per level (8 bytes) */
#define MESHGRID_BLOOM_LEVEL_BYTES 8     /* 8 bytes per level */
#define MESHGRID_BLOOM_TOTAL_BYTES 32    /* 4 levels × 8 bytes */

/*
 * Multi-level bloom filter set
 */
struct meshgrid_bloom_set {
    uint8_t level0[8];  /* Direct neighbors (0-1 hops) */
    uint8_t level1[8];  /* Near neighbors (2-3 hops) */
    uint8_t level2[8];  /* Mid-range (4-6 hops) */
    uint8_t level3[8];  /* Distant (7+ hops) */
};

/*
 * Initialization
 */

/**
 * Initialize empty bloom filter set
 *
 * @param bloom  Bloom filter set to initialize
 */
void meshgrid_bloom_init(struct meshgrid_bloom_set *bloom);

/**
 * Clear all bloom filters (set all bits to 0)
 *
 * @param bloom  Bloom filter set to clear
 */
void meshgrid_bloom_clear(struct meshgrid_bloom_set *bloom);

/*
 * Adding nodes to bloom filters
 */

/**
 * Add a node hash to a specific level
 *
 * Uses 2 hash functions to set 2 bits in the 64-bit filter.
 *
 * @param bloom  Bloom filter set
 * @param level  Level to add to (0-3)
 * @param hash   16-bit node hash
 */
void meshgrid_bloom_add(
    struct meshgrid_bloom_set *bloom,
    uint8_t level,
    uint16_t hash
);

/**
 * Add our own node to level 0 (always present)
 *
 * @param bloom     Bloom filter set
 * @param our_hash  Our 16-bit node hash
 */
void meshgrid_bloom_add_self(
    struct meshgrid_bloom_set *bloom,
    uint16_t our_hash
);

/*
 * Checking bloom filters
 */

/**
 * Check if a node hash is present in any level
 *
 * @param bloom  Bloom filter set
 * @param hash   16-bit node hash to check
 *
 * @return Level where found (0-3), or -1 if not found
 */
int meshgrid_bloom_check(
    const struct meshgrid_bloom_set *bloom,
    uint16_t hash
);

/**
 * Check if a node hash is present in a specific level
 *
 * @param bloom  Bloom filter set
 * @param level  Level to check (0-3)
 * @param hash   16-bit node hash
 *
 * @return true if likely present (may have false positives), false if definitely absent
 */
bool meshgrid_bloom_check_level(
    const struct meshgrid_bloom_set *bloom,
    uint8_t level,
    uint16_t hash
);

/*
 * Bloom filter operations
 */

/**
 * Attenuate bloom filters when forwarding (shift distant nodes out)
 *
 * Process:
 * - Level 0 → Level 1 (direct neighbors become near)
 * - Level 1 → Level 2 (near neighbors become mid-range)
 * - Level 2 → Level 3 (mid-range become distant)
 * - Level 3 → discarded (too distant)
 *
 * Then add ourselves to level 0.
 *
 * @param bloom     Bloom filter set to attenuate
 * @param our_hash  Our node hash (added to level 0)
 */
void meshgrid_bloom_attenuate(
    struct meshgrid_bloom_set *bloom,
    uint16_t our_hash
);

/**
 * Merge two bloom filter sets (bitwise OR)
 *
 * Used to combine neighbor information from multiple sources.
 *
 * @param dest  Destination bloom filter (modified)
 * @param src   Source bloom filter (unchanged)
 */
void meshgrid_bloom_merge(
    struct meshgrid_bloom_set *dest,
    const struct meshgrid_bloom_set *src
);

/**
 * Copy bloom filter set
 *
 * @param dest  Destination
 * @param src   Source
 */
void meshgrid_bloom_copy(
    struct meshgrid_bloom_set *dest,
    const struct meshgrid_bloom_set *src
);

/*
 * Statistics
 */

/**
 * Count approximate number of nodes in a bloom filter level
 *
 * Uses bit count estimation formula:
 * n ≈ -m/k * ln(1 - X/m)
 * where m = bits, k = hash functions, X = set bits
 *
 * @param bloom  Bloom filter set
 * @param level  Level to count (0-3)
 *
 * @return Estimated number of nodes in this level
 */
uint8_t meshgrid_bloom_count_level(
    const struct meshgrid_bloom_set *bloom,
    uint8_t level
);

/**
 * Count total approximate nodes across all levels
 *
 * @param bloom  Bloom filter set
 * @return Estimated total nodes visible (may have overlap)
 */
uint16_t meshgrid_bloom_count_total(const struct meshgrid_bloom_set *bloom);

/**
 * Check if bloom filter is empty (all zeros)
 *
 * @param bloom  Bloom filter set
 * @return true if empty, false if has any bits set
 */
bool meshgrid_bloom_is_empty(const struct meshgrid_bloom_set *bloom);

/*
 * Serialization (for packets)
 */

/**
 * Encode bloom filter set to bytes (32 bytes)
 *
 * Format: [level0(8)][level1(8)][level2(8)][level3(8)]
 *
 * @param bloom  Bloom filter set
 * @param buf    Output buffer (must be >= 32 bytes)
 *
 * @return Number of bytes written (always 32)
 */
int meshgrid_bloom_encode(
    const struct meshgrid_bloom_set *bloom,
    uint8_t *buf
);

/**
 * Decode bloom filter set from bytes (32 bytes)
 *
 * @param buf    Input buffer (32 bytes)
 * @param bloom  Output bloom filter set
 *
 * @return 0 on success, -1 on error
 */
int meshgrid_bloom_decode(
    const uint8_t *buf,
    struct meshgrid_bloom_set *bloom
);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_V1_BLOOM_H */
