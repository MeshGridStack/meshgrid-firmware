/**
 * Display screen rendering for Meshgrid firmware
 */

#include "screens.h"
#include "utils/debug.h"
#include "utils/ui_lib.h"
#include "utils/types.h"
#include "hardware/board.h"
#include "utils/constants.h"
#include "core/neighbors.h"
#include "core/messaging.h"
#include "core/security.h"
#include "hardware/telemetry/telemetry.h"
#include "version.h"

/* External references to global state (defined in main.cpp) */
extern struct meshgrid_state mesh;
extern enum meshgrid_device_mode device_mode;
extern const struct board_config *board;
extern struct telemetry_data telemetry;

/* Radio config structure (defined in main.cpp) */
struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
};
extern struct radio_config_t radio_config;

/* External references to statistics */
extern uint32_t stat_flood_rx;
extern uint32_t stat_flood_fwd;
extern uint32_t stat_duplicates;
extern uint32_t stat_clients;
extern uint32_t stat_repeaters;
extern uint32_t stat_rooms;

/* External references to neighbor table (declared in app/neighbors.h) */
extern struct meshgrid_neighbor neighbors[];
extern uint16_t neighbor_count;

/* External references to message buffers */
extern struct message_entry public_messages[];
extern int public_msg_count;
extern struct message_entry direct_messages[];
extern int direct_msg_count;

/* External functions */
extern uint32_t get_uptime_secs(void);

/* External global display object (defined in main.cpp) */
extern Adafruit_SSD1306 display_128x64;

/* ========================================================================= */
/* Display Initialization                                                    */
/* ========================================================================= */

int display_init(Adafruit_SSD1306 **display_out) {
    const struct display_pins *pins = &board->display_pins;
    if (board->display == DISPLAY_NONE) {
        *display_out = nullptr;
        return 0;
    }

    DEBUG_INFOF("Display init: SDA=%d SCL=%d RST=%d", pins->sda, pins->scl, pins->reset);

    /* Wire.begin() already called early in setup() */

    /* Manual reset sequence if reset pin is defined */
    if (pins->reset >= 0) {
        pinMode(pins->reset, OUTPUT);
        digitalWrite(pins->reset, LOW);
        delay(10);
        digitalWrite(pins->reset, HIGH);
        delay(50);
    }

    /* Scan I2C bus for display */
    Wire.beginTransmission(pins->addr);
    int i2c_result = Wire.endTransmission();
    DEBUG_INFOF("I2C scan 0x%02x: %s", pins->addr, i2c_result == 0 ? "found" : "not found");

    /* Point to global display object */
    *display_out = &display_128x64;

    /* Initialize display with SWITCHCAPVCC mode
     * Reset already done manually, periphBegin=false since Wire.begin() already called */
    if (!(*display_out)->begin(SSD1306_SWITCHCAPVCC, pins->addr, false, false)) {
        DEBUG_INFO("Display begin() failed!");
        *display_out = nullptr;
        return -1;
    }

    /* Clear and prepare display */
    (*display_out)->clearDisplay();
    (*display_out)->setTextSize(1);
    (*display_out)->setTextColor(SSD1306_WHITE);
    (*display_out)->setCursor(0, 0);
    (*display_out)->display();

    DEBUG_INFO("Display initialized OK");
    return 0;
}

/* ========================================================================= */
/* Helper Functions                                                          */
/* ========================================================================= */

const char* fw_name(enum meshgrid_firmware fw) {
    switch (fw) {
        case FW_MESHGRID: return "MG";
        case FW_MESHCORE: return "MC";
        case FW_MESHTASTIC: return "MT";
        default: return "??";
    }
}

const char* node_type_char(enum meshgrid_node_type t) {
    switch (t) {
        case NODE_TYPE_CLIENT: return "C";
        case NODE_TYPE_REPEATER: return "R";
        case NODE_TYPE_ROOM: return "S";
        default: return "?";
    }
}

static void draw_header(Adafruit_SSD1306 *display, const char *title) {
    int battery = (telemetry.battery_mv > 0) ? telemetry.battery_pct : -1;
    ui_draw_header(display, title, battery);
}

/* ========================================================================= */
/* Screen Rendering Functions                                                */
/* ========================================================================= */

void draw_screen_status(Adafruit_SSD1306 *display) {
    char line[32];

    draw_header(display, "STATUS");

    /* Device identity */
    const char *mode_str = (device_mode == MODE_REPEATER) ? "RPT" : "CLI";
    snprintf(line, sizeof(line), "%s %02X %s", mode_str, mesh.our_hash, mesh.name);
    display->setCursor(0, UI_CONTENT_TOP + 2); display->print(line);

    /* Network summary */
    snprintf(line, sizeof(line), "Net: C:%lu R:%lu S:%lu", stat_clients, stat_repeaters, stat_rooms);
    display->setCursor(0, UI_CONTENT_TOP + 14); display->print(line);

    /* Traffic stats */
    snprintf(line, sizeof(line), "RX:%lu TX:%lu FW:%lu",
             stat_flood_rx, mesh.packets_tx, stat_flood_fwd);
    display->setCursor(0, UI_CONTENT_TOP + 26); display->print(line);

    /* Uptime and temperature */
    uint32_t up = get_uptime_secs();
    if (telemetry.has_temp) {
        snprintf(line, sizeof(line), "Up %luh%lum %d.%dC",
                 up / 3600, (up % 3600) / 60,
                 telemetry.temp_deci_c / 10, abs(telemetry.temp_deci_c % 10));
    } else {
        snprintf(line, sizeof(line), "Uptime: %luh%lum", up / 3600, (up % 3600) / 60);
    }
    display->setCursor(0, UI_CONTENT_TOP + 38); display->print(line);

    /* Footer */
    ui_draw_footer(display, "[BTN:next]", 1, SCREEN_COUNT);
}

void draw_screen_neighbors(Adafruit_SSD1306 *display, struct display_state *state) {
    char line[32];

    snprintf(line, sizeof(line), "NODES (%d)", neighbor_count);
    draw_header(display, line);

    if (neighbor_count == 0) {
        ui_draw_centered_text(display, UI_CONTENT_TOP + 14, "No nodes seen");
        ui_draw_centered_text(display, UI_CONTENT_TOP + 26, "Listening...");
    } else {
        /* Build list items from neighbors */
        const int max_visible = 4;
        int start = state->neighbor_scroll;
        int end = (neighbor_count < start + max_visible) ? neighbor_count : start + max_visible;

        int y = UI_CONTENT_TOP;
        for (int i = start; i < end; i++) {
            struct meshgrid_neighbor *n = &neighbors[i];

            /* Build primary text: Type + Name + Firmware */
            char name_short[10];
            ui_truncate_text(name_short, n->name, 9);
            snprintf(line, sizeof(line), "%s %-9s %s",
                     node_type_char(n->node_type), name_short, fw_name(n->firmware));
            display->setCursor(0, y);
            display->print(line);

            /* Build secondary text: RSSI, hops, age */
            uint32_t age_sec = (millis() - n->last_seen) / 1000;
            char age_str[8];
            ui_format_duration(age_str, sizeof(age_str), age_sec);

            snprintf(line, sizeof(line), "%d %dh %s", n->rssi, n->hops, age_str);
            int text_width = strlen(line) * 6;
            display->setCursor(UI_SCREEN_WIDTH - text_width - 2, y);
            display->print(line);

            y += UI_SPACING_TIGHT;
        }

        /* Scroll indicator */
        if (neighbor_count > max_visible) {
            snprintf(line, sizeof(line), "%d-%d/%d", start + 1, end, neighbor_count);
            int text_width = strlen(line) * 6;
            display->setCursor(UI_SCREEN_WIDTH - text_width - 2, UI_SCREEN_HEIGHT - 8);
            display->print(line);
        }
    }

    ui_draw_footer(display, "[BTN:scroll]", 2, SCREEN_COUNT);
}

void draw_screen_messages(Adafruit_SSD1306 *display, struct display_state *state) {
    char line[32];

    /* Count total unread/recent messages */
    int total_messages = public_msg_count + direct_msg_count;
    snprintf(line, sizeof(line), "MESSAGES (%d)", total_messages);
    draw_header(display, line);

    if (total_messages == 0) {
        ui_draw_centered_text(display, UI_CONTENT_TOP + 14, "No messages");
        ui_draw_centered_text(display, UI_CONTENT_TOP + 26, "yet");
    } else {
        /* Show recent messages with scroll */
        const int max_visible = 4;
        int start = state->message_scroll;
        int displayed = 0;
        int y = UI_CONTENT_TOP;

        /* Display public messages first, then direct messages */
        for (int i = public_msg_count - 1; i >= 0 && displayed < max_visible; i--) {
            if (displayed < start) {
                displayed++;
                continue;
            }

            struct message_entry *msg = &public_messages[i];
            if (!msg->valid) continue;

            /* Line format: [Sender] Message preview... */
            char sender_short[10];
            ui_truncate_text(sender_short, msg->sender_name, 8);

            char text_preview[16];
            ui_truncate_text(text_preview, msg->text, 15);

            snprintf(line, sizeof(line), "%s: %s", sender_short, text_preview);
            display->setCursor(0, y);
            display->print(line);

            /* Show age on right */
            uint32_t age_sec = (millis() - msg->timestamp) / 1000;
            char age_str[8];
            ui_format_duration(age_str, sizeof(age_str), age_sec);
            int text_width = strlen(age_str) * 6;
            display->setCursor(UI_SCREEN_WIDTH - text_width - 2, y);
            display->print(age_str);

            y += UI_SPACING_TIGHT;
            displayed++;
        }

        /* Then direct messages */
        for (int i = direct_msg_count - 1; i >= 0 && displayed < max_visible; i--) {
            if (displayed < start) {
                displayed++;
                continue;
            }

            struct message_entry *msg = &direct_messages[i];
            if (!msg->valid) continue;

            char sender_short[10];
            ui_truncate_text(sender_short, msg->sender_name, 8);

            char text_preview[14];
            ui_truncate_text(text_preview, msg->text, 13);

            snprintf(line, sizeof(line), "[%s] %s", sender_short, text_preview);
            display->setCursor(0, y);
            display->print(line);

            uint32_t age_sec = (millis() - msg->timestamp) / 1000;
            char age_str[8];
            ui_format_duration(age_str, sizeof(age_str), age_sec);
            int text_width = strlen(age_str) * 6;
            display->setCursor(UI_SCREEN_WIDTH - text_width - 2, y);
            display->print(age_str);

            y += UI_SPACING_TIGHT;
            displayed++;
        }

        /* Scroll indicator */
        if (total_messages > max_visible) {
            int end = start + displayed;
            snprintf(line, sizeof(line), "%d-%d/%d", start + 1, end, total_messages);
            int text_width = strlen(line) * 6;
            display->setCursor(UI_SCREEN_WIDTH - text_width - 2, UI_SCREEN_HEIGHT - 8);
            display->print(line);
        }
    }

    ui_draw_footer(display, "[BTN:scroll]", 3, SCREEN_COUNT);
}

void draw_screen_stats(Adafruit_SSD1306 *display) {
    char line[32];

    draw_header(display, "STATISTICS");

    int y = UI_CONTENT_TOP + 2;

    /* Packet statistics with better formatting */
    snprintf(line, sizeof(line), "RX:  %lu pkts", mesh.packets_rx);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    snprintf(line, sizeof(line), "TX:  %lu pkts", mesh.packets_tx);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    snprintf(line, sizeof(line), "FWD: %lu pkts", mesh.packets_fwd);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    snprintf(line, sizeof(line), "DUP: %lu pkts", stat_duplicates);
    display->setCursor(0, y); display->print(line);

    ui_draw_footer(display, "[BTN:next]", 4, SCREEN_COUNT);
}

void draw_screen_info(Adafruit_SSD1306 *display) {
    char line[32];

    draw_header(display, "DEVICE INFO");

    /* Firmware version with icon */
    snprintf(line, sizeof(line), "Firmware: v%s", MESHGRID_VERSION);
    display->setCursor(0, 16); display->print(line);

    /* Board info - truncate if too long */
    char board_name[20];
    snprintf(board_name, sizeof(board_name), "%s %s", board->vendor, board->name);
    display->setCursor(0, 28);
    display->print("Board: ");
    display->print(board_name);

    /* Device hash */
    snprintf(line, sizeof(line), "ID Hash: 0x%02X", mesh.our_hash);
    display->setCursor(0, 40); display->print(line);

    /* Memory info - show in KB for readability */
    unsigned long heap_kb = telemetry.free_heap / 1024;
    snprintf(line, sizeof(line), "Free Mem: %lu KB", heap_kb);
    display->setCursor(0, 52); display->print(line);

    /* Footer with hint */
    ui_draw_footer(display, "[LONG:advert]", 5, SCREEN_COUNT);
}

void draw_screen_radio(Adafruit_SSD1306 *display) {
    char line[32];

    draw_header(display, "RADIO CONFIG");

    /* Determine preset name based on frequency */
    const char *preset = "CUSTOM";
    if (radio_config.frequency >= 869.0 && radio_config.frequency <= 870.0) {
        preset = "EU";
    } else if (radio_config.frequency >= 914.0 && radio_config.frequency <= 916.0) {
        preset = "US";
    }

    int y = UI_CONTENT_TOP + 2;

    /* Preset and frequency */
    snprintf(line, sizeof(line), "%s: %.3f MHz", preset, radio_config.frequency);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    /* TX power */
    snprintf(line, sizeof(line), "Power: %d dBm", radio_config.tx_power);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    /* Bandwidth and spreading factor */
    snprintf(line, sizeof(line), "BW:%.1fkHz SF:%d",
             radio_config.bandwidth, radio_config.spreading_factor);
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    /* Coding rate and preamble */
    snprintf(line, sizeof(line), "CR:4/%d Pre:%d",
             radio_config.coding_rate, radio_config.preamble_len);
    display->setCursor(0, y); display->print(line);

    ui_draw_footer(display, "[BTN:next]", 6, SCREEN_COUNT);
}

void draw_screen_security(Adafruit_SSD1306 *display) {
    char line[32];

    draw_header(display, "SECURITY");

    int y = UI_CONTENT_TOP + 2;

    /* PIN (large display) */
    display->setTextSize(2);
    display->setCursor(10, y);
    display->print("PIN:");
    y += 18;

    display->setCursor(10, y);
    display->print(security.pin);
    y += 20;

    /* Status (normal size) */
    display->setTextSize(1);
    snprintf(line, sizeof(line), "Auth: %s", security.pin_enabled ? "ENABLED" : "DISABLED");
    display->setCursor(0, y); display->print(line);
    y += UI_SPACING_NORMAL;

    /* Instructions */
    display->setCursor(0, y); display->print("Connect: AUTH <pin>");

    ui_draw_footer(display, "[BTN:next]", 7, SCREEN_COUNT);
}

/* ========================================================================= */
/* Display State Management                                                  */
/* ========================================================================= */

void display_state_init(struct display_state *state) {
    state->current_screen = SCREEN_STATUS;
    state->dirty = true;
    state->neighbor_scroll = 0;
    state->message_scroll = 0;
}

void display_update(Adafruit_SSD1306 *display, struct display_state *state) {
    if (!display) return;

    /* Force refresh every second for age updates, or when dirty */
    static uint32_t last_refresh = 0;
    if (!state->dirty && (millis() - last_refresh < 1000)) return;
    last_refresh = millis();
    state->dirty = false;

    display->clearDisplay();
    display->setTextSize(1);

    switch (state->current_screen) {
        case SCREEN_STATUS:
            draw_screen_status(display);
            break;
        case SCREEN_NEIGHBORS:
            draw_screen_neighbors(display, state);
            break;
        case SCREEN_MESSAGES:
            draw_screen_messages(display, state);
            break;
        case SCREEN_STATS:
            draw_screen_stats(display);
            break;
        case SCREEN_INFO:
            draw_screen_info(display);
            break;
        case SCREEN_RADIO:
            draw_screen_radio(display);
            break;
        case SCREEN_SECURITY:
            draw_screen_security(display);
            break;
        default:
            break;
    }

    display->display();
}

/* ========================================================================= */
/* Navigation Functions                                                      */
/* ========================================================================= */

void display_next_screen(struct display_state *state) {
    state->current_screen = (enum display_screen)((state->current_screen + 1) % SCREEN_COUNT);
    state->neighbor_scroll = 0;
    state->message_scroll = 0;
    state->dirty = true;
}

void display_scroll_up(struct display_state *state) {
    if (state->current_screen == SCREEN_NEIGHBORS && state->neighbor_scroll > 0) {
        state->neighbor_scroll--;
        state->dirty = true;
    } else if (state->current_screen == SCREEN_MESSAGES) {
        int total = public_msg_count + direct_msg_count;
        if (total > 4 && state->message_scroll > 0) {
            state->message_scroll--;
            state->dirty = true;
        }
    }
}

void display_scroll_down(struct display_state *state) {
    if (state->current_screen == SCREEN_NEIGHBORS && neighbor_count > 4) {
        if (state->neighbor_scroll < neighbor_count - 4) {
            state->neighbor_scroll++;
            state->dirty = true;
        } else {
            /* At end of list, go to next screen */
            display_next_screen(state);
        }
    } else if (state->current_screen == SCREEN_MESSAGES) {
        int total = public_msg_count + direct_msg_count;
        if (total > 4 && state->message_scroll < total - 4) {
            state->message_scroll++;
            state->dirty = true;
        } else {
            /* At end of list, go to next screen */
            display_next_screen(state);
        }
    } else {
        /* No scrolling on this screen, just go to next */
        display_next_screen(state);
    }
}
