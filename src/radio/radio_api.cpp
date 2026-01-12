/**
 * Radio API Wrappers
 *
 * Chip-agnostic wrappers for radio parameter changes.
 * Handles casting to specific radio type internally.
 */

#include "radio_hal.h"
#include <Arduino.h>

/* External radio instance from main.cpp */
extern struct radio_instance radio_inst;

int radio_set_frequency(float freq) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setFrequency(freq);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setFrequency(freq);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

int radio_set_bandwidth(float bw) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setBandwidth(bw);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setBandwidth(bw);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

int radio_set_spreading_factor(uint8_t sf) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setSpreadingFactor(sf);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setSpreadingFactor(sf);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

int radio_set_coding_rate(uint8_t cr) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setCodingRate(cr);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setCodingRate(cr);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

int radio_set_output_power(int8_t power) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setOutputPower(power);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setOutputPower(power);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

int radio_set_preamble_length(uint16_t len) {
    switch (radio_inst.type) {
        case RADIO_SX1262:
        case RADIO_SX1268:
            return radio_inst.sx1262->setPreambleLength(len);
        case RADIO_SX1276:
        case RADIO_SX1278:
            return radio_inst.sx1276->setPreambleLength(len);
        default:
            return RADIOLIB_ERR_UNKNOWN;
    }
}

PhysicalLayer* get_radio() {
    extern struct radio_instance radio_inst;
    return radio_inst.as_phy();
}
