/**
 * ============================================================================
 * MESHGRID FIRMWARE - Main Entry Point
 * ============================================================================
 *
 * MeshCore-compatible LoRa mesh networking firmware
 *
 * Device Modes:
 *   - CLIENT:   Full-featured node (send/receive messages)
 *   - REPEATER: Packet forwarding only (low power)
 *
 * Optional Features (compile-time):
 *   - BLE:  Bluetooth UART for wireless serial access
 *   - MQTT: Bridge mesh messages to MQTT broker
 *
 * ============================================================================
 */

/* ===== Standard Libraries ===== */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <mbedtls/base64.h>

/* ===== Third-Party Libraries ===== */
#include <RadioLib.h>
#include <Adafruit_GFX.h>
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>

/* ===== Utilities ===== */
#include "utils/constants.h"
#include "utils/memory.h"
#include "utils/types.h"
#include "utils/ui_lib.h"
#include "utils/serial_output.h"
#include "version.h"

/* ===== Hardware Abstraction ===== */
#include "hardware/board.h"
#include "hardware/boards/boards.h"
#ifdef ENABLE_BLE
#include "hardware/bluetooth/ble_serial.h"
#include "hardware/bluetooth/SerialBridge.h"
#else
#include "hardware/bluetooth/serial_bridge.h"
#endif
extern "C" {
#include "hardware/crypto/crypto.h"
#include "hardware/power/power.h"
#include "hardware/telemetry/telemetry.h"
#include "hardware/test/hw_test.h"
}

/* ===== Radio Subsystem ===== */
#include "radio/radio_hal.h"
#include "radio/radio_loop.h"

/* ===== Network Protocol ===== */
extern "C" {
#include "network/protocol.h"
}

/* ===== Core Functionality ===== */
#include "core/identity.h"
#include "core/config.h"
#include "core/security.h"
#include "core/neighbors.h"
#include "core/channels.h"
#include "core/messaging.h"
#include "core/advertising.h"
#include "core/power.h"
#include "core/commands.h"

/* ===== User Interface ===== */
#include "ui/screens.h"
#include "ui/button.h"

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
 * Use -1 for reset pin to avoid crashes, handle reset manually in display_init()
 * Non-static so it can be accessed by ui/screens.cpp */
Adafruit_SSD1306 display_128x64(128, 64, &Wire, -1);
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
 * Display state
 */
struct display_state display_state;
uint32_t last_activity_time = 0;

/*
 * Button configuration
 */
static struct button_config btn_config;

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

/*
 * Telemetry
 */
struct telemetry_data telemetry;
static uint32_t last_telemetry_read = 0;

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
/* Button callbacks                                                          */
/* ========================================================================= */

static void on_button_short_press(void) {
    /* Short press: next screen or scroll down */
    display_scroll_down(&display_state);
    led_blink();
}

static void on_button_long_press(void) {
    /* Long press: scroll up or send advertisement */
    if (display_state.current_screen == SCREEN_NEIGHBORS ||
        display_state.current_screen == SCREEN_MESSAGES) {
        display_scroll_up(&display_state);
    } else {
        /* Send local advertisement on button press */
        send_advertisement(ROUTE_DIRECT);
    }
    led_blink();
}

static void button_setup(void) {
    btn_config.pin = board->power_pins.button;
    btn_config.debounce_ms = BUTTON_DEBOUNCE_MS;
    btn_config.long_press_ms = BUTTON_LONG_PRESS_MS;
    btn_config.on_short_press = on_button_short_press;
    btn_config.on_long_press = on_button_long_press;
    button_init(&btn_config);
}

/* ========================================================================= */
/* Radio interrupt handling                                                  */
/* ========================================================================= */

/* Radio state (accessible by radio_loop.cpp) */
volatile bool radio_interrupt_flag = false;

volatile uint32_t isr_trigger_count = 0;
bool radio_in_rx_mode = false;

#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3)
void ICACHE_RAM_ATTR radio_isr(void) {
    radio_interrupt_flag = true;
    isr_trigger_count++;
    /* NOTE: Can't use Serial.print in ISR on ESP32 - causes crashes */
}
#else
void radio_isr(void) {
    radio_interrupt_flag = true;
    isr_trigger_count++;
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

    /* Set up interrupt callback for packet reception (RadioLib handles DIO1 GPIO config) */
    radio()->setPacketReceivedAction(radio_isr);

    return 0;
}

/* ========================================================================= */
/* Main                                                                      */
/* ========================================================================= */

bool radio_ok = true;

void setup() {
    Serial.begin(115200);
    // ESP32-S3 USB CDC takes time to become active
    // Wait for USB to be ready before sending any output
    delay(2000);

    Serial.println("\n=================================");
    Serial.println("  MESHGRID - MeshCore Compatible");
    SerialOutput.print("  Firmware v"); Serial.println(MESHGRID_VERSION);
    SerialOutput.print("  Build: "); Serial.println(MESHGRID_BUILD_DATE);
    Serial.println("=================================\n");

    board = &CURRENT_BOARD_CONFIG;
    SerialOutput.print("Board: "); SerialOutput.print(board->vendor); SerialOutput.print(" "); Serial.println(board->name);

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
        SerialOutput.print("I2C init: SDA="); SerialOutput.print(dpins->sda);
        SerialOutput.print(" SCL="); Serial.println(dpins->scl);
    }

    boot_time = millis();
    identity_init();
    init_public_channel();  // Initialize MeshCore public channel
    tx_queue_init();        // Initialize packet transmission queue
    config_load();          // Load saved radio config from flash
    security_init();        // Initialize PIN authentication
    neighbors_load_from_nvs();  // Restore neighbors with cached secrets
    channels_load_from_nvs();   // Restore custom channels

    SerialOutput.print("Node: "); SerialOutput.print(mesh.name);
    SerialOutput.print(" ("); SerialOutput.print(mesh.our_hash, HEX); Serial.println(")");
    SerialOutput.print("Mode: "); Serial.println(device_mode == MODE_REPEATER ? "REPEATER" : "CLIENT");

    button_setup();
    telemetry_init();
    display_init(&display);
    display_state_init(&display_state);

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

#ifdef ENABLE_BLE
    /* Initialize BLE UART service for wireless serial access */
    char ble_name[32];
    snprintf(ble_name, sizeof(ble_name), "meshgrid-%02X", mesh.our_hash);
    if (ble_serial_init(ble_name) == 0) {
        SerialOutput.print("BLE UART service: ");
        Serial.println(ble_name);
    }
#endif

    Serial.println("\nReady! Type /help for commands.\n");
#ifdef ENABLE_BLE
    Serial.println("Connect via USB Serial or Bluetooth (BLE UART)");
#else
    Serial.println("Connect via USB Serial");
#endif
}

void loop() {
    /* Radio RX handling */
    radio_loop_process();

    /* Process transmission queue */
    if (radio_ok) {
        tx_queue_process();
    }

    /* Debug: Print ISR trigger count every 10 seconds */
    static uint32_t last_isr_debug = 0;
    if (millis() - last_isr_debug > 10000) {
        SerialOutput.print("ISR count: ");
        Serial.println(isr_trigger_count);
        last_isr_debug = millis();
    }

    /* Handle serial commands (USB + BLE) */
    handle_serial();

#ifdef ENABLE_BLE
    /* Process BLE events */
    ble_serial_process();
#endif

    /* Handle button presses */
    button_check();

    /* Periodic advertisements */
    advertising_process();

    /* Read telemetry periodically */
    if (millis() - last_telemetry_read > TELEMETRY_READ_INTERVAL_MS) {
        telemetry_read(&telemetry);
        last_telemetry_read = millis();
        display_state.dirty = true;
    }

    /* Update display every 500ms */
    static uint32_t last_display = 0;
    if (millis() - last_display > 500) {
        display_update(display, &display_state);
        last_display = millis();
    }

    /* Power management */
    power_check_sleep();
}
