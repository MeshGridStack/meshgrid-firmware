/**
 * Radio HAL - Generic radio initialization for all chip types
 */

#include "radio_hal.h"
#include "lib/board.h"
#include <Arduino.h>

extern const struct board_config *board;

int radio_hal_init(struct radio_instance *radio_inst,
                   const struct radio_pins *pins,
                   SPIClass *spi,
                   const struct radio_config *config,
                   enum radio_type type) {
    int state = RADIOLIB_ERR_UNKNOWN;
    radio_inst->type = type;

    /* Create Module with correct DIO pin mapping for radio type */
    Module *mod;

    /* Initialize based on radio chip type */
    switch (type) {
        case RADIO_SX1262:
        case RADIO_SX1268: {
            /* SX126x: cs, dio1 (interrupt), reset, busy */
            mod = new Module(pins->cs, pins->dio1, pins->reset, pins->busy, *spi);
            SX1262 *sx1262 = new SX1262(mod);
            radio_inst->sx1262 = sx1262;

            Serial.print("SX1262 init (TCXO=");
            Serial.print(config->tcxo_voltage);
            Serial.print(")... ");

            /* SX126x::begin(freq, bw, sf, cr, syncWord, power, preamble, tcxo) */
            state = sx1262->begin(
                config->frequency,
                config->bandwidth,
                config->spreading_factor,
                config->coding_rate,
                RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                config->tx_power,
                config->preamble_len,
                config->tcxo_voltage
            );

            /* Retry without TCXO if it fails */
            if ((state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) &&
                config->tcxo_voltage > 0) {
                Serial.print("TCXO failed, retry... ");
                state = sx1262->begin(
                    config->frequency,
                    config->bandwidth,
                    config->spreading_factor,
                    config->coding_rate,
                    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                    config->tx_power,
                    config->preamble_len,
                    0.0
                );
            }

            if (state == RADIOLIB_ERR_NONE) {
                if (config->use_crc) sx1262->setCRC(1);
                sx1262->explicitHeader();
                if (config->dio2_as_rf_switch) {
                    sx1262->setDio2AsRfSwitch(true);
                }
            }
            break;
        }

        case RADIO_SX1276:
        case RADIO_SX1278: {
            /* SX127x: cs, dio0 (interrupt), reset, dio1 */
            mod = new Module(pins->cs, pins->dio0, pins->reset, pins->dio1, *spi);
            SX1276 *sx1276 = new SX1276(mod);
            radio_inst->sx1276 = sx1276;

            Serial.print("SX1276 init... ");

            /* SX127x::begin(freq, bw, sf, cr, syncWord, power, preamble) */
            state = sx1276->begin(
                config->frequency,
                config->bandwidth,
                config->spreading_factor,
                config->coding_rate,
                RADIOLIB_SX127X_SYNC_WORD,
                config->tx_power,
                config->preamble_len
            );

            if (state == RADIOLIB_ERR_NONE) {
                if (config->use_crc) sx1276->setCRC(1);
                sx1276->explicitHeader();
            }
            break;
        }

        default:
            Serial.println("Unsupported radio type!");
            return -1;
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("FAILED: ");
        Serial.println(state);
        return -1;
    }

    Serial.println("OK");
    return 0;
}
