/**
 * meshgrid-firmware - Radio driver abstraction
 *
 * Linux kernel-style ops structure for radio drivers.
 */

#ifndef MESHGRID_RADIO_H
#define MESHGRID_RADIO_H

#include <stdint.h>
#include <stdbool.h>
#include "lib/board.h"

/**
 * Radio state
 */
enum radio_state {
    RADIO_STATE_IDLE,
    RADIO_STATE_RX,
    RADIO_STATE_TX,
    RADIO_STATE_CAD,    // Channel Activity Detection
    RADIO_STATE_SLEEP,
};

/**
 * Received packet info
 */
struct rx_packet {
    uint8_t *data;
    uint8_t len;
    int16_t rssi;
    int8_t snr;
    uint32_t timestamp;
};

/**
 * Radio operations structure
 *
 * Each radio driver (SX1262, SX1276, etc.) implements these ops.
 */
struct radio_ops {
    const char *name;

    // Lifecycle
    int (*init)(const struct radio_pins *pins);
    void (*deinit)(void);

    // Configuration
    int (*set_frequency)(float freq_mhz);
    int (*set_bandwidth)(float bw_khz);
    int (*set_spreading_factor)(uint8_t sf);
    int (*set_coding_rate)(uint8_t cr);
    int (*set_tx_power)(int8_t dbm);
    int (*set_preamble_length)(uint16_t len);
    int (*set_sync_word)(uint16_t sync);
    int (*set_crc)(bool enable);

    // Operations
    int (*transmit)(const uint8_t *data, uint8_t len);
    int (*start_receive)(void);
    int (*receive)(struct rx_packet *pkt, uint32_t timeout_ms);
    int (*standby)(void);
    int (*sleep)(void);

    // Status
    enum radio_state (*get_state)(void);
    int16_t (*get_rssi)(void);
    int8_t (*get_snr)(void);
    bool (*is_channel_free)(void);

    // IRQ handling
    void (*handle_irq)(void);
    void (*set_rx_callback)(void (*cb)(struct rx_packet *pkt));
    void (*set_tx_done_callback)(void (*cb)(void));
};

/**
 * Get radio ops for a specific radio type
 */
const struct radio_ops *radio_get_ops(enum radio_type type);

/**
 * Global radio instance operations
 */
int radio_init(void);
int radio_transmit(const uint8_t *data, uint8_t len);
int radio_start_receive(void);
int radio_receive(struct rx_packet *pkt, uint32_t timeout_ms);

#endif // MESHGRID_RADIO_H
