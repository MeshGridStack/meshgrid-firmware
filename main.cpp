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
#include <Preferences.h>
#include <mbedtls/base64.h>

#include "lib/board.h"
#include "version.h"

extern "C" {
#include "net/protocol.h"
#include "drivers/telemetry/telemetry.h"
#include "drivers/test/hw_test.h"
#include "drivers/crypto/crypto.h"
}

#include "boards/boards.h"

/* Application modules */
#include "app/neighbors.h"
#include "app/messaging.h"
#include "app/config.h"
#include "app/commands.h"

/*
 * MeshCore Public Channel (for group messaging support)
 * PSK: Base64-encoded pre-shared key for the default "Public" group channel
 * Compatible with MeshCore's standard public channel
 */
#define PUBLIC_CHANNEL_PSK "izOH6cXN6mrJ5e26oRXNcg=="
uint8_t public_channel_secret[32];  /* Decoded PSK - defined here, used in config.cpp */
uint8_t public_channel_hash = 0;     /* Hash of the public channel */

/*
 * Configuration - set via serial commands or stored in flash
 */
enum meshgrid_device_mode device_mode = MODE_CLIENT;

/*
 * Board and hardware
 */
const struct board_config *board;
SX1262 *radio = nullptr;  /* Non-static for hw_test access */
static SPIClass *radio_spi = nullptr;
static U8G2 *display = nullptr;

/*
 * Mesh state
 */
struct meshgrid_state mesh;
uint32_t boot_time;

/*
 * RTC time tracking (set via /time command)
 */
struct rtc_time_t {
    bool valid;
    uint32_t epoch_at_boot;  /* Unix timestamp when device booted */
};
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
 */
#define SEEN_TABLE_SIZE 64
struct seen_entry {
    uint8_t hash;
    uint32_t time;
} seen_table[SEEN_TABLE_SIZE];
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
    SCREEN_STATS = 2,       /* Detailed statistics */
    SCREEN_INFO = 3,        /* Device info */
    SCREEN_RADIO = 4,       /* Radio configuration */
    SCREEN_COUNT = 5
};

static enum display_screen current_screen = SCREEN_STATUS;
bool display_dirty = true;
static uint8_t neighbor_scroll = 0;  /* Scroll offset for neighbor list */
uint32_t last_activity_time = 0;

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
#define LOG_BUFFER_SIZE 50
String log_buffer[LOG_BUFFER_SIZE];
int log_index = 0;
int log_count = 0;
bool log_enabled = false;

// log_event() moved to app/messaging.cpp

/*
 * Telemetry
 */
struct telemetry_data telemetry;
static uint32_t last_telemetry_read = 0;
#define TELEMETRY_READ_INTERVAL_MS 5000

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

void led_blink(void) {
    if (board->power_pins.led < 0) return;
    digitalWrite(board->power_pins.led, HIGH);
    delay(30);
    digitalWrite(board->power_pins.led, LOW);
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
    display->drawStr(70, 64, "1/5");
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
    display->drawStr(58, 64, "2/5");
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
    display->drawStr(70, 64, "3/5");
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
    display->drawStr(70, 64, "4/5");
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

    /* Line 2: Preset and frequency */
    snprintf(line, sizeof(line), "Preset: %s (%.3f MHz)", preset, radio_config.frequency);
    display->drawStr(0, 22, line);

    /* Line 3: TX power */
    snprintf(line, sizeof(line), "Power: %d dBm", radio_config.tx_power);
    display->drawStr(0, 33, line);

    /* Line 4: Bandwidth and spreading factor */
    snprintf(line, sizeof(line), "BW: %.1f kHz  SF: %d",
             radio_config.bandwidth, radio_config.spreading_factor);
    display->drawStr(0, 44, line);

    /* Line 5: Coding rate and preamble */
    snprintf(line, sizeof(line), "CR: 4/%d  Pre: %d",
             radio_config.coding_rate, radio_config.preamble_len);
    display->drawStr(0, 55, line);

    display->drawStr(0, 64, "[BTN:next]");
    display->drawStr(70, 64, "5/5");
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
        case SCREEN_RADIO:
            draw_screen_radio();
            break;
        default:
            break;
    }

    display->sendBuffer();
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

    Module *mod = new Module(pins->cs, pins->dio1, pins->reset, pins->busy, *radio_spi);
    radio = new SX1262(mod);

    Serial.print("Radio init... ");
    int state = radio->begin(radio_config.frequency, radio_config.bandwidth, radio_config.spreading_factor,
                             radio_config.coding_rate, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                             radio_config.tx_power, radio_config.preamble_len);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("FAILED: ");
        Serial.println(state);
        return -1;
    }

    if (board->lora_defaults.use_crc) radio->setCRC(1);  /* MeshCore uses CRC mode 1 */
    radio->explicitHeader();  /* MeshCore uses explicit header mode */

    /* Set up interrupt callback - CRITICAL for packet reception */
    radio->setPacketReceivedAction(radio_isr);

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
    init_public_channel();  // Initialize MeshCore public channel
    tx_queue_init();        // Initialize packet transmission queue
    config_load();  // Load saved radio config from flash

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
        send_advertisement(ROUTE_DIRECT);  /* Initial local advertisement */
    }

    Serial.println("\nReady! Type /help for commands.\n");
}

void loop() {
    /* Check for received packet - interrupt-driven approach like MeshCore */
    if (radio_ok && radio_interrupt_flag) {
        radio_interrupt_flag = false;  /* Reset flag */

        uint8_t rx_buf[MESHGRID_MAX_PACKET_SIZE];
        int len = radio->getPacketLength();

        if (len > 0 && len <= MESHGRID_MAX_PACKET_SIZE) {
            /* Clamp to max valid packet size (header + max path + max payload) */
            if (len > 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE) {
                len = 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE;
            }

            int state = radio->readData(rx_buf, len);
            if (state == RADIOLIB_ERR_NONE) {
                int16_t rssi = radio->getRSSI();
                int8_t snr = radio->getSNR();
                process_packet(rx_buf, len, rssi, snr);
            }
        }
        radio->startReceive();
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
