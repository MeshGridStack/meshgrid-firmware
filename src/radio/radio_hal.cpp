/**
 * Radio HAL - Generic radio initialization for all chip types
 */

#include "radio_hal.h"
#include "utils/debug.h"
#include "hardware/board.h"
#include <Arduino.h>

extern const struct board_config* board;

int radio_hal_init(struct radio_instance* radio_inst, const struct radio_pins* pins, SPIClass* spi,
                   const struct radio_config* config, enum radio_type type) {
    int state = RADIOLIB_ERR_UNKNOWN;
    radio_inst->type = type;

    /* Create Module with correct DIO pin mapping for radio type */
    Module* mod;

    /* Initialize based on radio chip type */
    switch (type) {
        case RADIO_SX1262:
        case RADIO_SX1268: {
            /* SX126x: cs, dio1 (interrupt), reset, busy */
            mod = new Module(pins->cs, pins->dio1, pins->reset, pins->busy, *spi);
            SX1262* sx1262 = new SX1262(mod);
            radio_inst->sx1262 = sx1262;

            DEBUG_INFOF("SX1262 init (TCXO=%.1f, DIO2_RF_SW=%s, sync=0x%02X)...", config->tcxo_voltage,
                        config->dio2_as_rf_switch ? "true" : "false",
                        config->sync_word ? config->sync_word : RADIOLIB_SX126X_SYNC_WORD_PRIVATE);

            /* SX126x::begin(freq, bw, sf, cr, syncWord, power, preamble, tcxo) */
            state = sx1262->begin(config->frequency, config->bandwidth, config->spreading_factor, config->coding_rate,
                                  config->sync_word ? config->sync_word : RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                  config->tx_power, config->preamble_len, config->tcxo_voltage);

            /* Retry without TCXO if it fails */
            if ((state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) &&
                config->tcxo_voltage > 0) {
                DEBUG_INFO("TCXO failed, retry...");
                state =
                    sx1262->begin(config->frequency, config->bandwidth, config->spreading_factor, config->coding_rate,
                                  config->sync_word ? config->sync_word : RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                  config->tx_power, config->preamble_len, 0.0);
            }

            if (state == RADIOLIB_ERR_NONE) {
                if (config->use_crc)
                    sx1262->setCRC(1);
                sx1262->explicitHeader();
                if (config->dio2_as_rf_switch) {
                    sx1262->setDio2AsRfSwitch(true);
                }
                /* Enable boosted RX gain for better sensitivity (MeshCore does this) */
                sx1262->setRxBoostedGainMode(true);
                /* Set current limit to 140mA (SX126x max safe value) */
                sx1262->setCurrentLimit(140);
            }
            break;
        }

        case RADIO_SX1276:
        case RADIO_SX1278: {
            /* SX127x: cs, dio0 (interrupt), reset, dio1 */
            DEBUG_INFOF("SX1276 pins: CS=%d DIO0=%d RST=%d DIO1=%d", pins->cs, pins->dio0, pins->reset, pins->dio1);
            mod = new Module(pins->cs, pins->dio0, pins->reset, pins->dio1, *spi);
            SX1276* sx1276 = new SX1276(mod);
            radio_inst->sx1276 = sx1276;

            DEBUG_INFOF("SX1276 init (sync=0x%02X)...",
                        config->sync_word ? config->sync_word : RADIOLIB_SX127X_SYNC_WORD);

            /* SX127x::begin(freq, bw, sf, cr, syncWord, power, preamble) */
            state = sx1276->begin(config->frequency, config->bandwidth, config->spreading_factor, config->coding_rate,
                                  config->sync_word ? config->sync_word : RADIOLIB_SX127X_SYNC_WORD, config->tx_power,
                                  config->preamble_len);

            DEBUG_INFOF("SX1276 begin() returned: %d", state);
            if (state == RADIOLIB_ERR_NONE) {
                if (config->use_crc)
                    sx1276->setCRC(1);
                sx1276->explicitHeader();
                DEBUG_INFO("SX1276 init SUCCESS");
            } else {
                DEBUG_ERRORF("SX1276 init FAILED: %d", state);
            }
            break;
        }

        default:
            DEBUG_INFO("Unsupported radio type!");
            return -1;
    }

    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_ERRORF("Radio init FAILED: %d", state);
        return -1;
    }

    DEBUG_INFO("Radio init OK");
    return 0;
}
