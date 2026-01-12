/**
 * meshgrid hardware tests - Battery, solar, radio power tests
 */

#ifndef MESHGRID_HW_TEST_H
#define MESHGRID_HW_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Test result structure
 */
struct hw_test_result {
    bool passed;
    uint32_t duration_ms;

    /* Battery test results */
    uint16_t battery_start_mv;
    uint16_t battery_end_mv;
    int16_t battery_drop_mv;
    uint16_t estimated_current_ma;

    /* Solar test results */
    uint16_t solar_mv;
    uint16_t solar_ma;
    uint16_t solar_power_mw;
    bool solar_charging;

    /* Radio test results */
    uint8_t tx_power_dbm;
    int16_t rssi_dbm;
    uint8_t packets_sent;
    uint8_t packets_received;
};

/**
 * Test callbacks for progress reporting
 */
typedef void (*hw_test_progress_cb)(const char *status, uint8_t percent);

/**
 * Battery drain test
 * Measures battery voltage drop over specified duration under load
 * Duration: milliseconds (default 60000 = 1 minute)
 */
int hw_test_battery(struct hw_test_result *result, uint32_t duration_ms,
                    hw_test_progress_cb progress);

/**
 * Solar panel test
 * Measures solar voltage and estimates charging capability
 */
int hw_test_solar(struct hw_test_result *result, hw_test_progress_cb progress);

/**
 * Radio power test
 * Transmits test packets and measures power consumption
 */
int hw_test_radio(struct hw_test_result *result, hw_test_progress_cb progress);

/**
 * Full hardware test suite
 * Runs all tests in sequence
 */
int hw_test_all(struct hw_test_result *results, int max_results,
                hw_test_progress_cb progress);

/**
 * Format test result for display
 */
void hw_test_format_result(char *buf, size_t len, const struct hw_test_result *result,
                           const char *test_name);

#ifdef __cplusplus
}
#endif

#endif /* MESHGRID_HW_TEST_H */
