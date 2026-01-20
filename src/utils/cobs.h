/**
 * COBS (Consistent Overhead Byte Stuffing)
 * Zero bytes mark packet boundaries
 * Overhead: SIZE + SIZE/254 + 1 bytes (~0.4% for typical packets)
 */

#ifndef MESHGRID_COBS_H
#define MESHGRID_COBS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode buffer using COBS
 * @param dst Output buffer (must be at least src_len + src_len/254 + 1 bytes)
 * @param src Input buffer
 * @param src_len Input length
 * @return Encoded length
 */
size_t cobs_encode(uint8_t* dst, const uint8_t* src, size_t src_len);

/**
 * Decode COBS buffer
 * @param dst Output buffer (must be at least src_len bytes)
 * @param src COBS-encoded buffer
 * @param src_len Encoded length
 * @return Decoded length, or 0 on error
 */
size_t cobs_decode(uint8_t* dst, const uint8_t* src, size_t src_len);

#ifdef __cplusplus
}
#endif

#endif
