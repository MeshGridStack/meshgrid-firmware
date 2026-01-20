/**
 * Radio API Wrappers
 *
 * Chip-agnostic wrappers for radio parameter changes.
 * Handles casting to specific radio type internally.
 */

#include "radio_hal.h"
#include <Arduino.h>
#include "../network/protocol.h"

/* Debug output */
#ifdef __cplusplus
extern "C" {
#endif
void debug_printf(int level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#ifdef __cplusplus
}
#endif

/* External instances from main.cpp */
extern struct radio_instance radio_inst;
extern struct meshgrid_state mesh;

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

/* External ISR from main.cpp */
extern void radio_isr(void);

/* C-style wrappers for protocol libraries to avoid vtable/struct layout issues */
extern "C" {
extern volatile bool radio_interrupt_flag;

int16_t radio_transmit(uint8_t* data, size_t len) {
    /* Simple blocking transmit - RadioLib will handle polling */
    int16_t result = get_radio()->transmit(data, len);
    /* Debug: Log actual return value to diagnose board differences */
    if (result != 0) {
        debug_printf(0, "WARN: radio_transmit returned %d (expected 0)", result);
    }
    return result;
}

int16_t radio_start_receive(void) {
    return get_radio()->startReceive();
}

// mesh_increment_tx/rx moved to core/mesh_accessor.c
}
