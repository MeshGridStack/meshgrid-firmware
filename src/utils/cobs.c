/**
 * COBS Implementation
 * Based on RFC standard - simple, fast, minimal
 */

#include "cobs.h"

size_t cobs_encode(uint8_t *dst, const uint8_t *src, size_t src_len) {
    const uint8_t *src_end = src + src_len;
    uint8_t *dst_start = dst;
    uint8_t *code_ptr = dst++;
    uint8_t code = 1;

    while (src < src_end) {
        if (*src == 0) {
            /* Found zero - write code byte */
            *code_ptr = code;
            code_ptr = dst++;
            code = 1;
        } else {
            *dst++ = *src;
            code++;
            if (code == 0xFF) {
                /* Code byte full - write it */
                *code_ptr = code;
                code_ptr = dst++;
                code = 1;
            }
        }
        src++;
    }

    /* Write final code byte */
    *code_ptr = code;
    return dst - dst_start;
}

size_t cobs_decode(uint8_t *dst, const uint8_t *src, size_t src_len) {
    if (src_len == 0) return 0;

    const uint8_t *src_end = src + src_len;
    uint8_t *dst_start = dst;

    while (src < src_end) {
        uint8_t code = *src++;
        if (code == 0) return 0;  /* Invalid */

        /* Copy data bytes */
        for (uint8_t i = 1; i < code && src < src_end; i++) {
            *dst++ = *src++;
        }

        /* Insert zero if not at end */
        if (code < 0xFF && src < src_end) {
            *dst++ = 0;
        }
    }

    return dst - dst_start;
}
