/**
 * Common response builder for commands
 */

#include "common.h"
#include "utils/cobs.h"
#include <Arduino.h>

/* Response builder state */
static String response_buffer;
static bool building_response = false;

/* COBS encode buffer - large enough for NEIGHBORS response with many nodes */
static uint8_t cobs_encode_buf[8192];

/* Forward declaration */
static void send_cobs_response(const String &response);

/* Response building helpers */
void response_start() {
    response_buffer = "";
    building_response = true;
}

void response_print(const String &str) {
    if (!building_response) response_start();
    response_buffer += str;
}

void response_print(const char *str) {
    if (!building_response) response_start();
    response_buffer += str;
}

void response_print(char c) {
    if (!building_response) response_start();
    response_buffer += c;
}

void response_print(int val) {
    if (!building_response) response_start();
    response_buffer += String(val);
}

void response_print(unsigned int val) {
    if (!building_response) response_start();
    response_buffer += String(val);
}

void response_print(long val) {
    if (!building_response) response_start();
    response_buffer += String(val);
}

void response_print(unsigned long val) {
    if (!building_response) response_start();
    response_buffer += String(val);
}

void response_print(float val, int decimals) {
    if (!building_response) response_start();
    response_buffer += String(val, decimals);
}

void response_println(const String &str) {
    response_print(str);
    response_send();
}

void response_println(const char *str) {
    response_print(str);
    response_send();
}

void response_println(int val) {
    response_print(val);
    response_send();
}

void response_send() {
    if (building_response) {
        send_cobs_response(response_buffer);
        building_response = false;
        response_buffer = "";
    }
}

/* JSON string escaping helper */
void print_json_string(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c < 32 || c > 126) {
            response_print(".");
        } else if (c == '"' || c == '\\') {
            response_buffer += '\\';
            response_buffer += c;
        } else {
            response_buffer += c;
        }
    }
}

/* Helper to send COBS-encoded response */
static void send_cobs_response(const String &response) {
    /* Safety check: ensure response fits in buffer (with COBS overhead ~1.004x) */
    if (response.length() > sizeof(cobs_encode_buf) - 10) {
        /* Response too large - send error instead */
        const char *err = "ERR Response too large";
        size_t err_len = cobs_encode(cobs_encode_buf, (const uint8_t*)err, strlen(err));
        Serial.write(cobs_encode_buf, err_len);
        Serial.write((uint8_t)0);
        return;
    }

    size_t encoded_len = cobs_encode(cobs_encode_buf, (const uint8_t*)response.c_str(), response.length());
    Serial.write(cobs_encode_buf, encoded_len);
    Serial.write((uint8_t)0);
    Serial.flush();  // Ensure response is sent before command returns
}
