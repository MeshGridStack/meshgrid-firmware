/**
 * Mesh messaging - packet send/receive
 */

#ifndef MESHGRID_MESSAGING_H
#define MESHGRID_MESSAGING_H

#include <Arduino.h>
#include <stdint.h>

extern "C" {
#include "net/protocol.h"
}

/* Packet queue configuration */
#define TX_QUEUE_SIZE 16
#define AIRTIME_WINDOW_MS 10000  /* 10 second window */
#define AIRTIME_BUDGET_PCT 33     /* 33% duty cycle */

/* Queued packet for transmission */
struct queued_packet {
    uint8_t buf[MESHGRID_MAX_PACKET_SIZE];
    int len;
    uint32_t scheduled_time;  /* millis() when packet should be sent */
    uint8_t priority;          /* Lower number = higher priority */
    bool valid;
};

/* Airtime tracking */
struct airtime_tracker {
    uint32_t window_start;     /* Start of current window (millis) */
    uint32_t total_tx_ms;      /* Total TX time in current window */
    uint32_t last_tx_ms;       /* Duration of last transmission */
};

/* Queue management functions */
void tx_queue_init(void);
bool tx_queue_add(const uint8_t *buf, int len, uint32_t delay_ms, uint8_t priority);
void tx_queue_process(void);
uint32_t airtime_get_silence_required(void);

/* Log an event to the log buffer */
void log_event(const String &msg);

/* Send advertisement packet */
void send_advertisement(uint8_t route);

/* Send encrypted text message to specific peer */
void send_text_message(uint8_t dest_hash, const char *text);

/* Send encrypted group message */
void send_group_message(const char *text);

/* Send encrypted message to specific channel */
void send_channel_message(uint8_t channel_hash, const uint8_t *channel_secret, const char *text, const char *channel_name);

/* Process received packet */
void process_packet(uint8_t *buf, int len, int16_t rssi, int8_t snr);

/* Get random byte */
uint8_t random_byte(void);

/* Get uptime in seconds */
uint32_t get_uptime_secs(void);

#endif /* MESHGRID_MESSAGING_H */
