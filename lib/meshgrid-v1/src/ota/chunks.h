#ifndef MESHGRID_V1_OTA_CHUNKS_H
#define MESHGRID_V1_OTA_CHUNKS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESHGRID_OTA_MAX_CHUNK_DATA 174

struct meshgrid_ota_chunk {
    uint32_t session_id;
    uint16_t chunk_index;
    uint16_t data_length;
    uint8_t data[MESHGRID_OTA_MAX_CHUNK_DATA];
};

int meshgrid_ota_chunk_encode(const struct meshgrid_ota_chunk *chunk, 
                               uint8_t *payload, size_t max_len);
int meshgrid_ota_chunk_decode(const uint8_t *payload, size_t len,
                               struct meshgrid_ota_chunk *chunk);

#ifdef __cplusplus
}
#endif

#endif
