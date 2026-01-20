/**
 * Mesh messaging - packet receive handlers
 * Protocol-specific handlers are in protocol libraries
 */

#include "receive.h"
#include "utils.h"
#include "../neighbors.h"
#include "utils/memory.h"
#include "utils/types.h"
#include "utils/serial_output.h"
#include "ui/screens.h"
#include <Arduino.h>

/* Protocol-auto advertisement handler */
#include <advert_auto.h>

extern "C" {
#include "hardware/crypto/crypto.h"
}

/* Externs from main.cpp */
extern struct meshgrid_state mesh;
extern struct display_state display_state;

/*
 * Handle received advertisement packet
 * Dispatches to protocol-specific handlers (v0 or v1)
 */
void handle_advert(struct meshgrid_packet* pkt, int16_t rssi, int8_t snr) {
    /* Dispatch to protocol-auto handler */
    advert_auto_receive(pkt, rssi, snr);

    /* Mark display as dirty for UI update */
    display_state.dirty = true;
}
