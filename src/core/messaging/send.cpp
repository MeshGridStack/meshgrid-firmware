/**
 * Message sending functions
 * Uses protocol libraries for v0/v1/auto protocol selection
 */

#include "send.h"
#include "utils.h"
#include "utils/types.h"
#include "utils/serial_output.h"
#include "utils/debug.h"
#include "network/protocol.h"
#include "radio/radio_hal.h"

// Use C bridge to avoid namespace conflict
extern "C" {
#include "core/meshcore_bridge.h"
#include "core/mesh_accessor.h"
int16_t radio_transmit(uint8_t* data, size_t len);
int16_t radio_start_receive(void);
}

extern "C" {
#include "hardware/crypto/crypto.h"
}

/* Externs from main.cpp */
extern struct meshgrid_state mesh;
extern uint32_t advert_interval_ms;
extern uint8_t public_channel_hash;
extern uint8_t public_channel_secret[32];
extern bool radio_in_rx_mode;

/* Functions from main.cpp */
extern void led_blink(void);

/*
 * Broadcast advertisement packet
 * Uses MeshCore v0 for advertisements
 */
void send_advertisement(uint8_t route_type) {
    uint32_t tx_before = mesh.packets_tx;
    DEBUG_INFOF("send_advertisement() START route_type=%d tx_before=%lu", route_type, tx_before);
    meshcore_bridge_send_advert();

    // Process MeshCore loop several times to ensure packet is transmitted
    for (int i = 0; i < 20; i++) {
        meshcore_bridge_loop();
        delay(5);
        if (mesh.packets_tx > tx_before) {
            break; // Transmission completed
        }
    }

    uint32_t tx_after = mesh.packets_tx;
    DEBUG_INFOF("send_advertisement() END tx_after=%lu", tx_after);
}

/*
 * Send advertisement directly (for testing)
 * Bypasses MeshCore queue system
 */
void send_advert_direct(void) {
    extern bool radio_in_rx_mode;

    // Create a simple advertisement packet
    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    // Simple packet format for testing
    packet[0] = 0xAD;             // Advertisement marker
    packet[1] = mesh.privkey[31]; // Node hash (last byte of privkey)
    const char* name = mesh_get_name();
    strncpy((char*)&packet[2], name ? name : "test", 16);

    // Transmit
    int16_t result = radio_transmit(packet, 32);
    if (result == 0) {
        mesh.packets_tx++;
    }

    // Return to RX mode
    if (!radio_in_rx_mode) {
        radio_start_receive();
        radio_in_rx_mode = true;
    }
}

/*
 * Send encrypted group message to public channel
 * Uses MeshCore v0 (MeshCore compatible)
 */
void send_group_message(const char* text) {
    /* Use v0 protocol for maximum compatibility */
    meshcore_bridge_send_channel(public_channel_hash, public_channel_secret, text, "Public");
}
