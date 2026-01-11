/**
 * Radio HAL - Generic radio chip abstraction
 *
 * Supports multiple LoRa chip families with unified interface
 */

#ifndef MESHGRID_RADIO_HAL_H
#define MESHGRID_RADIO_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "lib/board.h"

#ifdef __cplusplus

#include <RadioLib.h>

/**
 * Radio configuration structure
 */
struct radio_config {
    float frequency;
    float bandwidth;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    int8_t tx_power;
    uint16_t preamble_len;
    bool use_crc;
    float tcxo_voltage;
    bool dio2_as_rf_switch;
};

/**
 * Radio union - holds one radio type
 */
struct radio_instance {
    enum radio_type type;
    union {
        SX1262 *sx1262;
        SX1276 *sx1276;
        SX1280 *sx1280;
        LR1110 *lr1110;
    };

    /* Get as PhysicalLayer for common operations */
    PhysicalLayer* as_phy() {
        switch (type) {
            case RADIO_SX1262:
            case RADIO_SX1268:
                return (PhysicalLayer*)sx1262;
            case RADIO_SX1276:
            case RADIO_SX1278:
                return (PhysicalLayer*)sx1276;
            case RADIO_SX1280:
                return (PhysicalLayer*)sx1280;
            case RADIO_LR1110:
            case RADIO_LR1120:
            case RADIO_LR1121:
                return (PhysicalLayer*)lr1110;
            default:
                return nullptr;
        }
    }
};

/**
 * Initialize radio based on board configuration
 * Returns: 0 on success, RadioLib error code on failure
 */
int radio_hal_init(struct radio_instance *radio, Module *mod, const struct radio_config *config, enum radio_type type);

/**
 * Radio parameter setters - chip-agnostic wrappers
 * Implemented in radio_api.cpp
 */
PhysicalLayer* get_radio();
int radio_set_frequency(float freq);
int radio_set_bandwidth(float bw);
int radio_set_spreading_factor(uint8_t sf);
int radio_set_coding_rate(uint8_t cr);
int radio_set_output_power(int8_t power);
int radio_set_preamble_length(uint16_t len);

#endif /* __cplusplus */

#endif /* MESHGRID_RADIO_HAL_H */
