/**
 * Mesh messaging - packet send/receive
 */

#include "messaging.h"
#include "neighbors.h"
#include "config/memory.h"
#include "lib/types.h"
#include "drivers/radio/radio_hal.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <mbedtls/sha256.h>

extern "C" {
#include "drivers/crypto/crypto.h"
}

/* Externs from main.cpp - structs defined in lib/types.h */
extern struct meshgrid_state mesh;
extern uint32_t boot_time;

/* Radio accessor - use get_radio() for PhysicalLayer* */
extern struct rtc_time_t rtc_time;
extern bool display_dirty, monitor_mode, log_enabled;
extern uint32_t last_activity_time;
extern enum meshgrid_device_mode device_mode;

extern struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
extern int custom_channel_count;

extern struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
extern int public_msg_index, public_msg_count;

extern struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
extern int direct_msg_index, direct_msg_count;

extern struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
extern int channel_msg_index[MAX_CUSTOM_CHANNELS];
extern int channel_msg_count[MAX_CUSTOM_CHANNELS];

extern String log_buffer[];
extern int log_index, log_count;

extern struct seen_entry seen_table[];
extern uint8_t seen_idx;

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
    get_radio()->transmit(tx_queue[best_idx].buf, tx_queue[best_idx].len);
    get_radio()->startReceive();

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

        /* Don't add ourselves to neighbor table */
        if (hash != mesh.our_hash) {
            neighbor_update(pubkey, name, timestamp, rssi, snr, pkt->path_len);
        }

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
    /* MeshCore format: [dest_hash][src_hash][MAC + encrypted_data] */
    if (pkt->payload_len < 4) {
        log_event("RX MSG (too short)");
        return;
    }

    uint8_t dest_hash = pkt->payload[0];
    uint8_t src_hash = pkt->payload[1];

    /* Check if message is for us */
    if (dest_hash != mesh.our_hash) {
        /* Not for us - don't try to decrypt */
        return;
    }

    /* Try to decrypt with neighbor's shared secret */
    const uint8_t *shared_secret = neighbor_get_shared_secret(src_hash);

    if (shared_secret != nullptr) {
        /* We have a shared secret - try to decrypt */
        uint8_t decrypted[256];
        int dec_len = crypto_mac_then_decrypt(decrypted,
                                               &pkt->payload[2],  /* Skip dest_hash and src_hash */
                                               pkt->payload_len - 2,
                                               shared_secret);

        if (dec_len > 0) {
            /* Successfully decrypted - store in inbox */
            decrypted[dec_len] = '\0';

            struct meshgrid_neighbor *sender = neighbor_find(src_hash);
            const char *sender_name = sender ? sender->name : "Unknown";

            /* Check if this is MeshCore format (has timestamp + txt_type prefix) */
            const char *text_start = (char*)decrypted;
            int text_offset = 0;

            /* MeshCore direct messages start with [timestamp(4)][txt_type(1)][text] */
            if (dec_len >= 5) {
                /* Check if sender is MeshCore firmware */
                bool is_meshcore = (sender && sender->firmware == FW_MESHCORE);

                /* Or heuristically detect: if first 4 bytes look like timestamp and byte 5 is small */
                if (!is_meshcore && decrypted[4] < 16) {
                    /* Likely MeshCore format - skip timestamp and txt_type */
                    is_meshcore = true;
                }

                if (is_meshcore) {
                    text_offset = 5;  /* Skip 4-byte timestamp + 1-byte txt_type */
                    text_start = (char*)&decrypted[5];
                }
            }

            /* Store in direct messages buffer */
            struct message_entry *msg = &direct_messages[direct_msg_index];
            msg->valid = true;
            msg->decrypted = true;
            msg->sender_hash = src_hash;
            snprintf(msg->sender_name, sizeof(msg->sender_name), "%s", sender_name);
            msg->channel_hash = 0;  // Direct message
            msg->timestamp = millis();
            snprintf(msg->text, sizeof(msg->text), "%.*s", (int)(sizeof(msg->text)-1), text_start);

            direct_msg_index = (direct_msg_index + 1) % DIRECT_MESSAGE_BUFFER_SIZE;
            if (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) direct_msg_count++;

            String log_msg = "RX MSG from ";
            log_msg += sender_name;
            log_msg += " (0x";
            log_msg += String(src_hash, HEX);
            log_msg += "): ";
            log_msg += text_start;
            log_event(log_msg);

            /* Send ACK (MeshCore compatible) - only for MeshCore formatted messages */
            if (sender && sender->pubkey[0] != 0 && text_offset == 5) {
                /* Calculate ACK hash: SHA256(timestamp + txt_type + text + sender_pubkey)[0:4]
                 * MeshCore format: [timestamp(4)][txt_type(1)][text]
                 * ACK hash = SHA256([timestamp(4)][txt_type(1)][text] + sender_pubkey)[0:4]
                 * Use strlen on text to exclude AES padding */
                size_t text_len = strlen((char*)&decrypted[5]);  /* Text length (no padding) */
                size_t data_len = 5 + text_len;  /* timestamp(4) + txt_type(1) + text */

                /* Debug: show what we're hashing */
                Serial.print("DEBUG ACK calc: data_len=");
                Serial.print(data_len);
                Serial.print(" text_len=");
                Serial.print(text_len);
                Serial.print(" msg=[");
                for (size_t i = 0; i < min(data_len, (size_t)20); i++) {
                    if (i > 0) Serial.print(" ");
                    Serial.print("0x");
                    if (decrypted[i] < 16) Serial.print("0");
                    Serial.print(decrypted[i], HEX);
                }
                Serial.println("]");

                uint8_t ack_hash_full[32];
                uint8_t ack_data[256 + 32];  /* decrypted + pubkey */
                memcpy(ack_data, decrypted, data_len);  /* timestamp + txt_type + text (no padding) */
                memcpy(&ack_data[data_len], sender->pubkey, MESHGRID_PUBKEY_SIZE);
                crypto_sha256(ack_hash_full, sizeof(ack_hash_full), ack_data, data_len + MESHGRID_PUBKEY_SIZE);

                /* Send simple ACK packet */
                struct meshgrid_packet ack_pkt;
                memset(&ack_pkt, 0, sizeof(ack_pkt));
                ack_pkt.route_type = ROUTE_DIRECT;  /* Send ACK directly back */
                ack_pkt.payload_type = PAYLOAD_ACK;
                ack_pkt.version = 0;
                ack_pkt.header = MESHGRID_MAKE_HEADER(ack_pkt.route_type, ack_pkt.payload_type, ack_pkt.version);

                /* ACK payload: first 4 bytes of hash */
                memcpy(ack_pkt.payload, ack_hash_full, 4);
                ack_pkt.payload_len = 4;

                /* Reverse path for direct reply */
                if (pkt->path_len > 0) {
                    for (int i = 0; i < pkt->path_len && i < MESHGRID_MAX_PATH_SIZE; i++) {
                        ack_pkt.path[i] = pkt->path[pkt->path_len - 1 - i];
                    }
                    ack_pkt.path_len = pkt->path_len;
                } else {
                    /* No path, add our hash */
                    ack_pkt.path[0] = mesh.our_hash;
                    ack_pkt.path_len = 1;
                }

                /* Encode and transmit ACK */
                uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
                int tx_len = meshgrid_packet_encode(&ack_pkt, tx_buf, sizeof(tx_buf));
                if (tx_len > 0) {
                    get_radio()->transmit(tx_buf, tx_len);
                    get_radio()->startReceive();
                    mesh.packets_tx++;

                    /* Debug: show ACK hash */
                    String log_msg = "TX ACK hash=0x";
                    for (int i = 0; i < 4; i++) {
                        if (ack_hash_full[i] < 16) log_msg += "0";
                        log_msg += String(ack_hash_full[i], HEX);
                    }
                    log_msg += " to 0x";
                    log_msg += String(src_hash, HEX);
                    log_event(log_msg);
                }
            }
        } else {
            /* MAC failed - corrupted or wrong key */
            String log_msg = "RX MSG from 0x";
            log_msg += String(src_hash, HEX);
            log_msg += " MAC FAIL ";
            log_msg += String(rssi);
            log_msg += "dBm";
            log_event(log_msg);
        }
    } else {
        /* No shared secret for this sender - store encrypted in direct messages */
        struct message_entry *msg = &direct_messages[direct_msg_index];
        msg->valid = true;
        msg->decrypted = false;
        msg->sender_hash = src_hash;
        snprintf(msg->sender_name, sizeof(msg->sender_name), "0x%02x", src_hash);
        msg->channel_hash = 0;
        msg->timestamp = millis();
        snprintf(msg->text, sizeof(msg->text), "[encrypted %dB - no key]", pkt->payload_len);

        direct_msg_index = (direct_msg_index + 1) % DIRECT_MESSAGE_BUFFER_SIZE;
        if (direct_msg_count < DIRECT_MESSAGE_BUFFER_SIZE) direct_msg_count++;

        String log_msg = "RX MSG from 0x";
        log_msg += String(src_hash, HEX);
        log_msg += " (encrypted - no shared secret)";
        log_event(log_msg);
    }
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

    /* Try to decrypt with known channels */
    const uint8_t *channel_secret = nullptr;
    const char *channel_name = nullptr;

    /* Check public channel */
    if (channel_hash == public_channel_hash) {
        channel_secret = public_channel_secret;
        channel_name = "Public";
    } else {
        /* Check custom channels */
        for (int i = 0; i < custom_channel_count; i++) {
            if (custom_channels[i].valid && custom_channels[i].hash == channel_hash) {
                channel_secret = custom_channels[i].secret;
                channel_name = custom_channels[i].name;
                break;
            }
        }
    }

    if (channel_secret != nullptr) {
        /* Attempt to decrypt */
        uint8_t decrypted[256];
        int dec_len = crypto_mac_then_decrypt(decrypted,
                                               &pkt->payload[1],  /* Skip channel hash */
                                               pkt->payload_len - 1,
                                               channel_secret);

        if (dec_len > 0) {
            /* Successfully decrypted - parse MeshCore format */
            decrypted[dec_len] = '\0';

            /* MeshCore format: [timestamp(4)][txt_type(1)][sender_name: text] */
            if (dec_len > 5) {
                uint8_t txt_type = decrypted[4];

                if ((txt_type >> 2) == 0) {  /* Plain text (type 0) */
                    /* Text starts at offset 5 */
                    const char *text = (const char*)&decrypted[5];

                    struct message_entry *msg;
                    int *msg_index;
                    int *msg_count;
                    int buffer_size;

                    /* Determine which buffer to use */
                    if (channel_hash == public_channel_hash) {
                        /* Public channel */
                        msg = &public_messages[public_msg_index];
                        msg_index = &public_msg_index;
                        msg_count = &public_msg_count;
                        buffer_size = PUBLIC_MESSAGE_BUFFER_SIZE;
                    } else {
                        /* Find custom channel index */
                        int ch_idx = -1;
                        for (int i = 0; i < custom_channel_count; i++) {
                            if (custom_channels[i].valid && custom_channels[i].hash == channel_hash) {
                                ch_idx = i;
                                break;
                            }
                        }

                        if (ch_idx >= 0) {
                            msg = &channel_messages[ch_idx][channel_msg_index[ch_idx]];
                            msg_index = &channel_msg_index[ch_idx];
                            msg_count = &channel_msg_count[ch_idx];
                            buffer_size = CHANNEL_MESSAGE_BUFFER_SIZE;
                        } else {
                            /* Unknown channel - skip storing */
                            goto log_only;
                        }
                    }

                    msg->valid = true;
                    msg->decrypted = true;
                    msg->sender_hash = (pkt->path_len > 0) ? pkt->path[0] : 0;
                    snprintf(msg->sender_name, sizeof(msg->sender_name), "0x%02x", msg->sender_hash);
                    msg->channel_hash = channel_hash;
                    msg->timestamp = millis();
                    snprintf(msg->text, sizeof(msg->text), "%.*s", (int)(sizeof(msg->text)-1), text);

                    *msg_index = (*msg_index + 1) % buffer_size;
                    if (*msg_count < buffer_size) (*msg_count)++;

                log_only:

                    String log_msg = "RX GRP [";
                    log_msg += channel_name;
                    log_msg += "] ";
                    log_msg += String(rssi);
                    log_msg += "dBm: ";
                    log_msg += String(text);
                    log_event(log_msg);
                }
            }
        } else {
            /* MAC verification failed */
            String log_msg = "RX GRP [";
            log_msg += channel_name;
            log_msg += "] MAC FAIL ";
            log_msg += String(rssi);
            log_msg += "dBm";
            log_event(log_msg);
        }
    } else {
        /* Unknown channel - store encrypted in public buffer (fallback) */
        struct message_entry *msg = &public_messages[public_msg_index];
        msg->valid = true;
        msg->decrypted = false;
        msg->sender_hash = (pkt->path_len > 0) ? pkt->path[0] : 0;
        snprintf(msg->sender_name, sizeof(msg->sender_name), "0x%02x", msg->sender_hash);
        msg->channel_hash = channel_hash;
        msg->timestamp = millis();
        snprintf(msg->text, sizeof(msg->text), "[encrypted %dB - channel 0x%02x]",
                 pkt->payload_len, channel_hash);

        public_msg_index = (public_msg_index + 1) % PUBLIC_MESSAGE_BUFFER_SIZE;
        if (public_msg_count < PUBLIC_MESSAGE_BUFFER_SIZE) public_msg_count++;

        String log_msg = "RX GRP [0x";
        log_msg += String(channel_hash, HEX);
        log_msg += "] ";
        log_msg += String(pkt->payload_len);
        log_msg += "B ";
        log_msg += String(rssi);
        log_msg += "dBm (encrypted - unknown channel)";
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
    pkt.header = MESHGRID_MAKE_HEADER(pkt.route_type, pkt.payload_type, pkt.version);  /* Update header */

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
        get_radio()->transmit(tx_buf, tx_len);
        get_radio()->startReceive();
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

void send_text_message(uint8_t dest_hash, const char *text) {
    /* Get shared secret for destination */
    const uint8_t *shared_secret = neighbor_get_shared_secret(dest_hash);
    if (shared_secret == nullptr) {
        log_event("TX MSG FAIL: No shared secret for destination");
        return;
    }

    struct meshgrid_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Set packet header */
    pkt.route_type = ROUTE_FLOOD;
    pkt.payload_type = PAYLOAD_TXT_MSG;
    pkt.version = 0;
    pkt.header = MESHGRID_MAKE_HEADER(pkt.route_type, pkt.payload_type, pkt.version);

    /* MeshCore format: [dest_hash][src_hash][MAC + encrypted_data] */
    int i = 0;
    pkt.payload[i++] = dest_hash;
    pkt.payload[i++] = mesh.our_hash;

    /* Build plaintext with MeshCore-compatible format: [timestamp(4)][txt_type(1)][text] */
    uint8_t plaintext[256];
    int plaintext_len = 0;

    /* Timestamp (4 bytes) - use uptime in seconds */
    uint32_t timestamp = millis() / 1000;
    memcpy(&plaintext[plaintext_len], &timestamp, 4);
    plaintext_len += 4;

    /* Text type (1 byte): TXT_TYPE_PLAIN = 0 */
    plaintext[plaintext_len++] = 0;

    /* Message text */
    int text_len = strlen(text);
    memcpy(&plaintext[plaintext_len], text, text_len);
    plaintext_len += text_len;

    /* Encrypt the formatted message */
    int enc_len = crypto_encrypt_then_mac(&pkt.payload[i],
                                          plaintext,
                                          plaintext_len,
                                          shared_secret);

    pkt.payload_len = i + enc_len;

    /* Add our hash to start of path */
    pkt.path[0] = mesh.our_hash;
    pkt.path_len = 1;

    /* Encode and transmit */
    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        /* DEBUG: Print packet details */
        Serial.print("DEBUG TX type=");
        Serial.print(pkt.payload_type);
        Serial.print(" dest=0x");
        Serial.print(dest_hash, HEX);
        Serial.print(" src=0x");
        Serial.print(mesh.our_hash, HEX);
        Serial.print(" payload_len=");
        Serial.print(pkt.payload_len);
        Serial.print(" header=0x");
        Serial.print(pkt.header, HEX);
        Serial.print(" payload: ");
        for (int j = 0; j < min(12, (int)pkt.payload_len); j++) {
            Serial.print("0x");
            Serial.print(pkt.payload[j], HEX);
            Serial.print(" ");
        }
        Serial.println();

        get_radio()->transmit(tx_buf, tx_len);
        get_radio()->startReceive();
        mesh.packets_tx++;
        led_blink();

        struct meshgrid_neighbor *dest = neighbor_find(dest_hash);
        String log_msg = "TX MSG to ";
        log_msg += dest ? dest->name : "0x" + String(dest_hash, HEX);
        log_msg += ": ";
        log_msg += text;
        log_event(log_msg);
    }
}

/*
 * Send encrypted group message to public channel
 */
void send_group_message(const char *text) {
    send_channel_message(public_channel_hash, public_channel_secret, text, "Public");
}

/*
 * Send encrypted message to any channel
 */
void send_channel_message(uint8_t channel_hash, const uint8_t *channel_secret, const char *text, const char *channel_name) {
    struct meshgrid_packet pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Set packet header */
    pkt.route_type = ROUTE_FLOOD;
    pkt.payload_type = PAYLOAD_GRP_TXT;
    pkt.version = 0;
    pkt.header = MESHGRID_MAKE_HEADER(pkt.route_type, pkt.payload_type, pkt.version);

    /* Payload format: [channel_hash][MAC + encrypted([timestamp(4)][type(1)][sender: text])] */
    pkt.payload[0] = channel_hash;

    /* Build plaintext data to encrypt (MeshCore format) */
    uint8_t plaintext[200];
    int i = 0;

    /* Timestamp (4 bytes) */
    uint32_t timestamp = millis() / 1000;
    memcpy(&plaintext[i], &timestamp, 4);
    i += 4;

    /* Text type (1 byte): 0 = plain text */
    plaintext[i++] = 0;

    /* Message with sender prefix: "sender_name: text" */
    int prefix_len = snprintf((char*)&plaintext[i], sizeof(plaintext) - i, "%s: %s", mesh.name, text);
    if (prefix_len > 0) i += prefix_len;

    /* DEBUG: Print plaintext before encryption */
    Serial.print("DEBUG PLAINTEXT len=");
    Serial.print(i);
    Serial.print(" bytes: ");
    for (int j = 0; j < min(i, 64); j++) {
        if (j > 0) Serial.print(" ");
        Serial.print("0x");
        if (plaintext[j] < 16) Serial.print("0");
        Serial.print(plaintext[j], HEX);
    }
    Serial.println();

    /* Encrypt the formatted message */
    int enc_len = crypto_encrypt_then_mac(&pkt.payload[1],
                                          plaintext,
                                          i,
                                          channel_secret);

    pkt.payload_len = 1 + enc_len;  /* channel hash + encrypted data */

    /* Add our hash to start of path */
    pkt.path[0] = mesh.our_hash;
    pkt.path_len = 1;

    /* Encode and transmit */
    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        /* DEBUG: Print packet details */
        Serial.print("DEBUG TX GRP type=");
        Serial.print(pkt.payload_type);
        Serial.print(" channel=0x");
        Serial.print(channel_hash, HEX);
        Serial.print(" payload_len=");
        Serial.print(pkt.payload_len);
        Serial.print(" enc_len=");
        Serial.print(enc_len);
        Serial.print(" header=0x");
        Serial.print(pkt.header, HEX);
        Serial.print(" payload: ");
        for (int j = 0; j < min(12, (int)pkt.payload_len); j++) {
            if (j > 0) Serial.print(" ");
            Serial.print("0x");
            if (pkt.payload[j] < 16) Serial.print("0");
            Serial.print(pkt.payload[j], HEX);
        }
        Serial.println();

        get_radio()->transmit(tx_buf, tx_len);
        get_radio()->startReceive();
        mesh.packets_tx++;
        led_blink();
        String log_msg = "TX GRP [";
        log_msg += channel_name;
        log_msg += "]: ";
        log_msg += text;
        log_event(log_msg);
    }
}
