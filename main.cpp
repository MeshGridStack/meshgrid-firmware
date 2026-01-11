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
#include <Adafruit_GFX.h>
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "config/constants.h"
#include "config/memory.h"
#include "lib/types.h"
#include "lib/board.h"
#include "lib/ui_lib.h"
#include "version.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/telemetry/telemetry.h"
#include "drivers/test/hw_test.h"
#include "drivers/crypto/crypto.h"
#include "drivers/power/power.h"
}

#include "drivers/radio/radio_hal.h"
#include "boards/boards.h"

/* Application modules */
#include "app/neighbors.h"
#include "app/channels.h"
#include "app/messaging.h"
#include "app/config.h"
#include "app/commands.h"

/*
 * MeshCore Public Channel (for group messaging support)
 * PSK defined in config/config.h
 */
uint8_t public_channel_secret[32];  /* Decoded PSK - defined here, used in config.cpp */
uint8_t public_channel_hash = 0;     /* Hash of the public channel */

/*
 * Custom channels (stored in NVS)
 * Limit defined in config/memory.h, struct in lib/types.h
 */
struct channel_entry custom_channels[MAX_CUSTOM_CHANNELS];
int custom_channel_count = 0;

/*
 * Configuration - set via serial commands or stored in flash
 */
enum meshgrid_device_mode device_mode = MODE_CLIENT;

/*
 * Board and hardware
 */
const struct board_config *board;

/* Radio instance - holds the actual radio object (exported for radio_api.cpp) */
struct radio_instance radio_inst = {RADIO_NONE, {nullptr}};

/* Radio accessor */
static inline PhysicalLayer* radio() { return radio_inst.as_phy(); }

static SPIClass *radio_spi = nullptr;

/* Create display as global object BEFORE Wire.begin() - like MeshCore does
 * Use -1 for reset pin to avoid crashes, handle reset manually in display_init() */
static Adafruit_SSD1306 display_128x64(128, 64, &Wire, -1);
Adafruit_SSD1306 *display = nullptr;  /* Will point to display_128x64 after board detection */

/*
 * Mesh state
 */
struct meshgrid_state mesh;
uint32_t boot_time;

/*
 * RTC time tracking (set via /time command)
 * Struct defined in lib/types.h
 */
struct rtc_time_t rtc_time = {false, 0};

/*
 * Radio configuration (saved to flash)
 */
Preferences prefs;
struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
} radio_config;

/*
 * Seen packets table (for deduplication)
 * Size in config/memory.h, struct in lib/types.h
 */
struct seen_entry seen_table[SEEN_TABLE_SIZE];
uint8_t seen_idx = 0;

/*
 * Neighbor table - defined in neighbors.cpp
 */
// Neighbors are now in app/neighbors.cpp

/*
 * Display screens
 */
enum display_screen {
    SCREEN_STATUS = 0,      /* Main status overview */
    SCREEN_NEIGHBORS = 1,   /* Neighbor list */
    SCREEN_MESSAGES = 2,    /* Message inbox */
    SCREEN_STATS = 3,       /* Detailed statistics */
    SCREEN_INFO = 4,        /* Device info */
    SCREEN_RADIO = 5,       /* Radio configuration */
    SCREEN_COUNT = 6
};

static enum display_screen current_screen = SCREEN_STATUS;
bool display_dirty = true;
static uint8_t neighbor_scroll = 0;  /* Scroll offset for neighbor list */
static uint8_t message_scroll = 0;   /* Scroll offset for message list */
uint32_t last_activity_time = 0;

/*
 * Button state
 */
static bool button_pressed = false;
static uint32_t button_press_time = 0;
static uint32_t last_button_check = 0;
/* Button timing defined in config/config.h */

/*
 * Statistics
 */
uint32_t stat_flood_rx = 0;
uint32_t stat_flood_fwd = 0;
uint32_t stat_duplicates = 0;
uint32_t stat_clients = 0;
uint32_t stat_repeaters = 0;
uint32_t stat_rooms = 0;

/*
 * Monitor mode - output ADV/MSG events when true
 */
bool monitor_mode = false;

/*
 * Event log buffer (circular)
 */
/* LOG_BUFFER_SIZE defined in config/memory.h */
String log_buffer[LOG_BUFFER_SIZE];
int log_index = 0;
int log_count = 0;
bool log_enabled = false;

/*
 * Message inbox buffers (tiered by channel type)
 * Sizes in config/memory.h, struct in lib/types.h
 */

/* Separate buffers for different message types */
struct message_entry public_messages[PUBLIC_MESSAGE_BUFFER_SIZE];
int public_msg_index = 0;
int public_msg_count = 0;

struct message_entry direct_messages[DIRECT_MESSAGE_BUFFER_SIZE];
int direct_msg_index = 0;
int direct_msg_count = 0;

/* Per-channel message buffers (50 channels × 5 messages each) */
struct message_entry channel_messages[MAX_CUSTOM_CHANNELS][CHANNEL_MESSAGE_BUFFER_SIZE];
int channel_msg_index[MAX_CUSTOM_CHANNELS] = {0};
int channel_msg_count[MAX_CUSTOM_CHANNELS] = {0};

// log_event() moved to app/messaging.cpp

/*
 * Telemetry
 */
struct telemetry_data telemetry;
static uint32_t last_telemetry_read = 0;
/* Telemetry interval defined in config/config.h */

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

void led_blink(void) {
    if (board->power_pins.led < 0) return;
    digitalWrite(board->power_pins.led, HIGH);
    delay(30);
    digitalWrite(board->power_pins.led, LOW);
}

/* Radio accessor helpers - implemented in drivers/radio/radio_api.cpp */

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
            /* Long press: scroll up or special action */
            if (current_screen == SCREEN_NEIGHBORS && neighbor_count > 4) {
                /* On neighbor screen: scroll up */
                if (neighbor_scroll > 0) neighbor_scroll--;
            } else if (current_screen == SCREEN_MESSAGES) {
                /* On message screen: scroll up */
                int total = public_msg_count + direct_msg_count;
                if (total > 4 && message_scroll > 0) message_scroll--;
            } else {
                /* Send local advertisement on button press */
                send_advertisement(ROUTE_DIRECT);
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
            } else if (current_screen == SCREEN_MESSAGES) {
                /* On message screen: scroll down or next screen */
                int total = public_msg_count + direct_msg_count;
                if (total > 4 && message_scroll < total - 4) {
                    message_scroll++;
                } else {
                    message_scroll = 0;
                    current_screen = (enum display_screen)((current_screen + 1) % SCREEN_COUNT);
                }
            } else {
                current_screen = (enum display_screen)((current_screen + 1) % SCREEN_COUNT);
                neighbor_scroll = 0;
                message_scroll = 0;
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
    int battery = (telemetry.battery_mv > 0) ? telemetry.battery_pct : -1;
    ui_draw_header(display, title, battery);
}

static void draw_screen_status(void) {
    char line[32];

    draw_header("STATUS");

    /* Device identity */
    const char *mode_str = (device_mode == MODE_REPEATER) ? "RPT" :
                           (device_mode == MODE_ROOM) ? "ROOM" : "CLI";
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

static void draw_screen_neighbors(void) {
    char line[32];

    snprintf(line, sizeof(line), "NODES (%d)", neighbor_count);
    draw_header(line);

    if (neighbor_count == 0) {
        ui_draw_centered_text(display, UI_CONTENT_TOP + 14, "No nodes seen");
        ui_draw_centered_text(display, UI_CONTENT_TOP + 26, "Listening...");
    } else {
        /* Build list items from neighbors */
        const int max_visible = 4;
        int start = neighbor_scroll;
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

static void draw_screen_messages(void) {
    char line[32];

    /* Count total unread/recent messages */
    int total_messages = public_msg_count + direct_msg_count;
    snprintf(line, sizeof(line), "MESSAGES (%d)", total_messages);
    draw_header(line);

    if (total_messages == 0) {
        ui_draw_centered_text(display, UI_CONTENT_TOP + 14, "No messages");
        ui_draw_centered_text(display, UI_CONTENT_TOP + 26, "yet");
    } else {
        /* Show recent messages with scroll */
        const int max_visible = 4;
        int start = message_scroll;
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

static void draw_screen_stats(void) {
    char line[32];

    draw_header("STATISTICS");

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

static void draw_screen_info(void) {
    char line[32];

    draw_header("DEVICE INFO");

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

static void draw_screen_radio(void) {
    char line[32];

    draw_header("RADIO CONFIG");

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

static void display_update(void) {
    if (!display) return;

    /* Force refresh every second for age updates, or when dirty */
    static uint32_t last_refresh = 0;
    if (!display_dirty && (millis() - last_refresh < 1000)) return;
    last_refresh = millis();
    display_dirty = false;

    display->clearDisplay();
    display->setTextSize(1);

    switch (current_screen) {
        case SCREEN_STATUS:
            draw_screen_status();
            break;
        case SCREEN_NEIGHBORS:
            draw_screen_neighbors();
            break;
        case SCREEN_MESSAGES:
            draw_screen_messages();
            break;
        case SCREEN_STATS:
            draw_screen_stats();
            break;
        case SCREEN_INFO:
            draw_screen_info();
            break;
        case SCREEN_RADIO:
            draw_screen_radio();
            break;
        default:
            break;
    }

    display->display();
}

/* ========================================================================= */
/* Serial commands                                                           */
/* ========================================================================= */

/* Forward declarations - config functions now in app/config.cpp */

/* Helper to print public key as hex */
static void print_pubkey_hex(void) {
    for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
        Serial.printf("%02x", mesh.pubkey[i]);
    }
}

/* ========================================================================= */
/* Hardware initialization                                                   */
/* ========================================================================= */

/* power_init() now provided by drivers/power/power.c */

// init_public_channel(), config_load(), config_save() moved to app/config.cpp

/* Radio interrupt flag - matches MeshCore's approach */
static volatile bool radio_interrupt_flag = false;

#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3)
void ICACHE_RAM_ATTR radio_isr(void) {
    radio_interrupt_flag = true;
}
#else
void radio_isr(void) {
    radio_interrupt_flag = true;
}
#endif

static int radio_init(void) {
    const struct radio_pins *pins = &board->radio_pins;

#if defined(ARCH_ESP32S3) || defined(ARCH_ESP32)
    radio_spi = new SPIClass(HSPI);
    radio_spi->begin(pins->sck, pins->miso, pins->mosi, pins->cs);
#else
    radio_spi = &SPI;
    radio_spi->begin();
#endif

    /* Build radio config from current settings */
    struct radio_config hal_config = {
        .frequency = radio_config.frequency,
        .bandwidth = radio_config.bandwidth,
        .spreading_factor = radio_config.spreading_factor,
        .coding_rate = radio_config.coding_rate,
        .tx_power = radio_config.tx_power,
        .preamble_len = radio_config.preamble_len,
        .use_crc = board->lora_defaults.use_crc,
        .tcxo_voltage = board->lora_defaults.tcxo_voltage,
        .dio2_as_rf_switch = board->lora_defaults.dio2_as_rf_switch,
    };

    /* Initialize radio via HAL - Module creation handled inside */
    if (radio_hal_init(&radio_inst, pins, radio_spi, &hal_config, board->radio) != 0) {
        return -1;
    }

    /* Set up interrupt callback for packet reception */
    radio()->setPacketReceivedAction(radio_isr);

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

    /* Wire.begin() already called early in setup() - like MeshCore does in board.begin() */

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
    Serial.print("I2C scan 0x");
    Serial.print(pins->addr, HEX);
    Serial.print(": ");
    Serial.println(i2c_result == 0 ? "found" : "not found");

    /* Point to global display object (constructed before Wire.begin, like MeshCore) */
    display = &display_128x64;

    /* Initialize display with SWITCHCAPVCC mode
     * Reset already done manually, periphBegin=false since Wire.begin() already called */
    if (!display->begin(SSD1306_SWITCHCAPVCC, pins->addr, false, false)) {
        Serial.println("Display begin() failed!");
        display = nullptr;
        return -1;
    }

    /* Clear and prepare display */
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    display->display();

    Serial.println("Display initialized OK");
    return 0;
}

static void identity_init(void) {
    /* Initialize crypto subsystem */
    crypto_init();

    /* Try to load identity from NVS first */
    Preferences prefs;
    prefs.begin("meshgrid", true);  // Read-only
    bool has_identity = prefs.getBool("has_identity", false);

    if (has_identity) {
        /* Load saved keypair */
        if (prefs.getBytes("pubkey", mesh.pubkey, MESHGRID_PUBKEY_SIZE) == MESHGRID_PUBKEY_SIZE &&
            prefs.getBytes("privkey", mesh.privkey, MESHGRID_PRIVKEY_SIZE) == MESHGRID_PRIVKEY_SIZE) {
            Serial.println("Loaded identity from NVS");
        } else {
            has_identity = false;  // Load failed, regenerate
        }
    }
    prefs.end();

    if (!has_identity) {
        /* Generate new Ed25519 keypair */
        Serial.print("Generating Ed25519 keypair... ");
        if (crypto_generate_keypair(mesh.pubkey, mesh.privkey) == 0) {
            Serial.println("OK");

            /* Save to NVS for persistence */
            prefs.begin("meshgrid", false);  // Read-write
            prefs.putBool("has_identity", true);
            prefs.putBytes("pubkey", mesh.pubkey, MESHGRID_PUBKEY_SIZE);
            prefs.putBytes("privkey", mesh.privkey, MESHGRID_PRIVKEY_SIZE);
            prefs.end();
            Serial.println("Identity saved to NVS");
        } else {
            Serial.println("FAILED!");
        }
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

    if (board->early_init) {
        board->early_init();
    }

    /* Enable VEXT power FIRST for Heltec boards - OLED needs power before I2C init! */
    power_init();

    /* Initialize I2C AFTER power - like MeshCore's board.begin() does */
    const struct display_pins *dpins = &board->display_pins;
    if (dpins->sda >= 0 && dpins->scl >= 0) {
        Wire.begin(dpins->sda, dpins->scl);
        delay(100);  // Give I2C and OLED power time to stabilize
        Serial.print("I2C init: SDA="); Serial.print(dpins->sda);
        Serial.print(" SCL="); Serial.println(dpins->scl);
    }

    boot_time = millis();
    identity_init();
    init_public_channel();  // Initialize MeshCore public channel
    tx_queue_init();        // Initialize packet transmission queue
    config_load();          // Load saved radio config from flash
    neighbors_load_from_nvs();  // Restore neighbors with cached secrets
    channels_load_from_nvs();   // Restore custom channels

    Serial.print("Node: "); Serial.print(mesh.name);
    Serial.print(" ("); Serial.print(mesh.our_hash, HEX); Serial.println(")");
    Serial.print("Mode: "); Serial.println(device_mode == MODE_REPEATER ? "REPEATER" :
                                            device_mode == MODE_ROOM ? "ROOM" : "CLIENT");

    button_init();
    telemetry_init();
    display_init();

    /* Board-specific late initialization (e.g., display contrast) */
    if (board->late_init) board->late_init();

    if (display) {
        display->clearDisplay();
        display->setTextSize(2);
        display->setCursor(10, 20);
        display->println("MESHGRID");
        display->setTextSize(1);
        display->setCursor(20, 45);
        display->println(board->name);
        display->display();
        delay(1500);
    }

    radio_ok = (radio_init() == 0);
    if (!radio_ok) {
        Serial.println("FATAL: Radio init failed");
        Serial.println("Serial CLI still available for debugging");
        // DON'T hang - allow serial CLI to work for debugging
    } else {
        Serial.println("Radio init OK");
        radio()->startReceive();
        send_advertisement(ROUTE_DIRECT);  /* Initial local advertisement */
    }

    Serial.println("\nReady! Type /help for commands.\n");
}

void loop() {
    /* Check for received packet - interrupt-driven approach like MeshCore */
    if (radio_ok && radio_interrupt_flag) {
        radio_interrupt_flag = false;  /* Reset flag */

        uint8_t rx_buf[MESHGRID_MAX_PACKET_SIZE];
        int len = radio()->getPacketLength();

        if (len > 0 && len <= MESHGRID_MAX_PACKET_SIZE) {
            /* Clamp to max valid packet size (header + max path + max payload) */
            if (len > 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE) {
                len = 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE;
            }

            int state = radio()->readData(rx_buf, len);
            if (state == RADIOLIB_ERR_NONE) {
                int16_t rssi = radio()->getRSSI();
                int8_t snr = radio()->getSNR();
                process_packet(rx_buf, len, rssi, snr);
            }
        }
        radio()->startReceive();
    }

    /* Process transmission queue (non-blocking packet forwarding) */
    if (radio_ok) {
        tx_queue_process();
    }

    /* Handle serial commands */
    handle_serial();

    /* Handle button presses */
    button_check();

    /* Periodic advertisement */
    static uint32_t last_advert = 0;
    static uint32_t last_local_advert = 0;

    /* Local advertisement (every 2 minutes) - ROUTE_DIRECT for nearby discovery */
    if (millis() - last_local_advert > MESHGRID_LOCAL_ADVERT_MS) {
        send_advertisement(ROUTE_DIRECT);  /* Zero-hop, neighbors only - logging done in send_advertisement() */
        last_local_advert = millis();
    }

    /* Flood advertisement (every 12 hours) - ROUTE_FLOOD for network-wide presence */
    if (millis() - last_advert > MESHGRID_ADVERT_INTERVAL_MS) {
        send_advertisement(ROUTE_FLOOD);  /* Network-wide - logging done in send_advertisement() */
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

    /* Power saving: Light sleep when on battery and idle (all platforms) */
    static uint32_t sleep_check_time = 0;
    if (millis() - sleep_check_time > 2000) {  /* Check every 2 seconds */
        sleep_check_time = millis();

        /* Only sleep if on battery power (not USB) */
        if (!telemetry.is_usb_power) {
            /* Check if idle (no activity for >2 seconds) */
            uint32_t idle_time = millis() - last_activity_time;
            if (idle_time > 2000) {
                /* Enter light sleep for 100ms */
                /* Radio interrupt will wake us up if packet arrives */
                #if defined(ARDUINO_ARCH_ESP32)
                    /* ESP32: Hardware light sleep */
                    esp_sleep_enable_timer_wakeup(100000);  /* 100ms in microseconds */
                    esp_light_sleep_start();
                #elif defined(ARDUINO_ARCH_NRF52)
                    /* nRF52840: Low-power mode */
                    __WFI();  /* Wait For Interrupt - lowest power until interrupt */
                #elif defined(ARDUINO_ARCH_RP2040)
                    /* RP2040: Sleep briefly */
                    delay(100);  /* RP2040 will enter low-power during delay */
                #endif
            }
        }
    }
}
