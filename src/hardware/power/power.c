/**
 * Power Management HAL - Core Implementation
 */

#include "power.h"
#include "hardware/board.h"
#include <Arduino.h>

/* External board pointer */
extern const struct board_config *board;

int power_init(void) {
    if (board->power_ops && board->power_ops->init) {
        return board->power_ops->init();
    }

    /* Fallback: Legacy simple power init */
    const struct power_pins *pwr = &board->power_pins;

    if (pwr->vext >= 0) {
        pinMode(pwr->vext, OUTPUT);
        digitalWrite(pwr->vext, pwr->vext_active_low ? LOW : HIGH);
        delay(100);
    }

    if (pwr->led >= 0) {
        pinMode(pwr->led, OUTPUT);
        digitalWrite(pwr->led, LOW);
    }

    return 0;
}

int power_enable_rail(enum power_rail rail, bool enable) {
    if (board->power_ops && board->power_ops->enable_rail) {
        return board->power_ops->enable_rail(rail, enable);
    }

    /* No power rail control available */
    return -1;
}

void power_radio_tx_begin(void) {
    if (board->power_ops && board->power_ops->radio_tx_begin) {
        board->power_ops->radio_tx_begin();
    }
}

void power_radio_tx_end(void) {
    if (board->power_ops && board->power_ops->radio_tx_end) {
        board->power_ops->radio_tx_end();
    }
}
