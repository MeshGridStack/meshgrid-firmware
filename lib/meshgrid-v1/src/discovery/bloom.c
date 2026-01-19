/**
 * meshgrid v1 Bloom Filter Implementation
 */

#include "bloom.h"
#include <string.h>
#include <math.h>

/*
 * Hash functions for bloom filter
 *
 * We use 2 hash functions derived from the 16-bit node hash:
 * - h1 = hash & 0x3F (first 6 bits → 0-63)
 * - h2 = (hash >> 6) & 0x3F (next 6 bits → 0-63)
 *
 * This maps the 16-bit hash to 2 bit positions in a 64-bit filter.
 */

static inline uint8_t hash1(uint16_t hash) {
    return hash & 0x3F;  /* Bits 0-5 → 0-63 */
}

static inline uint8_t hash2(uint16_t hash) {
    return (hash >> 6) & 0x3F;  /* Bits 6-11 → 0-63 */
}

/*
 * Set a bit in an 8-byte array
 */
static inline void set_bit(uint8_t *array, uint8_t bit_pos) {
    uint8_t byte_idx = bit_pos / 8;
    uint8_t bit_idx = bit_pos % 8;
    array[byte_idx] |= (1 << bit_idx);
}

/*
 * Test a bit in an 8-byte array
 */
static inline bool test_bit(const uint8_t *array, uint8_t bit_pos) {
    uint8_t byte_idx = bit_pos / 8;
    uint8_t bit_idx = bit_pos % 8;
    return (array[byte_idx] & (1 << bit_idx)) != 0;
}

/*
 * Count set bits in an 8-byte array (population count)
 */
static uint8_t popcount_64(const uint8_t *array) {
    uint8_t count = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t byte = array[i];
        /* Brian Kernighan's algorithm */
        while (byte) {
            byte &= byte - 1;
            count++;
        }
    }
    return count;
}

/*
 * Get pointer to level array
 */
static inline uint8_t* get_level_ptr(struct meshgrid_bloom_set *bloom, uint8_t level) {
    switch (level) {
        case 0: return bloom->level0;
        case 1: return bloom->level1;
        case 2: return bloom->level2;
        case 3: return bloom->level3;
        default: return NULL;
    }
}

static inline const uint8_t* get_level_ptr_const(const struct meshgrid_bloom_set *bloom, uint8_t level) {
    switch (level) {
        case 0: return bloom->level0;
        case 1: return bloom->level1;
        case 2: return bloom->level2;
        case 3: return bloom->level3;
        default: return NULL;
    }
}

/*
 * Initialization
 */

void meshgrid_bloom_init(struct meshgrid_bloom_set *bloom) {
    meshgrid_bloom_clear(bloom);
}

void meshgrid_bloom_clear(struct meshgrid_bloom_set *bloom) {
    memset(bloom, 0, sizeof(struct meshgrid_bloom_set));
}

/*
 * Adding nodes
 */

void meshgrid_bloom_add(
    struct meshgrid_bloom_set *bloom,
    uint8_t level,
    uint16_t hash
) {
    if (level >= MESHGRID_BLOOM_LEVELS) {
        return;  /* Invalid level */
    }

    uint8_t *array = get_level_ptr(bloom, level);
    if (array == NULL) {
        return;
    }

    /* Set 2 bits using 2 hash functions */
    set_bit(array, hash1(hash));
    set_bit(array, hash2(hash));
}

void meshgrid_bloom_add_self(
    struct meshgrid_bloom_set *bloom,
    uint16_t our_hash
) {
    meshgrid_bloom_add(bloom, 0, our_hash);
}

/*
 * Checking
 */

int meshgrid_bloom_check(
    const struct meshgrid_bloom_set *bloom,
    uint16_t hash
) {
    /* Check each level, return first match */
    for (uint8_t level = 0; level < MESHGRID_BLOOM_LEVELS; level++) {
        if (meshgrid_bloom_check_level(bloom, level, hash)) {
            return level;
        }
    }
    return -1;  /* Not found */
}

bool meshgrid_bloom_check_level(
    const struct meshgrid_bloom_set *bloom,
    uint8_t level,
    uint16_t hash
) {
    if (level >= MESHGRID_BLOOM_LEVELS) {
        return false;
    }

    const uint8_t *array = get_level_ptr_const(bloom, level);
    if (array == NULL) {
        return false;
    }

    /* Check both bits from both hash functions */
    return test_bit(array, hash1(hash)) && test_bit(array, hash2(hash));
}

/*
 * Operations
 */

void meshgrid_bloom_attenuate(
    struct meshgrid_bloom_set *bloom,
    uint16_t our_hash
) {
    /* Shift levels: 0→1, 1→2, 2→3, 3→discard */
    memcpy(bloom->level3, bloom->level2, 8);
    memcpy(bloom->level2, bloom->level1, 8);
    memcpy(bloom->level1, bloom->level0, 8);

    /* Clear level 0 and add ourselves */
    memset(bloom->level0, 0, 8);
    meshgrid_bloom_add_self(bloom, our_hash);
}

void meshgrid_bloom_merge(
    struct meshgrid_bloom_set *dest,
    const struct meshgrid_bloom_set *src
) {
    /* Bitwise OR each level */
    for (int i = 0; i < 8; i++) {
        dest->level0[i] |= src->level0[i];
        dest->level1[i] |= src->level1[i];
        dest->level2[i] |= src->level2[i];
        dest->level3[i] |= src->level3[i];
    }
}

void meshgrid_bloom_copy(
    struct meshgrid_bloom_set *dest,
    const struct meshgrid_bloom_set *src
) {
    memcpy(dest, src, sizeof(struct meshgrid_bloom_set));
}

/*
 * Statistics
 */

uint8_t meshgrid_bloom_count_level(
    const struct meshgrid_bloom_set *bloom,
    uint8_t level
) {
    if (level >= MESHGRID_BLOOM_LEVELS) {
        return 0;
    }

    const uint8_t *array = get_level_ptr_const(bloom, level);
    if (array == NULL) {
        return 0;
    }

    uint8_t X = popcount_64(array);  /* Set bits */

    /* Estimate number of elements using bloom filter formula:
     * n ≈ -m/k * ln(1 - X/m)
     * where m = 64 bits, k = 2 hash functions */

    if (X == 0) {
        return 0;
    }

    if (X >= 64) {
        return 255;  /* Saturated */
    }

    /* n ≈ -64/2 * ln(1 - X/64) = -32 * ln(1 - X/64) */
    float ratio = (float)X / 64.0f;
    float n = -32.0f * logf(1.0f - ratio);

    /* Round and clamp */
    if (n < 0) n = 0;
    if (n > 255) n = 255;

    return (uint8_t)n;
}

uint16_t meshgrid_bloom_count_total(const struct meshgrid_bloom_set *bloom) {
    uint16_t total = 0;
    for (uint8_t level = 0; level < MESHGRID_BLOOM_LEVELS; level++) {
        total += meshgrid_bloom_count_level(bloom, level);
    }
    return total;
}

bool meshgrid_bloom_is_empty(const struct meshgrid_bloom_set *bloom) {
    for (int i = 0; i < 8; i++) {
        if (bloom->level0[i] != 0 ||
            bloom->level1[i] != 0 ||
            bloom->level2[i] != 0 ||
            bloom->level3[i] != 0) {
            return false;
        }
    }
    return true;
}

/*
 * Serialization
 */

int meshgrid_bloom_encode(
    const struct meshgrid_bloom_set *bloom,
    uint8_t *buf
) {
    memcpy(buf, bloom->level0, 8);
    memcpy(buf + 8, bloom->level1, 8);
    memcpy(buf + 16, bloom->level2, 8);
    memcpy(buf + 24, bloom->level3, 8);
    return 32;
}

int meshgrid_bloom_decode(
    const uint8_t *buf,
    struct meshgrid_bloom_set *bloom
) {
    memcpy(bloom->level0, buf, 8);
    memcpy(bloom->level1, buf + 8, 8);
    memcpy(bloom->level2, buf + 16, 8);
    memcpy(bloom->level3, buf + 24, 8);
    return 0;
}
