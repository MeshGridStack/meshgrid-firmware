/**
 * meshgrid-firmware Shared Type Definitions
 *
 * Common structures used across multiple modules.
 * Defined once here to avoid duplication and inconsistencies.
 */

#ifndef MESHGRID_TYPES_H
#define MESHGRID_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Channel entry - custom chat channel
 */
struct channel_entry {
    bool valid;
    uint8_t hash;
    char name[17];
    uint8_t secret[32];
};

/**
 * Message entry - inbox message storage
 */
struct message_entry {
    bool valid;
    bool decrypted;
    uint8_t sender_hash;
    char sender_name[17];
    uint8_t channel_hash;  /* 0 for direct message */
    uint32_t timestamp;
    char text[128];
};

/**
 * Seen entry - packet deduplication table
 */
struct seen_entry {
    uint8_t hash;
    uint32_t time;
};

/**
 * RTC time tracking
 */
struct rtc_time_t {
    bool valid;
    uint32_t epoch_at_boot;
};

#endif /* MESHGRID_TYPES_H */
