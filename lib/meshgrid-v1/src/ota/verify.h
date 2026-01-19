#ifndef MESHGRID_V1_OTA_VERIFY_H
#define MESHGRID_V1_OTA_VERIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool meshgrid_ota_verify_firmware_hash(const uint8_t *firmware, size_t size,
                                        const uint8_t *expected_hash);

#ifdef __cplusplus
}
#endif

#endif
