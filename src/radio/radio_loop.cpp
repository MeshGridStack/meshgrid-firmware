/**
 * Radio receive loop handling
 */

#include "radio_loop.h"
#include "utils/debug.h"
#include <Arduino.h>
#include <RadioLib.h>

extern "C" {
#include "network/protocol.h"
}

#include "radio/radio_hal.h"
#include "core/messaging.h"

/* External state from main.cpp */
extern bool radio_ok;
extern volatile bool radio_interrupt_flag;
extern volatile uint32_t isr_trigger_count;
extern bool radio_in_rx_mode;

void radio_loop_process(void) {
    if (!radio_ok) {
        return;
    }

    /* Handle received packet if interrupt fired */
    if (radio_interrupt_flag) {
        radio_interrupt_flag = false;  /* Reset flag */
        radio_in_rx_mode = false;      /* No longer in RX after interrupt */

        uint8_t rx_buf[MESHGRID_MAX_PACKET_SIZE];
        int len = get_radio()->getPacketLength();

        if (len > 0 && len <= MESHGRID_MAX_PACKET_SIZE) {
            /* Clamp to max valid packet size (header + max path + max payload) */
            if (len > 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE) {
                len = 1 + 4 + 1 + MESHGRID_MAX_PATH_SIZE + MESHGRID_MAX_PAYLOAD_SIZE;
            }

            int state = get_radio()->readData(rx_buf, len);
            if (state == RADIOLIB_ERR_NONE) {
                int16_t rssi = get_radio()->getRSSI();
                int8_t snr = get_radio()->getSNR();
                process_packet(rx_buf, len, rssi, snr);
            }
        }
    }

    /* Ensure radio is in RX mode - only call if not already in RX (MeshCore pattern) */
    if (!radio_in_rx_mode) {
        int state = get_radio()->startReceive();
        if (state == RADIOLIB_ERR_NONE) {
            radio_in_rx_mode = true;
            static uint32_t last_ok_log = 0;
            if (millis() - last_ok_log > 5000) {  /* Log success periodically */
                DEBUG_INFOF("[RX] In RX mode, ISR count=%lu", isr_trigger_count);
                last_ok_log = millis();
            }
        } else {
            static uint32_t last_error_log = 0;
            if (millis() - last_error_log > 1000) {  /* Rate limit error logging */
                DEBUG_ERRORF("[RX] startReceive() failed: %d", state);
                last_error_log = millis();
            }
        }
    }
}
