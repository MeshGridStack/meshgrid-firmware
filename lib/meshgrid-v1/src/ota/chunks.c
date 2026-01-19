#include "chunks.h"
#include <string.h>

int meshgrid_ota_chunk_encode(const struct meshgrid_ota_chunk *chunk,
                               uint8_t *payload, size_t max_len) {
    if (max_len < 8 + chunk->data_length) return -1;
    
    int pos = 0;
    payload[pos++] = (chunk->session_id >> 24) & 0xFF;
    payload[pos++] = (chunk->session_id >> 16) & 0xFF;
    payload[pos++] = (chunk->session_id >> 8) & 0xFF;
    payload[pos++] = chunk->session_id & 0xFF;
    payload[pos++] = (chunk->chunk_index >> 8) & 0xFF;
    payload[pos++] = chunk->chunk_index & 0xFF;
    payload[pos++] = (chunk->data_length >> 8) & 0xFF;
    payload[pos++] = chunk->data_length & 0xFF;
    memcpy(&payload[pos], chunk->data, chunk->data_length);
    pos += chunk->data_length;
    
    return pos;
}

int meshgrid_ota_chunk_decode(const uint8_t *payload, size_t len,
                               struct meshgrid_ota_chunk *chunk) {
    if (len < 8) return -1;
    
    int pos = 0;
    chunk->session_id = ((uint32_t)payload[pos] << 24) |
                        ((uint32_t)payload[pos+1] << 16) |
                        ((uint32_t)payload[pos+2] << 8) |
                        payload[pos+3];
    pos += 4;
    chunk->chunk_index = ((uint16_t)payload[pos] << 8) | payload[pos+1];
    pos += 2;
    chunk->data_length = ((uint16_t)payload[pos] << 8) | payload[pos+1];
    pos += 2;
    
    if (len < (size_t)(8 + chunk->data_length)) return -1;
    if (chunk->data_length > MESHGRID_OTA_MAX_CHUNK_DATA) return -1;
    
    memcpy(chunk->data, &payload[pos], chunk->data_length);
    return 0;
}
