#ifndef MESSAGING_UTILS_H
#define MESSAGING_UTILS_H

#include <Arduino.h>
#include "utils/types.h"

/* TX Queue */
void tx_queue_init(void);
bool tx_queue_add(const uint8_t *buf, int len, uint32_t delay_ms, uint8_t priority);
void tx_queue_process(void);

/* Airtime Management */
uint32_t airtime_get_silence_required(void);

/* Utilities */
uint8_t random_byte(void);
uint32_t get_uptime_secs(void);

/* Deduplication */
bool seen_check_and_add(const struct meshgrid_packet *pkt);

/* Rate Limiting */
bool rate_limit_check(uint8_t source_hash);

#endif /* MESSAGING_UTILS_H */
