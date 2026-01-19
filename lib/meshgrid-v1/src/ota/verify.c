#include "verify.h"
#include <string.h>
#include <mbedtls/sha256.h>

bool meshgrid_ota_verify_firmware_hash(const uint8_t *firmware, size_t size,
                                        const uint8_t *expected_hash) {
    uint8_t computed_hash[32];
    mbedtls_sha256(firmware, size, computed_hash, 0);
    return memcmp(computed_hash, expected_hash, 32) == 0;
}
