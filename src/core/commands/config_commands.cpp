/**
 * Config command implementations
 */

#include "config_commands.h"
#include "common.h"
#include "radio/radio_hal.h"
#include "utils/types.h"
#include <Preferences.h>
#include <Esp.h>

extern "C" {
#include "network/protocol.h"
}

extern struct meshgrid_state mesh;
extern struct radio_config_t {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint16_t preamble_len;
    int8_t tx_power;
    bool config_saved;
} radio_config;

extern Preferences prefs;
extern void config_save(void);

void cmd_config_save() {
    config_save();
    response_println("OK Config saved to flash");
}

void cmd_config_reset() {
    prefs.begin("meshgrid", false);
    prefs.clear();
    prefs.end();
    response_println("OK Config cleared, rebooting...");
    delay(100);
    ESP.restart();
}

void cmd_set_name(const String &name) {
    if (name.length() > 0 && name.length() <= 16) {
        strncpy(mesh.name, name.c_str(), 16);
        mesh.name[16] = '\0';
        config_save();
        response_println("OK");
    } else {
        response_println("ERR Name must be 1-16 characters");
    }
}

void cmd_set_freq(float freq) {
    if (freq >= 137.0 && freq <= 1020.0) {
        radio_set_frequency(freq);
        radio_config.frequency = freq;
        config_save();
        response_println("OK");
    } else {
        response_println("ERR Invalid frequency");
    }
}

void cmd_set_bw(float bw) {
    if (radio_set_bandwidth(bw) == RADIOLIB_ERR_NONE) {
        radio_config.bandwidth = bw;
        config_save();
        response_println("OK");
    } else {
        response_println("ERR Invalid bandwidth");
    }
}

void cmd_set_sf(int sf) {
    if (sf >= 6 && sf <= 12) {
        if (radio_set_spreading_factor(sf) == RADIOLIB_ERR_NONE) {
            radio_config.spreading_factor = sf;
            config_save();
            response_println("OK");
        } else {
            response_println("ERR Failed to set SF");
        }
    } else {
        response_println("ERR SF must be 6-12");
    }
}

void cmd_set_cr(int cr) {
    if (cr >= 5 && cr <= 8) {
        if (radio_set_coding_rate(cr) == RADIOLIB_ERR_NONE) {
            radio_config.coding_rate = cr;
            config_save();
            response_println("OK");
        } else {
            response_println("ERR Failed to set CR");
        }
    } else {
        response_println("ERR CR must be 5-8");
    }
}

void cmd_set_power(int power) {
    if (power >= -9 && power <= 22) {
        if (radio_set_output_power(power) == RADIOLIB_ERR_NONE) {
            radio_config.tx_power = power;
            config_save();
            response_println("OK");
        } else {
            response_println("ERR Failed to set power");
        }
    } else {
        response_println("ERR Power must be -9 to 22 dBm");
    }
}

void cmd_set_preamble(int preamble) {
    if (preamble >= 6 && preamble <= 65535) {
        if (radio_set_preamble_length(preamble) == RADIOLIB_ERR_NONE) {
            radio_config.preamble_len = preamble;
            config_save();
            response_println("OK");
        } else {
            response_println("ERR Failed to set preamble");
        }
    } else {
        response_println("ERR Preamble must be 6-65535");
    }
}

void cmd_set_preset(const String &preset) {
    if (preset == "EU_NARROW" || preset == "EU") {
        radio_set_frequency(869.618);
        radio_set_bandwidth(62.5);
        radio_set_spreading_factor(8);
        radio_set_coding_rate(8);
        radio_set_preamble_length(16);
        radio_config.frequency = 869.618;
        radio_config.bandwidth = 62.5;
        radio_config.spreading_factor = 8;
        radio_config.coding_rate = 8;
        radio_config.preamble_len = 16;
        config_save();
        response_println("OK EU/UK Narrow: 869.618MHz 62.5kHz SF8 CR8 Pre16");
    } else if (preset == "US_STANDARD" || preset == "US") {
        radio_set_frequency(915.0);
        radio_set_bandwidth(250.0);
        radio_set_spreading_factor(10);
        radio_set_coding_rate(7);
        radio_set_preamble_length(16);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 250.0;
        radio_config.spreading_factor = 10;
        radio_config.coding_rate = 7;
        radio_config.preamble_len = 16;
        config_save();
        response_println("OK US Standard: 915MHz 250kHz SF10 CR7");
    } else if (preset == "US_FAST") {
        radio_set_frequency(915.0);
        radio_set_bandwidth(500.0);
        radio_set_spreading_factor(7);
        radio_set_coding_rate(5);
        radio_set_preamble_length(8);
        radio_config.frequency = 915.0;
        radio_config.bandwidth = 500.0;
        radio_config.spreading_factor = 7;
        radio_config.coding_rate = 5;
        radio_config.preamble_len = 8;
        config_save();
        response_println("OK US Fast: 915MHz 500kHz SF7 CR5");
    } else if (preset == "LONG_RANGE") {
        radio_set_bandwidth(125.0);
        radio_set_spreading_factor(12);
        radio_set_coding_rate(8);
        radio_set_preamble_length(16);
        radio_config.bandwidth = 125.0;
        radio_config.spreading_factor = 12;
        radio_config.coding_rate = 8;
        radio_config.preamble_len = 16;
        config_save();
        response_println("OK Long Range: 125kHz SF12 CR8");
    } else {
        response_println("ERR Unknown preset");
    }
}
