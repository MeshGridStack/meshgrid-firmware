/**
 * Mesh messaging - packet send/receive
 */

#include "messaging.h"
#include "neighbors.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <mbedtls/sha256.h>

extern "C" {
#include "drivers/crypto/crypto.h"
}

/* Externs from main.cpp */
extern struct meshgrid_state mesh;
extern class SX1262 *radio;
extern uint32_t boot_time;

/* RTC time tracking */
struct rtc_time_t {
    bool valid;
    uint32_t epoch_at_boot;
};
extern struct rtc_time_t rtc_time;
extern bool display_dirty, monitor_mode, log_enabled;
extern uint32_t last_activity_time;
extern enum meshgrid_device_mode device_mode;

extern String log_buffer[];
extern int log_index, log_count;
#define LOG_BUFFER_SIZE 50

/* Seen table */
struct seen_entry {
    uint8_t hash;
    uint32_t time;
};
extern struct seen_entry seen_table[];
extern uint8_t seen_idx;
#define SEEN_TABLE_SIZE 32

extern uint32_t stat_duplicates, stat_flood_fwd, stat_flood_rx;
extern uint8_t public_channel_secret[32];
extern uint8_t public_channel_hash;

/* Functions from main.cpp */
extern void led_blink(void);

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
    log_event("TX QUEUE FULL - dropped packet");
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

/* NOTE: Proper CAD (Channel Activity Detection) not available with current RadioLib API
 * getRSSI() returns last packet RSSI, not real-time channel state
 * Relying on priority queue + randomized delays for collision avoidance instead */

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
            log_event("AIRTIME budget exceeded - delaying TX");
            last_budget_warning = now;
        }
        return;
    }

    /* Transmit (collision avoidance via queue + random delays) */
    radio->transmit(tx_queue[best_idx].buf, tx_queue[best_idx].len);
    radio->startReceive();

    /* Update statistics */
    airtime_record_tx(tx_duration);
    last_tx_time = now;

    /* Mark packet as transmitted */
    tx_queue[best_idx].valid = false;
}

void log_event(const String &msg) {
    if (!log_enabled && !monitor_mode) return;

    char time_buf[32];

    if (rtc_time.valid) {
        /* Wall clock time: HH:MM:SS DD-MM-YYYY */
        uint32_t current_epoch = rtc_time.epoch_at_boot + (millis() / 1000);

        /* Convert Unix epoch to calendar time */
        uint32_t days_since_epoch = current_epoch / 86400;
        uint32_t seconds_today = current_epoch % 86400;

        uint32_t hours = seconds_today / 3600;
        uint32_t minutes = (seconds_today % 3600) / 60;
        uint32_t seconds = seconds_today % 60;

        /* Simple epoch to date conversion (Y2K epoch: 2000-01-01) */
        uint32_t year = 2000;
        uint32_t days_in_year = 365;

        while (days_since_epoch >= days_in_year) {
            days_since_epoch -= days_in_year;
            year++;
            days_in_year = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
        }

        /* Simple month/day calculation */
        const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        uint8_t month = 1;
        uint32_t day = days_since_epoch + 1;

        for (int m = 0; m < 12; m++) {
            uint32_t dim = days_in_month[m];
            if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) dim = 29;
            if (day <= dim) {
                month = m + 1;
                break;
            }
            day -= dim;
        }

        snprintf(time_buf, sizeof(time_buf), "[%02lu:%02lu:%02lu %02lu-%02u-%04lu] ",
                 hours, minutes, seconds, day, month, year);
    } else {
        /* Fallback: time since boot */
        uint32_t ms = millis();
        uint32_t total_seconds = ms / 1000;
        uint32_t hours = total_seconds / 3600;
        uint32_t minutes = (total_seconds % 3600) / 60;
        uint32_t seconds = total_seconds % 60;

        snprintf(time_buf, sizeof(time_buf), "[%02lu:%02lu:%02lu] ",
                 hours, minutes, seconds);
    }

    String timestamped_msg = String(time_buf) + msg;

    if (monitor_mode) {
        Serial.println(timestamped_msg);
    }

    if (log_enabled) {
        log_buffer[log_index] = timestamped_msg;
        log_index = (log_index + 1) % LOG_BUFFER_SIZE;
        if (log_count < LOG_BUFFER_SIZE) log_count++;
    }
}

uint8_t random_byte(void) {
    return (uint8_t)random(256);
}

uint32_t get_uptime_secs(void) {
    return (millis() - boot_time) / 1000;
}

static bool seen_check_and_add(const struct meshgrid_packet *pkt) {
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

void handle_advert(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr) {
    uint8_t pubkey[MESHGRID_PUBKEY_SIZE];
    char name[MESHGRID_NODE_NAME_MAX + 1];
    uint32_t timestamp;

    if (meshgrid_parse_advert(pkt, pubkey, name, sizeof(name), &timestamp) == 0) {
        uint8_t hash = meshgrid_hash_pubkey(pubkey);
        neighbor_update(pubkey, name, timestamp, rssi, snr, pkt->path_len);

        /* Log received advertisement with packet details */
        // Calculate total packet length for verification
        // Format: header(1) + [transport(4)] + path_len(1) + path(N) + payload(M)
        int expected_len = 1 + (MESHGRID_HAS_TRANSPORT(pkt->route_type) ? 4 : 0) + 1 + pkt->path_len + pkt->payload_len;

        String log_msg = "RX ADV 0x";
        log_msg += String(hash, HEX);
        log_msg += " ";
        log_msg += String(rssi);
        log_msg += "dBm ";
        log_msg += name;
        log_msg += " len=";
        log_msg += String(expected_len);
        log_msg += " payload=";
        log_msg += String(pkt->payload_len);
        log_msg += " snr=";
        log_msg += String(snr);

        // Add path details if present
        if (pkt->path_len > 0) {
            log_msg += " path:[";
            for (int i = 0; i < pkt->path_len; i++) {
                if (i > 0) log_msg += ",";
                log_msg += "0x";
                log_msg += String(pkt->path[i], HEX);
            }
            log_msg += "]";
        }

        log_event(log_msg);

        display_dirty = true;
    }
}

void handle_text_msg(struct meshgrid_packet *pkt, int16_t rssi) {
    /* For now, just log encrypted messages */
    /* TODO: Decrypt if we have the key */
    String log_msg = "RX MSG ";
    log_msg += String(pkt->payload_len);
    log_msg += "B ";
    log_msg += String(rssi);
    log_msg += "dBm hops:";
    log_msg += String(pkt->path_len);
    log_event(log_msg);
}

/*
 * Initialize public channel from Base64-encoded PSK
 */
void handle_group_msg(struct meshgrid_packet *pkt, int16_t rssi) {
    /* Group message format: [channel_hash][MAC 2 bytes][encrypted payload] */
    if (pkt->payload_len < 3) {
        log_event("RX GRP (too short)");
        return;
    }

    uint8_t channel_hash = pkt->payload[0];

    /* Check if this is for the public channel */
    if (channel_hash == public_channel_hash) {
        /* Attempt to decrypt */
        uint8_t decrypted[256];
        int dec_len = crypto_mac_then_decrypt(decrypted,
                                               &pkt->payload[1],  /* Skip channel hash */
                                               pkt->payload_len - 1,
                                               public_channel_secret);

        if (dec_len > 0) {
            /* Successfully decrypted */
            decrypted[dec_len] = '\0';  /* Null-terminate for safety */

            String log_msg = "RX GRP [Public] ";
            log_msg += String(rssi);
            log_msg += "dBm: ";
            log_msg += String((char*)decrypted);
            log_event(log_msg);
        } else {
            /* MAC verification failed */
            String log_msg = "RX GRP [Public] MAC FAIL ";
            log_msg += String(rssi);
            log_msg += "dBm";
            log_event(log_msg);
        }
    } else {
        /* Unknown channel */
        String log_msg = "RX GRP [0x";
        log_msg += String(channel_hash, HEX);
        log_msg += "] ";
        log_msg += String(pkt->payload_len);
        log_msg += "B ";
        log_msg += String(rssi);
        log_msg += "dBm";
        log_event(log_msg);
    }
}

/*
 * Process received packet
 */
void process_packet(uint8_t *buf, int len, int16_t rssi, int8_t snr) {
    struct meshgrid_packet pkt;

    if (meshgrid_packet_parse(buf, len, &pkt) != 0) {
        Serial.println("[ERR] Bad packet");
        mesh.packets_dropped++;
        return;
    }

    pkt.rssi = rssi;
    pkt.snr = snr;
    pkt.rx_time = millis();
    mesh.packets_rx++;
    stat_flood_rx++;

    /* Check for duplicates */
    if (seen_check_and_add(&pkt)) {
        return;  /* Already processed */
    }

    /* Handle by payload type */
    switch (pkt.payload_type) {
        case PAYLOAD_ADVERT:
            handle_advert(&pkt, rssi, snr);
            break;
        case PAYLOAD_TXT_MSG:
            handle_text_msg(&pkt, rssi);
            break;
        case PAYLOAD_GRP_TXT:
        case PAYLOAD_GRP_DATA:
            handle_group_msg(&pkt, rssi);
            break;
        case PAYLOAD_ACK:
            /* ACKs don't need forwarding in flood mode */
            {
                String log_msg = "RX ACK ";
                log_msg += String(rssi);
                log_msg += "dBm hops:";
                log_msg += String(pkt.path_len);
                log_event(log_msg);
            }
            return;
        default:
            break;
    }

    /* Forward if appropriate (only REPEATER and ROOM forward, CLIENT does not) */
    if (meshgrid_should_forward(&pkt, mesh.our_hash, device_mode)) {
        /* Add ourselves to path */
        meshgrid_path_append(&pkt, mesh.our_hash);

        /* Calculate delay based on path length */
        uint32_t delay_ms = meshgrid_retransmit_delay(&pkt, random_byte());

        /* Priority: longer paths get HIGHER priority (lower number) */
        uint8_t priority = (pkt.path_len > 0) ? (10 - pkt.path_len) : 10;
        if (priority < 1) priority = 1;  /* Clamp to minimum */

        /* Re-encode packet */
        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
        int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

        if (tx_len > 0) {
            /* Add to transmission queue (non-blocking) */
            if (tx_queue_add(tx_buf, tx_len, delay_ms, priority)) {
                /* Successfully queued */
                mesh.packets_fwd++;
                stat_flood_fwd++;

                /* Log queued packet with details */
                String log_msg = "QUEUE ";
                switch (pkt.payload_type) {
                    case PAYLOAD_ADVERT: log_msg += "ADV"; break;
                    case PAYLOAD_TXT_MSG: log_msg += "MSG"; break;
                    case PAYLOAD_GRP_TXT: log_msg += "GRP"; break;
                    case PAYLOAD_GRP_DATA: log_msg += "DAT"; break;
                    default: log_msg += "PKT"; break;
                }
                log_msg += " len=";
                log_msg += String(tx_len);
                log_msg += " payload=";
                log_msg += String(pkt.payload_len);
                log_msg += " hops:";
                log_msg += String(pkt.path_len);
                log_msg += " delay:";
                log_msg += String(delay_ms);
                log_msg += "ms prio:";
                log_msg += String(priority);

                // Show path traveled
                if (pkt.path_len > 0) {
                    log_msg += " path:[";
                    for (int i = 0; i < pkt.path_len; i++) {
                        if (i > 0) log_msg += ",";
                        log_msg += "0x";
                        log_msg += String(pkt.path[i], HEX);
                    }
                    log_msg += "]";
                }

                log_event(log_msg);
            }
            /* If queue full, tx_queue_add() logs error */
        }
    }
}

/* ========================================================================= */
/* Advertising                                                               */
/* ========================================================================= */

void send_advertisement(uint8_t route_type) {
    struct meshgrid_packet pkt;
    uint32_t timestamp = get_uptime_secs();  /* TODO: Use RTC if available */

    meshgrid_create_advert(&pkt, mesh.pubkey, mesh.name, timestamp);
    pkt.route_type = route_type;  /* Override with specified route type */

    /* Compute signature over pubkey + timestamp + app_data (MeshCore format)
     * Signature goes at offset 36 (after pubkey and timestamp) */
    uint8_t sig_data[256];
    size_t sig_len = 0;

    /* Copy pubkey (32 bytes) */
    memcpy(&sig_data[sig_len], pkt.payload, MESHGRID_PUBKEY_SIZE);
    sig_len += MESHGRID_PUBKEY_SIZE;

    /* Copy timestamp (4 bytes) */
    memcpy(&sig_data[sig_len], &pkt.payload[MESHGRID_PUBKEY_SIZE], 4);
    sig_len += 4;

    /* Copy app_data (starts after pubkey + timestamp + signature) */
    size_t app_data_offset = MESHGRID_PUBKEY_SIZE + 4 + MESHGRID_SIGNATURE_SIZE;
    size_t app_data_len = pkt.payload_len - app_data_offset;
    memcpy(&sig_data[sig_len], &pkt.payload[app_data_offset], app_data_len);
    sig_len += app_data_len;

    /* Sign and insert signature */
    uint8_t signature[MESHGRID_SIGNATURE_SIZE];
    crypto_sign(signature, sig_data, sig_len, mesh.pubkey, mesh.privkey);
    memcpy(&pkt.payload[MESHGRID_PUBKEY_SIZE + 4], signature, MESHGRID_SIGNATURE_SIZE);

    /* Verify signature immediately to ensure it's valid */
    bool sig_valid = crypto_verify(signature, sig_data, sig_len, mesh.pubkey);
    if (!sig_valid) {
        Serial.println("WARNING: Self-signature verification failed!");
    }

    /* Set path based on route type */
    if (route_type == ROUTE_FLOOD) {
        /* Add our hash to start of path for flood routing */
        pkt.path[0] = mesh.our_hash;
        pkt.path_len = 1;
    } else {
        /* Zero-hop (ROUTE_DIRECT) has empty path */
        pkt.path_len = 0;
    }

    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        Serial.print("TX ADV: total_len=");
        Serial.print(tx_len);
        Serial.print(" payload_len=");
        Serial.print(pkt.payload_len);
        Serial.print(" sig_valid=");
        Serial.println(sig_valid ? "YES" : "NO");
        radio->transmit(tx_buf, tx_len);
        radio->startReceive();
        mesh.packets_tx++;
        led_blink();

        /* Log with route type and packet details */
        String log_msg = "TX ADV ";
        if (route_type == ROUTE_FLOOD) {
            log_msg += "FLOOD 0x";
        } else {
            log_msg += "LOCAL 0x";
        }
        log_msg += String(mesh.our_hash, HEX);
        log_msg += " ";
        log_msg += mesh.name;
        log_msg += " len=";
        log_msg += String(tx_len);
        log_msg += " payload=";
        log_msg += String(pkt.payload_len);
        log_msg += " sig=";
        log_msg += sig_valid ? "OK" : "BAD";
        log_event(log_msg);
    }
}

void send_text_message(const char *text) {
    struct meshgrid_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Set packet header */
    pkt.route_type = ROUTE_FLOOD;
    pkt.payload_type = PAYLOAD_TXT_MSG;
    pkt.version = 0;

    /* Add timestamp */
    uint32_t timestamp = get_uptime_secs();
    pkt.payload_len = snprintf((char*)pkt.payload, MESHGRID_MAX_PAYLOAD_SIZE,
                               "%lu:%s", timestamp, text);

    /* Add our hash to start of path */
    pkt.path[0] = mesh.our_hash;
    pkt.path_len = 1;

    /* Encode and transmit */
    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        radio->transmit(tx_buf, tx_len);
        radio->startReceive();
        mesh.packets_tx++;
        led_blink();
        String log_msg = "TX MSG ";
        log_msg += text;
        log_event(log_msg);
    }
}

/*
 * Send encrypted group message to public channel
 */
void send_group_message(const char *text) {
    struct meshgrid_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Set packet header */
    pkt.route_type = ROUTE_FLOOD;
    pkt.payload_type = PAYLOAD_GRP_TXT;
    pkt.version = 0;

    /* Payload format: [channel_hash][MAC 2 bytes][encrypted text] */
    pkt.payload[0] = public_channel_hash;

    /* Encrypt the message */
    int enc_len = crypto_encrypt_then_mac(&pkt.payload[1],
                                          (const uint8_t*)text,
                                          strlen(text),
                                          public_channel_secret);

    pkt.payload_len = 1 + enc_len;  /* channel hash + encrypted data */

    /* Add our hash to start of path */
    pkt.path[0] = mesh.our_hash;
    pkt.path_len = 1;

    /* Encode and transmit */
    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        radio->transmit(tx_buf, tx_len);
        radio->startReceive();
        mesh.packets_tx++;
        led_blink();
        String log_msg = "TX GRP [Public]: ";
        log_msg += text;
        log_event(log_msg);
    }
}
