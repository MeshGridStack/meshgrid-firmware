/**
 * Heltec V4 Power Amplifier Control
 *
 * Heltec V4 has a power amplifier that requires special GPIO control.
 * Also handles VEXT for peripheral power.
 */

#include "power.h"
#include "lib/board.h"
#include <Arduino.h>

/* Heltec V4 PA control pins */
#define PIN_PA_POWER    7   /* Must be HIGH to enable PA power */
#define PIN_PA_EN       2   /* Must be HIGH to enable PA */
#define PIN_PA_TX_EN    46  /* HIGH during TX, LOW during RX */
#define PIN_ADC_CTRL    37  /* ADC control (LOW = inactive) */

extern const struct board_config *board;

static int heltec_v4_init(void) {
    /* Initialize PA control pins */
    pinMode(PIN_PA_POWER, OUTPUT);
    digitalWrite(PIN_PA_POWER, HIGH);  /* Enable PA power supply */

    pinMode(PIN_PA_EN, OUTPUT);
    digitalWrite(PIN_PA_EN, HIGH);     /* Enable PA */

    pinMode(PIN_PA_TX_EN, OUTPUT);
    digitalWrite(PIN_PA_TX_EN, LOW);   /* Keep TX disabled initially */

    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW);   /* Keep ADC inactive */

    /* Handle VEXT if present */
    const struct power_pins *pwr = &board->power_pins;
    if (pwr->vext >= 0) {
        pinMode(pwr->vext, OUTPUT);
        digitalWrite(pwr->vext, pwr->vext_active_low ? LOW : HIGH);
        delay(100);
    }

    /* Handle LED if present */
    if (pwr->led >= 0) {
        pinMode(pwr->led, OUTPUT);
        digitalWrite(pwr->led, LOW);
    }

    delay(100);  /* Give PA time to stabilize */

    return 0;
}

static void heltec_v4_tx_begin(void) {
    digitalWrite(PIN_PA_TX_EN, HIGH);  /* Enable TX mode */
    if (board->power_pins.led >= 0) {
        digitalWrite(board->power_pins.led, HIGH);  /* TX LED on */
    }
}

static void heltec_v4_tx_end(void) {
    digitalWrite(PIN_PA_TX_EN, LOW);   /* Disable TX mode */
    if (board->power_pins.led >= 0) {
        digitalWrite(board->power_pins.led, LOW);   /* TX LED off */
    }
}

/* Heltec V4 power operations export */
const struct power_ops heltec_v4_power_ops = {
    .name = "Heltec_V4_PA",
    .init = heltec_v4_init,
    .enable_rail = NULL,  /* No individual rail control */
    .radio_tx_begin = heltec_v4_tx_begin,
    .radio_tx_end = heltec_v4_tx_end,
};
