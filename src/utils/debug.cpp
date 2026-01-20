/**
 * Debug output via COBS
 */

#include "debug.h"
#include "cobs.h"
#include <Arduino.h>
#include <stdarg.h>

/* Level names for JSON output */
static const char* level_names[] = {"ERROR", "WARN", "INFO", "DEBUG"};

/**
 * Escape string for JSON output
 * Replaces control chars with . and escapes quotes/backslashes
 */
static void escape_json_string(char* dst, size_t dst_size, const char* src) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        char c = src[i];
        if (c < 32 || c > 126) {
            dst[j++] = '.'; // Replace control chars
        } else if (c == '"' || c == '\\') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
                dst[j++] = c;
            }
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

void debug_output(debug_level_t level, const char* msg) {
#if !DEBUG_ENABLED
    return;
#endif

    /* Build JSON: {"type":"debug","level":"INFO","msg":"..."} */
    char escaped[256];
    escape_json_string(escaped, sizeof(escaped), msg);

    char json[384];
    snprintf(json, sizeof(json), "{\"type\":\"debug\",\"level\":\"%s\",\"msg\":\"%s\"}", level_names[level], escaped);

    /* COBS encode and send with zero terminator */
    uint8_t encoded[512];
    size_t encoded_len = cobs_encode(encoded, (const uint8_t*)json, strlen(json));

    Serial.write(encoded, encoded_len);
    Serial.write((uint8_t)0); /* COBS packet boundary */
    Serial.flush();
}

void debug_printf(debug_level_t level, const char* fmt, ...) {
#if !DEBUG_ENABLED
    return;
#endif

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    debug_output(level, buf);
}
