/**
 * Info command implementations
 */

#include "info_commands.h"
#include "common.h"
#include "core/neighbors.h"
#include "hardware/board.h"
#include "utils/constants.h"
#include "version.h"
#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)
#    include <Preferences.h>
#endif

extern "C" {
#include "hardware/telemetry/telemetry.h"
}

extern struct meshgrid_state mesh;
extern enum meshgrid_device_mode device_mode;
extern struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
} radio_config;

extern const struct board_config* board;
extern struct telemetry_data telemetry; /* Defined in hardware/telemetry/telemetry.h */
extern volatile uint32_t isr_trigger_count;
extern uint32_t stat_duplicates;
extern uint32_t stat_clients, stat_repeaters, stat_rooms;
extern uint32_t get_uptime_secs(void);

void cmd_info() {
    response_print("{\"name\":\"");
    response_print(mesh.name);
    response_print("\",\"node_hash\":");
    response_print(mesh.our_hash);
    response_print(",\"public_key\":[");
    for (int i = 0; i < MESHGRID_PUBKEY_SIZE; i++) {
        if (i > 0)
            response_print(",");
        response_print(mesh.pubkey[i]);
    }
    response_print("],\"firmware_version\":\"");
    response_print(MESHGRID_VERSION);
    response_print("\",\"mode\":\"");
    response_print(device_mode == MODE_REPEATER ? "REPEATER" : "CLIENT");
    response_print("\",\"freq_mhz\":");
    response_print(radio_config.frequency, 2);
    response_print(",\"tx_power_dbm\":");
    response_print(radio_config.tx_power);
    response_println("}");
}

void cmd_telemetry() {
    response_print("{\"device\":{\"battery\":");
    response_print(telemetry.battery_pct);
    response_print(",\"voltage\":");
    response_print(telemetry.battery_mv / 1000.0, 3);
    response_print(",\"charging\":");
    response_print(telemetry.is_charging ? "true" : "false");
    response_print(",\"usb\":");
    response_print(telemetry.is_usb_power ? "true" : "false");
    response_print(",\"uptime\":");
    response_print(get_uptime_secs());
    response_print(",\"heap\":");
    response_print(telemetry.free_heap);
    if (telemetry.has_temp) {
        response_print(",\"cpu_temp\":");
        response_print(telemetry.temp_deci_c / 10.0, 1);
    }
    response_println("}}");
}

void cmd_stats() {
    response_print("{");
    response_print("\"hardware\":{");
    response_print("\"board\":\"");
    response_print(board->vendor);
    response_print(" ");
    response_print(board->name);
    response_print("\",");
    response_print("\"chip\":\"ESP32-S3\",");
    response_print("\"cpu_mhz\":240,");
    response_print("\"cores\":2");
    response_print("},");
    response_print("\"memory\":{");
    response_print("\"ram_total_kb\":320,");
    response_print("\"ram_used_kb\":32,");
    response_print("\"ram_free_kb\":288,");
    response_print("\"heap_free_kb\":");
    response_print(telemetry.free_heap / 1024);
    response_print(",");
    response_print("\"flash_total_kb\":3264,");
    response_print("\"flash_used_kb\":481");
    response_print("},");
    response_print("\"packets\":{");
    response_print("\"rx\":");
    response_print(mesh.packets_rx);
    response_print(",");
    response_print("\"tx\":");
    response_print(mesh.packets_tx);
    response_print(",");
    response_print("\"fwd\":");
    response_print(mesh.packets_fwd);
    response_print(",");
    response_print("\"dropped\":");
    response_print(mesh.packets_dropped);
    response_print(",");
    response_print("\"duplicates\":");
    response_print(stat_duplicates);
    response_print("},");
    response_print("\"neighbors\":{");
    response_print("\"total\":");
    response_print(neighbor_count);
    response_print(",");
    response_print("\"clients\":");
    response_print(stat_clients);
    response_print(",");
    response_print("\"repeaters\":");
    response_print(stat_repeaters);
    response_print(",");
    response_print("\"rooms\":");
    response_print(stat_rooms);
    response_print("},");
    response_print("\"radio\":{");
    response_print("\"freq_mhz\":");
    response_print(radio_config.frequency, 2);
    response_print(",");
    response_print("\"bandwidth_khz\":");
    response_print(radio_config.bandwidth, 1);
    response_print(",");
    response_print("\"spreading_factor\":");
    response_print(radio_config.spreading_factor);
    response_print(",");
    response_print("\"coding_rate\":");
    response_print(radio_config.coding_rate);
    response_print(",");
    response_print("\"tx_power_dbm\":");
    response_print(radio_config.tx_power);
    response_print(",");
    response_print("\"tcxo_voltage\":");
    response_print(board->lora_defaults.tcxo_voltage, 1);
    response_print(",");
    response_print("\"dio2_as_rf_switch\":");
    response_print(board->lora_defaults.dio2_as_rf_switch ? "true" : "false");
    response_print(",");
    response_print("\"isr_count\":");
    response_print(isr_trigger_count);
    response_print("},");
    response_print("\"power\":{");
    response_print("\"battery_mv\":");
    response_print(telemetry.battery_mv);
    response_print(",");
    response_print("\"battery_pct\":");
    response_print(telemetry.battery_pct);
    response_print(",");
    response_print("\"charging\":");
    response_print(telemetry.is_charging ? "true" : "false");
    response_print(",");
    response_print("\"usb_power\":");
    response_print(telemetry.is_usb_power ? "true" : "false");
    response_print(",");
    response_print("\"sleep_enabled\":");
    response_print(!telemetry.is_usb_power ? "true" : "false");
    response_print("},");
    response_print("\"features\":{");
#if defined(ARDUINO_ARCH_ESP32)
    response_print("\"hw_aes\":true,");
    response_print("\"hw_sha256\":true,");
#elif defined(ARDUINO_ARCH_NRF52)
    response_print("\"hw_aes\":true,");
    response_print("\"hw_sha256\":false,");
#else
    response_print("\"hw_aes\":false,");
    response_print("\"hw_sha256\":false,");
#endif
    response_print("\"tx_queue_size\":16,");
    response_print("\"priority_scheduling\":true,");
    response_print("\"airtime_budget\":true,");
    response_print("\"secret_caching\":true");
    response_print("},");
    response_print("\"firmware\":{");
    response_print("\"version\":\"");
    response_print(MESHGRID_VERSION);
    response_print("\",");
    response_print("\"build\":\"");
    response_print(MESHGRID_BUILD_DATE);
    response_print("\",");
    response_print("\"uptime_secs\":");
    response_print(get_uptime_secs());
    response_print(",");
    response_print("\"mode\":\"");
    response_print(device_mode == MODE_REPEATER ? "REPEATER" : "CLIENT");
    response_print("\"");
    response_print("},");
    response_print("\"temperature\":{");
    if (telemetry.has_temp) {
        response_print("\"cpu_c\":");
        response_print(telemetry.temp_deci_c / 10.0, 1);
    } else {
        response_print("\"cpu_c\":null");
    }
    response_print("}");
    response_println("}");
}

void cmd_config() {
    response_print("{\"name\":\"");
    response_print(mesh.name);
    response_print("\",\"freq_mhz\":");
    response_print(radio_config.frequency, 2);
    response_print(",\"tx_power_dbm\":");
    response_print(radio_config.tx_power);
    response_print(",\"bandwidth_khz\":");
    response_print((int)(radio_config.bandwidth));
    response_print(",\"spreading_factor\":");
    response_print(radio_config.spreading_factor);
    response_print(",\"coding_rate\":");
    response_print(radio_config.coding_rate);
    response_print(",\"preamble_len\":");
    response_print(radio_config.preamble_len);
    response_println("}");
}
