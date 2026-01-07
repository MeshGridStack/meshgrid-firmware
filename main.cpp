/**
 * meshgrid-firmware - MeshCore-compatible mesh networking
 *
 * Supports three device modes:
 *   - REPEATER: Forward packets only, minimal power usage
 *   - ROOM: Group chat server with MQTT bridge (optional)
 *   - CLIENT: Full client for sending/receiving messages
 *
 * Fully compatible with MeshCore protocol for interoperability.
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>

#include "lib/board.h"
#include "version.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/telemetry/telemetry.h"
#include "drivers/test/hw_test.h"
#include "drivers/crypto/crypto.h"
}

#include "boards/boards.h"

/*
 * Configuration - set via serial commands or stored in flash
 */
static enum meshgrid_device_mode device_mode = MODE_REPEATER;

/*
 * Board and hardware
 */
static const struct board_config *board;
SX1262 *radio = nullptr;  /* Non-static for hw_test access */
static SPIClass *radio_spi = nullptr;
static U8G2 *display = nullptr;

/*
 * Mesh state
 */
static struct meshgrid_state mesh;
static uint32_t boot_time;

/*
 * Seen packets table (for deduplication)
 */
#define SEEN_TABLE_SIZE 64
static struct {
    uint8_t hash;
    uint32_t time;
} seen_table[SEEN_TABLE_SIZE];
static uint8_t seen_idx = 0;

/*
 * Neighbor table
 */
#define MAX_NEIGHBORS 32
static struct meshgrid_neighbor neighbors[MAX_NEIGHBORS];
static uint8_t neighbor_count = 0;

/*
 * Display screens
 */
enum display_screen {
    SCREEN_STATUS = 0,      /* Main status overview */
    SCREEN_NEIGHBORS = 1,   /* Neighbor list */
    SCREEN_STATS = 2,       /* Detailed statistics */
    SCREEN_INFO = 3,        /* Device info */
    SCREEN_COUNT = 4
};

static enum display_screen current_screen = SCREEN_STATUS;
static bool display_dirty = true;
static uint8_t neighbor_scroll = 0;  /* Scroll offset for neighbor list */
static uint32_t last_activity_time = 0;

/*
 * Button state
 */
static bool button_pressed = false;
static uint32_t button_press_time = 0;
static uint32_t last_button_check = 0;
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 1000

/*
 * Statistics
 */
static uint32_t stat_flood_rx = 0;
static uint32_t stat_flood_fwd = 0;
static uint32_t stat_duplicates = 0;
static uint32_t stat_clients = 0;
static uint32_t stat_repeaters = 0;
static uint32_t stat_rooms = 0;

/*
 * Monitor mode - output ADV/MSG events when true
 */
static bool monitor_mode = false;

/*
 * Telemetry
 */
static struct telemetry_data telemetry;
static uint32_t last_telemetry_read = 0;
#define TELEMETRY_READ_INTERVAL_MS 5000

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

static void led_blink(void) {
    if (board->power_pins.led < 0) return;
    digitalWrite(board->power_pins.led, HIGH);
    delay(30);
    digitalWrite(board->power_pins.led, LOW);
}

static uint8_t random_byte(void) {
    return (uint8_t)random(256);
}

static uint32_t get_uptime_secs(void) {
    return (millis() - boot_time) / 1000;
}

/* ========================================================================= */
/* Seen table - packet deduplication                                         */
/* ========================================================================= */

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

/* ========================================================================= */
/* Neighbor management                                                       */
/* ========================================================================= */

static struct meshgrid_neighbor *neighbor_find(uint8_t hash) {
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbors[i].hash == hash) {
            return &neighbors[i];
        }
    }
    return nullptr;
}

/* Infer node type from name prefix */
static enum meshgrid_node_type infer_node_type(const char *name) {
    if (strncmp(name, "rpt-", 4) == 0 || strncmp(name, "RPT", 3) == 0 ||
        strstr(name, "relay") || strstr(name, "Relay") ||
        strstr(name, "repeater") || strstr(name, "Repeater")) {
        return NODE_TYPE_REPEATER;
    }
    if (strncmp(name, "room-", 5) == 0 || strncmp(name, "Room", 4) == 0 ||
        strstr(name, "server") || strstr(name, "Server")) {
        return NODE_TYPE_ROOM;
    }
    /* Default to client for user-like names */
    return NODE_TYPE_CLIENT;
}

/* Infer firmware from name prefix */
static enum meshgrid_firmware infer_firmware(const char *name) {
    if (strncmp(name, "mg-", 3) == 0 || strncmp(name, "MG-", 3) == 0) {
        return FW_MESHGRID;
    }
    if (strncmp(name, "Meshtastic", 10) == 0 || strstr(name, "!")) {
        return FW_MESHTASTIC;
    }
    /* Most nodes are MeshCore */
    return FW_MESHCORE;
}

static void neighbor_update(const uint8_t *pubkey, const char *name,
                            uint32_t timestamp, int16_t rssi, int8_t snr,
                            uint8_t hops) {
    uint8_t hash = meshgrid_hash_pubkey(pubkey);
    struct meshgrid_neighbor *n = neighbor_find(hash);
    bool is_new = false;

    if (n == nullptr) {
        is_new = true;
        /* Add new neighbor */
        if (neighbor_count >= MAX_NEIGHBORS) {
            /* Table full - remove oldest */
            uint32_t oldest_time = neighbors[0].last_seen;
            int oldest_idx = 0;
            for (int i = 1; i < neighbor_count; i++) {
                if (neighbors[i].last_seen < oldest_time) {
                    oldest_time = neighbors[i].last_seen;
                    oldest_idx = i;
                }
            }
            /* Decrement stat for old node type */
            switch (neighbors[oldest_idx].node_type) {
                case NODE_TYPE_CLIENT: if (stat_clients > 0) stat_clients--; break;
                case NODE_TYPE_REPEATER: if (stat_repeaters > 0) stat_repeaters--; break;
                case NODE_TYPE_ROOM: if (stat_rooms > 0) stat_rooms--; break;
                default: break;
            }
            n = &neighbors[oldest_idx];
        } else {
            n = &neighbors[neighbor_count++];
        }

        memcpy(n->pubkey, pubkey, MESHGRID_PUBKEY_SIZE);
        n->hash = hash;
        strncpy(n->name, name, MESHGRID_NODE_NAME_MAX);
        n->name[MESHGRID_NODE_NAME_MAX] = '\0';
        n->node_type = infer_node_type(name);
        n->firmware = infer_firmware(name);
        n->hops = hops;

        /* Update type stats */
        switch (n->node_type) {
            case NODE_TYPE_CLIENT: stat_clients++; break;
            case NODE_TYPE_REPEATER: stat_repeaters++; break;
            case NODE_TYPE_ROOM: stat_rooms++; break;
            default: break;
        }

        /* Debug info for new neighbor - formatted for easy reading */
        // Serial.print("[NEW] ");
        // Serial.print(name);
        // Serial.print(" (");
        // Serial.print(hash, HEX);
        // Serial.print(") ");
        // Serial.print(n->node_type == NODE_TYPE_REPEATER ? "RPT" :
        //              n->node_type == NODE_TYPE_ROOM ? "ROOM" : "CLI");
        // Serial.print(" RSSI:");
        // Serial.print(rssi);
        // Serial.print(" hops:");
        // Serial.println(hops);
    }

    n->last_seen = millis();
    n->advert_timestamp = timestamp;
    n->rssi = rssi;
    n->snr = snr;
    if (hops < n->hops) n->hops = hops;  /* Track shortest path */
    last_activity_time = millis();
}

/* ========================================================================= */
/* Packet handling                                                           */
/* ========================================================================= */

static void handle_advert(struct meshgrid_packet *pkt, int16_t rssi, int8_t snr) {
    uint8_t pubkey[MESHGRID_PUBKEY_SIZE];
    char name[MESHGRID_NODE_NAME_MAX + 1];
    uint32_t timestamp;

    if (meshgrid_parse_advert(pkt, pubkey, name, sizeof(name), &timestamp) == 0) {
        uint8_t hash = meshgrid_hash_pubkey(pubkey);
        neighbor_update(pubkey, name, timestamp, rssi, snr, pkt->path_len);

        /* Output in CLI-compatible format (only in monitor mode) */
        if (monitor_mode) {
            Serial.print("ADV 0x");
            Serial.print(hash, HEX);
            Serial.print(" ");
            Serial.print(rssi);
            Serial.print(" ");
            Serial.println(name);
        }

        display_dirty = true;
    }
}

static void handle_text_msg(struct meshgrid_packet *pkt, int16_t rssi) {
    /* For now, just log encrypted messages */
    /* TODO: Decrypt if we have the key */
    Serial.print("[TXT] ");
    Serial.print(pkt->payload_len);
    Serial.print(" bytes from path len ");
    Serial.println(pkt->path_len);
}

static void handle_group_msg(struct meshgrid_packet *pkt, int16_t rssi) {
    /* For ROOM mode: decrypt and store/broadcast */
    /* For other modes: just forward */
    Serial.print("[GRP] ");
    Serial.print(pkt->payload_len);
    Serial.println(" bytes");
}

/*
 * Process received packet
 */
static void process_packet(uint8_t *buf, int len, int16_t rssi, int8_t snr) {
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
            return;
        default:
            break;
    }

    /* Forward if appropriate (REPEATER mode always forwards, others may filter) */
    if (meshgrid_should_forward(&pkt, mesh.our_hash)) {
        /* Add ourselves to path */
        meshgrid_path_append(&pkt, mesh.our_hash);

        /* Calculate delay based on path length */
        uint32_t delay_ms = meshgrid_retransmit_delay(&pkt, random_byte());

        /* Re-encode and transmit */
        uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
        int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

        if (tx_len > 0) {
            delay(delay_ms);
            radio->transmit(tx_buf, tx_len);
            radio->startReceive();
            mesh.packets_fwd++;
            stat_flood_fwd++;
            led_blink();
        }
    }
}

/* ========================================================================= */
/* Advertising                                                               */
/* ========================================================================= */

static void send_advertisement(void) {
    struct meshgrid_packet pkt;
    uint32_t timestamp = get_uptime_secs();  /* TODO: Use RTC if available */

    meshgrid_create_advert(&pkt, mesh.pubkey, mesh.name, timestamp);

    /* Add our hash to start of path */
    pkt.path[0] = mesh.our_hash;
    pkt.path_len = 1;

    uint8_t tx_buf[MESHGRID_MAX_PACKET_SIZE];
    int tx_len = meshgrid_packet_encode(&pkt, tx_buf, sizeof(tx_buf));

    if (tx_len > 0) {
        radio->transmit(tx_buf, tx_len);
        radio->startReceive();
        mesh.packets_tx++;
        led_blink();
        // Serial.println("[ADV] Sent advertisement");
    }
}

/* ========================================================================= */
/* Button handling                                                           */
/* ========================================================================= */

static void button_init(void) {
    if (board->power_pins.button >= 0) {
        pinMode(board->power_pins.button, INPUT_PULLUP);
    }
}

static void button_check(void) {
    if (board->power_pins.button < 0) return;
    if (millis() - last_button_check < BUTTON_DEBOUNCE_MS) return;
    last_button_check = millis();

    bool pressed = (digitalRead(board->power_pins.button) == LOW);

    if (pressed && !button_pressed) {
        /* Button just pressed */
        button_pressed = true;
        button_press_time = millis();
    } else if (!pressed && button_pressed) {
        /* Button released */
        uint32_t press_duration = millis() - button_press_time;
        button_pressed = false;

        if (press_duration >= BUTTON_LONG_PRESS_MS) {
            /* Long press: toggle through screens backwards or special action */
            if (current_screen == SCREEN_NEIGHBORS && neighbor_count > 4) {
                /* On neighbor screen: scroll up */
                if (neighbor_scroll > 0) neighbor_scroll--;
            } else {
                /* Send advertisement */
                send_advertisement();
            }
        } else {
            /* Short press: next screen or scroll down */
            if (current_screen == SCREEN_NEIGHBORS && neighbor_count > 4) {
                /* On neighbor screen with many neighbors: scroll first */
                if (neighbor_scroll < neighbor_count - 4) {
                    neighbor_scroll++;
                } else {
                    /* At end of list, go to next screen */
                    neighbor_scroll = 0;
                    current_screen = (enum display_screen)((current_screen + 1) % SCREEN_COUNT);
                }
            } else {
                current_screen = (enum display_screen)((current_screen + 1) % SCREEN_COUNT);
                neighbor_scroll = 0;
            }
        }
        display_dirty = true;
        led_blink();
    }
}

/* ========================================================================= */
/* Display                                                                   */
/* ========================================================================= */

static const char* fw_name(enum meshgrid_firmware fw) {
    switch (fw) {
        case FW_MESHGRID: return "MG";
        case FW_MESHCORE: return "MC";
        case FW_MESHTASTIC: return "MT";
        default: return "??";
    }
}

static const char* node_type_char(enum meshgrid_node_type t) {
    switch (t) {
        case NODE_TYPE_CLIENT: return "C";
        case NODE_TYPE_REPEATER: return "R";
        case NODE_TYPE_ROOM: return "S";
        default: return "?";
    }
}

static void draw_header(const char *title) {
    char line[32];
    display->setFont(u8g2_font_6x10_tf);

    /* Header bar */
    display->drawBox(0, 0, 128, 11);
    display->setDrawColor(0);
    snprintf(line, sizeof(line), " %s ", title);
    display->drawStr(0, 9, line);

    /* Battery indicator on right */
    if (telemetry.battery_mv > 0) {
        snprintf(line, sizeof(line), "%d%%", telemetry.battery_pct);
        display->drawStr(100, 9, line);
    }
    display->setDrawColor(1);
}

static void draw_screen_status(void) {
    char line[32];

    draw_header("STATUS");

    /* Line 2: Our identity */
    const char *mode_str = (device_mode == MODE_REPEATER) ? "RPT" :
                           (device_mode == MODE_ROOM) ? "ROOM" : "CLI";
    snprintf(line, sizeof(line), "%s %02X %s", mode_str, mesh.our_hash, mesh.name);
    display->drawStr(0, 22, line);

    /* Line 3: Network summary with icons */
    snprintf(line, sizeof(line), "C:%lu R:%lu S:%lu", stat_clients, stat_repeaters, stat_rooms);
    display->drawStr(0, 33, line);

    /* Line 4: Traffic stats */
    snprintf(line, sizeof(line), "RX:%lu TX:%lu FWD:%lu",
             stat_flood_rx, mesh.packets_tx, stat_flood_fwd);
    display->drawStr(0, 44, line);

    /* Line 5: Uptime */
    uint32_t up = get_uptime_secs();
    if (telemetry.has_temp) {
        snprintf(line, sizeof(line), "Up %luh%02lum  %d.%dC",
                 up / 3600, (up % 3600) / 60,
                 telemetry.temp_deci_c / 10, abs(telemetry.temp_deci_c % 10));
    } else {
        snprintf(line, sizeof(line), "Uptime: %luh %02lum", up / 3600, (up % 3600) / 60);
    }
    display->drawStr(0, 55, line);

    /* Footer: Screen indicator */
    display->drawStr(0, 64, "[BTN:next]");
    display->drawStr(70, 64, "1/4");
}

static void draw_screen_neighbors(void) {
    char line[32];

    snprintf(line, sizeof(line), "NODES (%d)", neighbor_count);
    draw_header(line);

    if (neighbor_count == 0) {
        display->drawStr(20, 35, "No nodes seen");
        display->drawStr(15, 47, "Listening...");
    } else {
        /* Show up to 4 neighbors with scroll */
        int start = neighbor_scroll;
        int end = (neighbor_count < start + 4) ? neighbor_count : start + 4;

        for (int i = start; i < end; i++) {
            struct meshgrid_neighbor *n = &neighbors[i];
            int y = 22 + (i - start) * 11;

            /* Type icon, name, firmware, signal, hops */
            uint32_t age_sec = (millis() - n->last_seen) / 1000;
            char age_str[8];
            if (age_sec < 60) {
                snprintf(age_str, sizeof(age_str), "%lus", age_sec);
            } else if (age_sec < 3600) {
                snprintf(age_str, sizeof(age_str), "%lum", age_sec / 60);
            } else {
                snprintf(age_str, sizeof(age_str), "%luh", age_sec / 3600);
            }

            snprintf(line, sizeof(line), "%s%-8s %s %4d %dh %s",
                     node_type_char(n->node_type),
                     n->name,
                     fw_name(n->firmware),
                     n->rssi,
                     n->hops,
                     age_str);
            display->drawStr(0, y, line);
        }

        /* Scroll indicator */
        if (neighbor_count > 4) {
            snprintf(line, sizeof(line), "[%d-%d/%d]", start + 1, end, neighbor_count);
            display->drawStr(75, 64, line);
        }
    }

    display->drawStr(0, 64, "[BTN:scroll]");
    display->drawStr(58, 64, "2/4");
}

static void draw_screen_stats(void) {
    char line[32];

    draw_header("STATISTICS");

    /* Detailed packet stats */
    snprintf(line, sizeof(line), "Packets RX: %lu", mesh.packets_rx);
    display->drawStr(0, 22, line);

    snprintf(line, sizeof(line), "Packets TX: %lu", mesh.packets_tx);
    display->drawStr(0, 33, line);

    snprintf(line, sizeof(line), "Forwarded:  %lu", mesh.packets_fwd);
    display->drawStr(0, 44, line);

    snprintf(line, sizeof(line), "Duplicates: %lu", stat_duplicates);
    display->drawStr(0, 55, line);

    display->drawStr(0, 64, "[BTN:next]");
    display->drawStr(70, 64, "3/4");
}

static void draw_screen_info(void) {
    char line[32];

    draw_header("DEVICE INFO");

    snprintf(line, sizeof(line), "FW: meshgrid %s", MESHGRID_VERSION);
    display->drawStr(0, 22, line);

    snprintf(line, sizeof(line), "Board: %s %s", board->vendor, board->name);
    display->drawStr(0, 33, line);

    snprintf(line, sizeof(line), "Hash: %02X", mesh.our_hash);
    display->drawStr(0, 44, line);

    snprintf(line, sizeof(line), "Heap: %lu bytes", (unsigned long)telemetry.free_heap);
    display->drawStr(0, 55, line);

    display->drawStr(0, 64, "[LONG:advert]");
    display->drawStr(70, 64, "4/4");
}

static void display_update(void) {
    if (!display) return;

    /* Force refresh every second for age updates, or when dirty */
    static uint32_t last_refresh = 0;
    if (!display_dirty && (millis() - last_refresh < 1000)) return;
    last_refresh = millis();
    display_dirty = false;

    display->clearBuffer();
    display->setFont(u8g2_font_6x10_tf);

    switch (current_screen) {
        case SCREEN_STATUS:
            draw_screen_status();
            break;
        case SCREEN_NEIGHBORS:
            draw_screen_neighbors();
            break;
        case SCREEN_STATS:
            draw_screen_stats();
            break;
        case SCREEN_INFO:
            draw_screen_info();
            break;
        default:
            break;
    }

    display->sendBuffer();
}

/* ========================================================================= */
/* Serial commands                                                           */
/* ========================================================================= */

/* Helper to print public key as hex */
static void print_pubkey_hex(void) {
    for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
        Serial.printf("%02x", mesh.pubkey[i]);
    }
}

/* CLI JSON commands (for meshgrid-cli) */
static void handle_cli_command(const String &cmd) {
    if (cmd == "INFO") {
        Serial.print("{\"name\":\"");
        Serial.print(mesh.name);
        Serial.print("\",\"node_hash\":");
        Serial.print(mesh.our_hash);
        Serial.print(",\"public_key\":[");
        for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
            if (i > 0) Serial.print(",");
            Serial.print(mesh.pubkey[i]);
        }
        Serial.print("],\"firmware_version\":\"");
        Serial.print(MESHGRID_VERSION);
        Serial.print("\",\"freq_mhz\":");
        Serial.print(board->lora_defaults.frequency, 2);
        Serial.print(",\"tx_power_dbm\":");
        Serial.print(board->lora_defaults.tx_power);
        Serial.println("}");
    } else if (cmd == "NEIGHBORS") {
        Serial.print("[");
        for (int i = 0; i < neighbor_count; i++) {
            if (i > 0) Serial.print(",");
            Serial.print("{\"node_hash\":");
            Serial.print(neighbors[i].hash);
            Serial.print(",\"name\":\"");
            Serial.print(neighbors[i].name);
            Serial.print("\",\"public_key\":[");
            for (int j = 0; j < MESHGRID_PUBKEY_SIZE; j++) {
                if (j > 0) Serial.print(",");
                Serial.print(neighbors[i].pubkey[j]);
            }
            Serial.print("],\"rssi\":");
            Serial.print(neighbors[i].rssi);
            Serial.print(",\"snr\":");
            Serial.print(neighbors[i].snr);
            Serial.print(",\"last_seen_secs\":");
            Serial.print((millis() - neighbors[i].last_seen) / 1000);
            Serial.print("}");
        }
        Serial.println("]");
    } else if (cmd == "TELEMETRY") {
        Serial.print("{\"device\":{\"battery\":");
        Serial.print(telemetry.battery_pct);
        Serial.print(",\"voltage\":");
        Serial.print(telemetry.battery_mv / 1000.0, 3);
        Serial.print(",\"charging\":");
        Serial.print(telemetry.is_charging ? "true" : "false");
        Serial.print(",\"usb\":");
        Serial.print(telemetry.is_usb_power ? "true" : "false");
        Serial.print(",\"uptime\":");
        Serial.print(get_uptime_secs());
        Serial.print(",\"heap\":");
        Serial.print(telemetry.free_heap);
        if (telemetry.has_temp) {
            Serial.print(",\"cpu_temp\":");
            Serial.print(telemetry.temp_deci_c / 10.0, 1);
        }
        Serial.println("}}");
    } else if (cmd == "MONITOR") {
        monitor_mode = true;
        Serial.println("OK");
        /* Monitor mode enabled - will output ADV/MSG/ACK events */
    } else if (cmd == "REBOOT") {
        Serial.println("OK");
        delay(100);
        ESP.restart();
    } else if (cmd == "CONFIG") {
        Serial.print("{\"name\":\"");
        Serial.print(mesh.name);
        Serial.print("\",\"freq_mhz\":");
        Serial.print(board->lora_defaults.frequency, 2);
        Serial.print(",\"tx_power_dbm\":");
        Serial.print(board->lora_defaults.tx_power);
        Serial.print(",\"bandwidth_khz\":");
        Serial.print((int)(board->lora_defaults.bandwidth));
        Serial.print(",\"spreading_factor\":");
        Serial.print(board->lora_defaults.spreading_factor);
        Serial.print(",\"coding_rate\":");
        Serial.print(board->lora_defaults.coding_rate);
        Serial.print(",\"preamble_len\":");
        Serial.print(board->lora_defaults.preamble_len);
        Serial.println("}");
    } else if (cmd.startsWith("SET FREQ ")) {
        float freq = cmd.substring(9).toFloat();
        if (freq >= 137.0 && freq <= 1020.0) {
            radio->setFrequency(freq);
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid frequency");
        }
    } else if (cmd.startsWith("SET BW ")) {
        float bw = cmd.substring(7).toFloat();
        if (radio->setBandwidth(bw) == RADIOLIB_ERR_NONE) {
            Serial.println("OK");
        } else {
            Serial.println("ERR Invalid bandwidth");
        }
    } else if (cmd.startsWith("SET SF ")) {
        int sf = cmd.substring(7).toInt();
        if (sf >= 6 && sf <= 12) {
            if (radio->setSpreadingFactor(sf) == RADIOLIB_ERR_NONE) {
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set SF");
            }
        } else {
            Serial.println("ERR SF must be 6-12");
        }
    } else if (cmd.startsWith("SET CR ")) {
        int cr = cmd.substring(7).toInt();
        if (cr >= 5 && cr <= 8) {
            if (radio->setCodingRate(cr) == RADIOLIB_ERR_NONE) {
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set CR");
            }
        } else {
            Serial.println("ERR CR must be 5-8");
        }
    } else if (cmd.startsWith("SET POWER ")) {
        int power = cmd.substring(10).toInt();
        if (power >= -9 && power <= 22) {
            if (radio->setOutputPower(power) == RADIOLIB_ERR_NONE) {
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set power");
            }
        } else {
            Serial.println("ERR Power must be -9 to 22 dBm");
        }
    } else if (cmd.startsWith("SET PREAMBLE ")) {
        int preamble = cmd.substring(13).toInt();
        if (preamble >= 6 && preamble <= 65535) {
            if (radio->setPreambleLength(preamble) == RADIOLIB_ERR_NONE) {
                Serial.println("OK");
            } else {
                Serial.println("ERR Failed to set preamble");
            }
        } else {
            Serial.println("ERR Preamble must be 6-65535");
        }
    } else if (cmd == "SET PRESET EU_NARROW" || cmd == "SET PRESET EU") {
        /* EU/UK (Narrow) - 869.618 MHz, 62.5 kHz BW, SF8, CR8 */
        radio->setFrequency(869.618);
        radio->setBandwidth(62.5);
        radio->setSpreadingFactor(8);
        radio->setCodingRate(8);
        radio->setPreambleLength(8);
        Serial.println("OK EU/UK Narrow: 869.618MHz 62.5kHz SF8 CR8");
    } else if (cmd == "SET PRESET US_STANDARD" || cmd == "SET PRESET US") {
        /* US Standard - 915 MHz, 250 kHz BW, SF10, CR7 */
        radio->setFrequency(915.0);
        radio->setBandwidth(250.0);
        radio->setSpreadingFactor(10);
        radio->setCodingRate(7);
        radio->setPreambleLength(16);
        Serial.println("OK US Standard: 915MHz 250kHz SF10 CR7");
    } else if (cmd == "SET PRESET US_FAST") {
        /* US Fast - 915 MHz, 500 kHz BW, SF7, CR5 */
        radio->setFrequency(915.0);
        radio->setBandwidth(500.0);
        radio->setSpreadingFactor(7);
        radio->setCodingRate(5);
        radio->setPreambleLength(8);
        Serial.println("OK US Fast: 915MHz 500kHz SF7 CR5");
    } else if (cmd == "SET PRESET LONG_RANGE") {
        /* Long range - SF12, 125 kHz BW */
        radio->setBandwidth(125.0);
        radio->setSpreadingFactor(12);
        radio->setCodingRate(8);
        radio->setPreambleLength(16);
        Serial.println("OK Long Range: 125kHz SF12 CR8");
    } else {
        Serial.print("ERR Unknown command: ");
        Serial.println(cmd);
    }
}

static char serial_cmd_buf[256];
static int serial_cmd_len = 0;

static void handle_serial(void) {
    // Read characters one at a time (non-blocking like MeshCore)
    bool complete = false;
    while (Serial.available() && serial_cmd_len < sizeof(serial_cmd_buf) - 1) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            // Complete line received
            serial_cmd_buf[serial_cmd_len] = '\0';
            complete = true;
            break;
        }
        serial_cmd_buf[serial_cmd_len++] = c;
    }

    // Check if we have a complete command
    if (!complete) {
        return;  // Not a complete line yet
    }

    String cmd(serial_cmd_buf);
    serial_cmd_len = 0;  // Reset for next command
    cmd.trim();
    if (cmd.length() == 0) return;

    /* Echo for debugging */
    if (cmd == "PING") {
        Serial.println("PONG");
        return;
    }

    /* Check for CLI commands (uppercase, no slash) */
    if (cmd == "INFO" || cmd == "NEIGHBORS" || cmd == "TELEMETRY" ||
        cmd == "MONITOR" || cmd == "REBOOT" || cmd == "CONFIG" ||
        cmd.startsWith("SET ") || cmd.startsWith("SEND ") || cmd.startsWith("TRACE ")) {
        handle_cli_command(cmd);
        return;
    }

    /* Human-readable commands */
    if (cmd == "/mode repeater" || cmd == "/mode rpt") {
        device_mode = MODE_REPEATER;
        Serial.println("Mode: REPEATER");
    } else if (cmd == "/mode room") {
        device_mode = MODE_ROOM;
        Serial.println("Mode: ROOM");
    } else if (cmd == "/mode client" || cmd == "/mode cli") {
        device_mode = MODE_CLIENT;
        Serial.println("Mode: CLIENT");
    } else if (cmd == "/advert" || cmd == "/a") {
        send_advertisement();
    } else if (cmd == "/neighbors" || cmd == "/n") {
        Serial.println("Neighbors:");
        for (int i = 0; i < neighbor_count; i++) {
            Serial.print("  ");
            Serial.print(neighbors[i].name);
            Serial.print(" (");
            Serial.print(neighbors[i].hash, HEX);
            Serial.print(") RSSI:");
            Serial.print(neighbors[i].rssi);
            Serial.print(" SNR:");
            Serial.println(neighbors[i].snr);
        }
    } else if (cmd == "/stats" || cmd == "/s") {
        Serial.println("Statistics:");
        Serial.print("  RX: "); Serial.println(mesh.packets_rx);
        Serial.print("  TX: "); Serial.println(mesh.packets_tx);
        Serial.print("  FWD: "); Serial.println(mesh.packets_fwd);
        Serial.print("  DROP: "); Serial.println(mesh.packets_dropped);
        Serial.print("  DUP: "); Serial.println(stat_duplicates);
        Serial.print("  Uptime: "); Serial.print(get_uptime_secs()); Serial.println("s");
    } else if (cmd == "/info" || cmd == "/i") {
        Serial.println("Device Info:");
        Serial.print("  Firmware: meshgrid v"); Serial.println(MESHGRID_VERSION);
        Serial.print("  Build: "); Serial.println(MESHGRID_BUILD_DATE);
        Serial.print("  Board: "); Serial.print(board->vendor); Serial.print(" "); Serial.println(board->name);
        Serial.print("  Node: "); Serial.print(mesh.name); Serial.print(" ("); Serial.print(mesh.our_hash, HEX); Serial.println(")");
        Serial.print("  Mode: "); Serial.println(device_mode == MODE_REPEATER ? "REPEATER" :
                                                  device_mode == MODE_ROOM ? "ROOM" : "CLIENT");
        Serial.print("  Neighbors: "); Serial.println(neighbor_count);
    } else if (cmd == "/test battery" || cmd == "/test bat") {
        Serial.println("Starting battery drain test (1 minute)...");
        struct hw_test_result result;
        hw_test_battery(&result, 60000, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "battery");
        Serial.println(buf);
    } else if (cmd == "/test solar") {
        Serial.println("Starting solar panel test...");
        struct hw_test_result result;
        hw_test_solar(&result, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "solar");
        Serial.println(buf);
    } else if (cmd == "/test radio") {
        Serial.println("Starting radio TX test...");
        struct hw_test_result result;
        hw_test_radio(&result, [](const char *status, uint8_t pct) {
            Serial.print("  ["); Serial.print(pct); Serial.print("%] ");
            Serial.println(status);
        });
        char buf[256];
        hw_test_format_result(buf, sizeof(buf), &result, "radio");
        Serial.println(buf);
    } else if (cmd == "/telemetry" || cmd == "/telem") {
        Serial.println("Telemetry:");
        Serial.print("  Battery: "); Serial.print(telemetry.battery_mv); Serial.print("mV (");
        Serial.print(telemetry.battery_pct); Serial.println("%)");
        Serial.print("  USB Power: "); Serial.println(telemetry.is_usb_power ? "Yes" : "No");
        Serial.print("  Charging: "); Serial.println(telemetry.is_charging ? "Yes" : "No");
        if (telemetry.has_solar) {
            Serial.print("  Solar: "); Serial.print(telemetry.solar_mv); Serial.println("mV");
        }
        if (telemetry.has_temp) {
            Serial.print("  Temp: "); Serial.print(telemetry.temp_deci_c / 10); Serial.print(".");
            Serial.print(abs(telemetry.temp_deci_c % 10)); Serial.println("C");
        }
        Serial.print("  Free Heap: "); Serial.print(telemetry.free_heap); Serial.println(" bytes");
        Serial.print("  Uptime: "); Serial.print(telemetry.uptime_secs); Serial.println("s");
    } else if (cmd == "/help" || cmd == "/h") {
        Serial.println("Commands:");
        Serial.println("  /mode repeater|room|client - Set device mode");
        Serial.println("  /advert - Send advertisement");
        Serial.println("  /neighbors - List neighbors");
        Serial.println("  /stats - Show statistics");
        Serial.println("  /info - Device info");
        Serial.println("  /telemetry - Show telemetry data");
        Serial.println("  /test battery - 1-min battery drain test");
        Serial.println("  /test solar   - Solar panel test");
        Serial.println("  /test radio   - Radio TX test");
    } else {
        Serial.print("Unknown: ");
        Serial.println(cmd);
    }

    display_dirty = true;
}

/* ========================================================================= */
/* Hardware initialization                                                   */
/* ========================================================================= */

static void power_init(void) {
    const struct power_pins *pwr = &board->power_pins;

    if (pwr->vext >= 0) {
        pinMode(pwr->vext, OUTPUT);
        digitalWrite(pwr->vext, pwr->vext_active_low ? LOW : HIGH);
        delay(100);
    }

    if (pwr->led >= 0) {
        pinMode(pwr->led, OUTPUT);
        digitalWrite(pwr->led, LOW);
    }
}

static int radio_init(void) {
    const struct radio_pins *pins = &board->radio_pins;
    const struct lora_config *cfg = &board->lora_defaults;

#if defined(ARCH_ESP32S3) || defined(ARCH_ESP32)
    radio_spi = new SPIClass(HSPI);
    radio_spi->begin(pins->sck, pins->miso, pins->mosi, pins->cs);
#else
    radio_spi = &SPI;
    radio_spi->begin();
#endif

    Module *mod = new Module(pins->cs, pins->dio1, pins->reset, pins->busy, *radio_spi);
    radio = new SX1262(mod);

    Serial.print("Radio init... ");
    int state = radio->begin(cfg->frequency, cfg->bandwidth, cfg->spreading_factor,
                             cfg->coding_rate, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                             cfg->tx_power, cfg->preamble_len);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("FAILED: ");
        Serial.println(state);
        return -1;
    }

    if (cfg->use_crc) radio->setCRC(2);
    Serial.println("OK");
    return 0;
}

static int display_init(void) {
    const struct display_pins *pins = &board->display_pins;
    if (board->display == DISPLAY_NONE) return 0;

    Serial.print("Display init: SDA=");
    Serial.print(pins->sda);
    Serial.print(" SCL=");
    Serial.print(pins->scl);
    Serial.print(" RST=");
    Serial.println(pins->reset);

    /* Initialize I2C with correct pins for this board */
    Wire.begin(pins->sda, pins->scl);
    delay(10);

    /* Manual reset sequence - important for Heltec displays */
    if (pins->reset >= 0) {
        pinMode(pins->reset, OUTPUT);
        digitalWrite(pins->reset, HIGH);
        delay(1);
        digitalWrite(pins->reset, LOW);
        delay(10);
        digitalWrite(pins->reset, HIGH);
        delay(100);  /* Give display time to initialize after reset */
    }

    /* Scan I2C bus for display */
    Wire.beginTransmission(0x3C);
    int i2c_result = Wire.endTransmission();
    Serial.print("I2C scan 0x3C: ");
    Serial.println(i2c_result == 0 ? "found" : "not found");

    /* Create U8G2 display - don't pass SCL/SDA, Wire handles it */
    switch (board->display) {
        case DISPLAY_SSD1306_128X64:
            display = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset= */ U8X8_PIN_NONE);
            break;
        case DISPLAY_SSD1306_128X32:
            display = new U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C(U8G2_R0, /* reset= */ U8X8_PIN_NONE);
            break;
        case DISPLAY_SH1106_128X64:
            display = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, /* reset= */ U8X8_PIN_NONE);
            break;
        default:
            return -1;
    }

    if (!display->begin()) {
        Serial.println("Display begin() failed!");
        delete display;
        display = nullptr;
        return -1;
    }

    display->setFont(u8g2_font_6x10_tf);
    Serial.println("Display initialized OK");
    return 0;
}

static void identity_init(void) {
    /* Initialize crypto subsystem */
    crypto_init();

    /* Generate proper Ed25519 keypair */
    Serial.print("Generating Ed25519 keypair... ");
    if (crypto_generate_keypair(mesh.pubkey, mesh.privkey) == 0) {
        Serial.println("OK");
    } else {
        Serial.println("FAILED!");
    }

    /* Compute hash (MeshCore uses first byte of pubkey) */
    mesh.our_hash = crypto_hash_pubkey(mesh.pubkey);

    /* Generate name from hash */
    snprintf(mesh.name, sizeof(mesh.name), "mg-%02X", mesh.our_hash);

    /* Print pubkey for debugging */
    Serial.print("PubKey: ");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%02X", mesh.pubkey[i]);
    }
    Serial.println("...");
}

/* ========================================================================= */
/* Main                                                                      */
/* ========================================================================= */

static bool radio_ok = true;  // Will be set in setup()

void setup() {
    Serial.begin(115200);
    // ESP32-S3 USB CDC takes time to become active
    // Wait for USB to be ready before sending any output
    delay(2000);

    Serial.println("\n=================================");
    Serial.println("  MESHGRID - MeshCore Compatible");
    Serial.print("  Firmware v"); Serial.println(MESHGRID_VERSION);
    Serial.print("  Build: "); Serial.println(MESHGRID_BUILD_DATE);
    Serial.println("=================================\n");

    board = &CURRENT_BOARD_CONFIG;
    Serial.print("Board: "); Serial.print(board->vendor); Serial.print(" "); Serial.println(board->name);

    if (board->early_init) board->early_init();

    boot_time = millis();
    identity_init();

    Serial.print("Node: "); Serial.print(mesh.name);
    Serial.print(" ("); Serial.print(mesh.our_hash, HEX); Serial.println(")");
    Serial.print("Mode: "); Serial.println(device_mode == MODE_REPEATER ? "REPEATER" :
                                            device_mode == MODE_ROOM ? "ROOM" : "CLIENT");

    power_init();
    button_init();
    telemetry_init();
    display_init();

    if (display) {
        display->clearBuffer();
        display->setFont(u8g2_font_ncenB14_tr);
        display->drawStr(10, 30, "MESHGRID");
        display->setFont(u8g2_font_6x10_tf);
        display->drawStr(20, 50, board->name);
        display->sendBuffer();
        delay(1500);
    }

    radio_ok = (radio_init() == 0);
    if (!radio_ok) {
        Serial.println("FATAL: Radio init failed");
        Serial.println("Serial CLI still available for debugging");
        // DON'T hang - allow serial CLI to work for debugging
    } else {
        Serial.println("Radio init OK");
        if (board->late_init) board->late_init();
        radio->startReceive();
        send_advertisement();
    }

    Serial.println("\nReady! Type /help for commands.\n");
}

void loop() {
    /* Check for received packet */
    if (radio_ok && radio->available()) {
        uint8_t rx_buf[MESHGRID_MAX_PACKET_SIZE];
        int len = radio->getPacketLength();

        if (len > 0 && len <= MESHGRID_MAX_PACKET_SIZE) {
            int state = radio->readData(rx_buf, len);
            if (state == RADIOLIB_ERR_NONE) {
                int16_t rssi = radio->getRSSI();
                int8_t snr = radio->getSNR();
                process_packet(rx_buf, len, rssi, snr);
            }
        }
        radio->startReceive();
    }

    /* Handle serial commands */
    handle_serial();

    /* Handle button presses */
    button_check();

    /* Periodic advertisement (every 5 minutes) */
    static uint32_t last_advert = 0;
    if (millis() - last_advert > MESHGRID_ADVERT_INTERVAL_MS) {
        send_advertisement();
        last_advert = millis();
    }

    /* Read telemetry periodically */
    if (millis() - last_telemetry_read > TELEMETRY_READ_INTERVAL_MS) {
        telemetry_read(&telemetry);
        last_telemetry_read = millis();
        display_dirty = true;
    }

    /* Update display (every 500ms) */
    static uint32_t last_display = 0;
    if (millis() - last_display > 500) {
        display_update();
        last_display = millis();
    }

    /* Heartbeat disabled - interferes with CLI */
    // static uint32_t last_heartbeat = 0;
    // if (millis() - last_heartbeat > 30000) {
    //     Serial.print(".");
    //     last_heartbeat = millis();
    // }
}
