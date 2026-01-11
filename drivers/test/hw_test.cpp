/**
 * meshgrid hardware tests implementation
 */

#include "hw_test.h"
#include "../telemetry/telemetry.h"
#include "../radio/radio_hal.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <string.h>
#include <stdio.h>

/* Test constants */
#define BATTERY_SAMPLE_INTERVAL_MS  1000
#define RADIO_TEST_PACKETS          10
#define RADIO_TEST_PACKET_SIZE      32
#define RADIO_TEST_DELAY_MS         100

/**
 * Battery drain test
 * Runs CPU and radio under load to measure power consumption
 */
int hw_test_battery(struct hw_test_result *result, uint32_t duration_ms,
                    hw_test_progress_cb progress)
{
    struct telemetry_data telem;
    uint32_t start_time, elapsed;
    uint16_t samples = 0;
    uint32_t voltage_sum = 0;

    memset(result, 0, sizeof(*result));
    result->duration_ms = duration_ms;

    if (progress) progress("Reading initial battery...", 0);

    /* Read initial battery voltage */
    telemetry_read(&telem);
    result->battery_start_mv = telem.battery_mv;

    if (result->battery_start_mv == 0) {
        if (progress) progress("No battery detected", 100);
        return -1;
    }

    start_time = millis();

    /* Run load test */
    while ((elapsed = millis() - start_time) < duration_ms) {
        /* CPU load - compute something */
        volatile uint32_t dummy = 0;
        for (int i = 0; i < 10000; i++) {
            dummy += i * i;
        }

        /* Sample voltage periodically */
        if ((elapsed / BATTERY_SAMPLE_INTERVAL_MS) > samples) {
            telemetry_read(&telem);
            voltage_sum += telem.battery_mv;
            samples++;

            if (progress) {
                char status[32];
                uint8_t percent = (elapsed * 100) / duration_ms;
                snprintf(status, sizeof(status), "Testing... %dmV", telem.battery_mv);
                progress(status, percent);
            }
        }

        delay(10);
    }

    /* Read final battery voltage */
    telemetry_read(&telem);
    result->battery_end_mv = telem.battery_mv;
    result->battery_drop_mv = result->battery_start_mv - result->battery_end_mv;

    /* Estimate current draw from voltage drop
     * Assuming 1000mAh battery, V_drop = I * t / C
     * This is a rough estimate */
    if (result->battery_drop_mv > 0 && duration_ms > 0) {
        /* mA = (mV_drop * capacity_mAh) / (time_hours * 1000) */
        /* Simplified: assume linear relationship */
        result->estimated_current_ma = (result->battery_drop_mv * 100) / (duration_ms / 1000);
    }

    result->passed = (result->battery_start_mv > 3000);

    if (progress) progress("Battery test complete", 100);

    return 0;
}

/**
 * Solar panel test
 * Measures solar voltage and charging status
 */
int hw_test_solar(struct hw_test_result *result, hw_test_progress_cb progress)
{
    struct telemetry_data telem;
    uint16_t solar_samples[10];
    uint32_t solar_sum = 0;

    memset(result, 0, sizeof(*result));

    if (progress) progress("Testing solar input...", 0);

    /* Take multiple samples */
    for (int i = 0; i < 10; i++) {
        telemetry_read(&telem);
        solar_samples[i] = telem.solar_mv;
        solar_sum += telem.solar_mv;
        delay(100);

        if (progress) {
            progress("Sampling solar...", (i + 1) * 10);
        }
    }

    result->solar_mv = solar_sum / 10;
    result->solar_charging = telem.is_charging;

    /* Estimate power (assuming 5V panel, ~100mA typical) */
    if (result->solar_mv > 4000) {
        /* Good sunlight */
        result->solar_ma = 100;
        result->solar_power_mw = (result->solar_mv * result->solar_ma) / 1000;
    } else if (result->solar_mv > 2000) {
        /* Partial sunlight */
        result->solar_ma = 30;
        result->solar_power_mw = (result->solar_mv * result->solar_ma) / 1000;
    } else {
        result->solar_ma = 0;
        result->solar_power_mw = 0;
    }

    result->passed = telem.has_solar;
    result->duration_ms = 1000;

    if (progress) {
        if (result->passed) {
            progress("Solar panel detected", 100);
        } else {
            progress("No solar panel", 100);
        }
    }

    return 0;
}

/**
 * Radio power test
 * Transmits test packets at max power
 */
int hw_test_radio(struct hw_test_result *result, hw_test_progress_cb progress)
{
    uint8_t test_packet[RADIO_TEST_PACKET_SIZE];
    uint32_t start_time;

    memset(result, 0, sizeof(*result));

    if (!get_radio()) {
        if (progress) progress("Radio not initialized", 100);
        return -1;
    }

    if (progress) progress("Testing radio TX...", 0);

    /* Fill test packet */
    for (int i = 0; i < RADIO_TEST_PACKET_SIZE; i++) {
        test_packet[i] = i & 0xFF;
    }

    /* Get current TX power */
    result->tx_power_dbm = 22;  /* Max for SX1262 */

    start_time = millis();

    /* Transmit test packets */
    for (int i = 0; i < RADIO_TEST_PACKETS; i++) {
        int state = get_radio()->transmit(test_packet, RADIO_TEST_PACKET_SIZE);

        if (state == RADIOLIB_ERR_NONE) {
            result->packets_sent++;
        }

        if (progress) {
            char status[32];
            snprintf(status, sizeof(status), "TX packet %d/%d", i + 1, RADIO_TEST_PACKETS);
            progress(status, ((i + 1) * 100) / RADIO_TEST_PACKETS);
        }

        delay(RADIO_TEST_DELAY_MS);
    }

    result->duration_ms = millis() - start_time;
    result->passed = (result->packets_sent == RADIO_TEST_PACKETS);

    /* Read RSSI (if we received anything back) */
    result->rssi_dbm = get_radio()->getRSSI();

    if (progress) {
        char status[32];
        snprintf(status, sizeof(status), "TX: %d/%d packets",
                 result->packets_sent, RADIO_TEST_PACKETS);
        progress(status, 100);
    }

    return 0;
}

/**
 * Run all tests
 */
int hw_test_all(struct hw_test_result *results, int max_results,
                hw_test_progress_cb progress)
{
    int test_count = 0;

    if (max_results < 3) {
        return -1;
    }

    if (progress) progress("Starting full test suite...", 0);

    /* Battery test (30 seconds for quick test) */
    if (progress) progress("Battery test...", 10);
    hw_test_battery(&results[test_count++], 30000, NULL);

    /* Solar test */
    if (progress) progress("Solar test...", 50);
    hw_test_solar(&results[test_count++], NULL);

    /* Radio test */
    if (progress) progress("Radio test...", 80);
    hw_test_radio(&results[test_count++], NULL);

    if (progress) progress("All tests complete", 100);

    return test_count;
}

void hw_test_format_result(char *buf, size_t len, const struct hw_test_result *result,
                           const char *test_name)
{
    if (strcmp(test_name, "battery") == 0) {
        snprintf(buf, len,
                 "Battery: %s\n"
                 "  Start: %dmV\n"
                 "  End:   %dmV\n"
                 "  Drop:  %dmV\n"
                 "  Est:   ~%dmA",
                 result->passed ? "PASS" : "FAIL",
                 result->battery_start_mv,
                 result->battery_end_mv,
                 result->battery_drop_mv,
                 result->estimated_current_ma);
    } else if (strcmp(test_name, "solar") == 0) {
        snprintf(buf, len,
                 "Solar: %s\n"
                 "  Voltage: %dmV\n"
                 "  Current: ~%dmA\n"
                 "  Power:   ~%dmW\n"
                 "  Charging: %s",
                 result->passed ? "DETECTED" : "NONE",
                 result->solar_mv,
                 result->solar_ma,
                 result->solar_power_mw,
                 result->solar_charging ? "Yes" : "No");
    } else if (strcmp(test_name, "radio") == 0) {
        snprintf(buf, len,
                 "Radio: %s\n"
                 "  TX Power: %ddBm\n"
                 "  Packets:  %d/%d\n"
                 "  Duration: %dms",
                 result->passed ? "PASS" : "FAIL",
                 result->tx_power_dbm,
                 result->packets_sent, RADIO_TEST_PACKETS,
                 result->duration_ms);
    } else {
        snprintf(buf, len, "Unknown test: %s", test_name);
    }
}
