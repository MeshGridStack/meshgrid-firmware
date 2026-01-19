/**
 * Messaging utilities - TX queue, airtime, rate limiting, deduplication, logging
 */

#include "utils.h"
#include "../messaging.h"
#include "utils/memory.h"
#include "utils/debug.h"
#include "utils/types.h"
#include "radio/radio_hal.h"
#include <Arduino.h>

extern "C" {
#include "network/protocol.h"
#include "utils/cobs.h"
}

/* Externs from main.cpp */
extern struct meshgrid_state mesh;
extern uint32_t boot_time;
extern struct rtc_time_t rtc_time;
extern bool radio_in_rx_mode;

extern struct seen_entry seen_table[];
extern uint8_t seen_idx;

extern uint32_t stat_duplicates;

/* Rate limiting: Track packet timestamps per source hash */
#define RATE_LIMIT_WINDOW_MS  1000  // 1 second window
#define RATE_LIMIT_MAX_PKTS   10    // Max 10 packets per second per source

struct rate_limit_entry {
    uint8_t source_hash;
    uint32_t timestamps[RATE_LIMIT_MAX_PKTS];
    uint8_t count;
};

#define RATE_LIMIT_TABLE_SIZE 32
static struct rate_limit_entry rate_limit_table[RATE_LIMIT_TABLE_SIZE];

/**
 * Check if source hash exceeds rate limit
 * Returns true if packet should be rate limited (dropped)
 */
bool rate_limit_check(uint8_t source_hash) {
    uint32_t now = millis();
    struct rate_limit_entry *entry = NULL;

    /* Find existing entry for this source */
    for (int i = 0; i < RATE_LIMIT_TABLE_SIZE; i++) {
        if (rate_limit_table[i].source_hash == source_hash && rate_limit_table[i].count > 0) {
            entry = &rate_limit_table[i];
            break;
        }
    }

    /* No entry found - find free slot */
    if (entry == NULL) {
        for (int i = 0; i < RATE_LIMIT_TABLE_SIZE; i++) {
            if (rate_limit_table[i].count == 0) {
                entry = &rate_limit_table[i];
                entry->source_hash = source_hash;
                break;
            }
        }
    }

    /* Table full - allow packet (fail open for availability) */
    if (entry == NULL) {
        return false;
    }

    /* Remove expired timestamps (older than 1 second) */
    uint8_t new_count = 0;
    for (int i = 0; i < entry->count; i++) {
        if (now - entry->timestamps[i] < RATE_LIMIT_WINDOW_MS) {
            entry->timestamps[new_count++] = entry->timestamps[i];
        }
    }
    entry->count = new_count;

    /* Check if rate limit exceeded */
    if (entry->count >= RATE_LIMIT_MAX_PKTS) {
        /* Rate limit exceeded - drop packet */
        return true;
    }

    /* Add current timestamp */
    entry->timestamps[entry->count++] = now;
    return false;
}

/* ========================================================================= */
/* Packet Queue and Airtime Management                                      */
/* ========================================================================= */

static struct queued_packet tx_queue[TX_QUEUE_SIZE];
static struct airtime_tracker airtime = {0};

void tx_queue_init(void) {
    for (int i = 0; i < TX_QUEUE_SIZE; i++) {
        tx_queue[i].valid = false;
    }
    airtime.window_start = millis();
    airtime.total_tx_ms = 0;
    airtime.last_tx_ms = 0;
}

/*
 * Add packet to transmission queue with priority
 * Priority based on path_len: longer paths get HIGHER priority (lower number)
 * Returns true if added, false if queue is full
 */
bool tx_queue_add(const uint8_t *buf, int len, uint32_t delay_ms, uint8_t priority) {
    /* Find empty slot */
    for (int i = 0; i < TX_QUEUE_SIZE; i++) {
        if (!tx_queue[i].valid) {
            memcpy(tx_queue[i].buf, buf, len);
            tx_queue[i].len = len;
            tx_queue[i].scheduled_time = millis() + delay_ms;
            tx_queue[i].priority = priority;
            tx_queue[i].valid = true;
            return true;
        }
    }

    /* Queue full - drop packet */
    DEBUG_WARN("TX QUEUE FULL - dropped packet");
    return false;
}

/*
 * Calculate airtime for a packet
 * Based on RadioLib transmission time calculation
 * Assumes SF7, BW125, CR5 (typical LoRa settings)
 */
static uint32_t calculate_airtime_ms(int packet_len) {
    /* Rough estimation: ~5-6ms per byte for SF7/BW125 */
    /* Add preamble time (~15ms) */
    return 15 + (packet_len * 6);
}

/*
 * Get required silence time based on last transmission
 * MeshCore uses 2.0x transmission time
 */
uint32_t airtime_get_silence_required(void) {
    if (airtime.last_tx_ms == 0) {
        return 0;  /* No recent transmission */
    }
    return airtime.last_tx_ms * 2;
}

/*
 * Check if we have airtime budget available
 * Returns true if we can transmit
 */
static bool airtime_check_budget(uint32_t tx_duration_ms) {
    uint32_t now = millis();

    /* Reset window if expired */
    if (now - airtime.window_start >= AIRTIME_WINDOW_MS) {
        airtime.window_start = now;
        airtime.total_tx_ms = 0;
    }

    /* Check if adding this transmission exceeds budget */
    uint32_t max_airtime = (AIRTIME_WINDOW_MS * AIRTIME_BUDGET_PCT) / 100;
    if (airtime.total_tx_ms + tx_duration_ms > max_airtime) {
        return false;  /* Would exceed budget */
    }

    return true;
}

/*
 * Update airtime tracking after transmission
 */
static void airtime_record_tx(uint32_t tx_duration_ms) {
    airtime.total_tx_ms += tx_duration_ms;
    airtime.last_tx_ms = tx_duration_ms;
}

/*
 * Process transmission queue
 * Called from main loop() - finds highest priority ready packet and transmits
 */
void tx_queue_process(void) {
    uint32_t now = millis();

    /* Check silence period after last transmission */
    uint32_t silence_required = airtime_get_silence_required();
    if (silence_required > 0) {
        static uint32_t last_tx_time = 0;
        if (now - last_tx_time < silence_required) {
            return;  /* Still in silence period */
        }
    }
    static uint32_t last_tx_time = 0;  /* Moved outside to persist */

    /* Find highest priority packet that's ready to send */
    int best_idx = -1;
    uint8_t best_priority = 255;

    for (int i = 0; i < TX_QUEUE_SIZE; i++) {
        if (!tx_queue[i].valid) continue;

        /* Check if scheduled time has arrived */
        if (now < tx_queue[i].scheduled_time) continue;

        /* Check if higher priority */
        if (tx_queue[i].priority < best_priority) {
            best_priority = tx_queue[i].priority;
            best_idx = i;
        }
    }

    /* No packets ready */
    if (best_idx < 0) return;

    /* Calculate airtime for this packet */
    uint32_t tx_duration = calculate_airtime_ms(tx_queue[best_idx].len);

    /* Check airtime budget */
    if (!airtime_check_budget(tx_duration)) {
        /* Budget exceeded - log and wait */
        static uint32_t last_budget_warning = 0;
        if (now - last_budget_warning > 5000) {
            DEBUG_WARN("AIRTIME budget exceeded - delaying TX");
            last_budget_warning = now;
        }
        return;
    }

    /* Transmit (collision avoidance via queue + random delays) */
    int16_t result = get_radio()->transmit(tx_queue[best_idx].buf, tx_queue[best_idx].len);
    radio_in_rx_mode = false;  /* Mark as not in RX after TX */
    get_radio()->startReceive();

    /* Update statistics */
    if (result == 0) {  /* RADIOLIB_ERR_NONE = 0 */
        mesh.packets_tx++;  /* Increment TX counter on success */
    }
    airtime_record_tx(tx_duration);
    last_tx_time = now;

    /* Mark packet as transmitted */
    tx_queue[best_idx].valid = false;
}

uint8_t random_byte(void) {
    return (uint8_t)random(256);
}

uint32_t get_uptime_secs(void) {
    return (millis() - boot_time) / 1000;
}

bool seen_check_and_add(const struct meshgrid_packet *pkt) {
    uint8_t hash;
    meshgrid_packet_hash(pkt, &hash);

    uint32_t now = millis();

    /* Check if we've seen this packet recently */
    for (int i = 0; i < SEEN_TABLE_SIZE; i++) {
        if (seen_table[i].hash == hash &&
            (now - seen_table[i].time) < MESHGRID_DUPLICATE_WINDOW_MS) {
            stat_duplicates++;
            return true;  /* Already seen */
        }
    }

    /* Add to seen table */
    seen_table[seen_idx].hash = hash;
    seen_table[seen_idx].time = now;
    seen_idx = (seen_idx + 1) % SEEN_TABLE_SIZE;

    return false;  /* Not seen before */
}
