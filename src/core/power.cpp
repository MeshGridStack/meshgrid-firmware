/**
 * Power management
 */

#include "power.h"
#include <Arduino.h>

extern "C" {
#include "hardware/telemetry/telemetry.h"
}

#if defined(ARDUINO_ARCH_ESP32)
#    include <esp_sleep.h>
#endif

/* External telemetry state */
extern struct telemetry_data telemetry;

/* Last activity timestamp */
static uint32_t last_activity_time = 0;

void power_mark_activity(void) {
    last_activity_time = millis();
}

void power_check_sleep(void) {
    static uint32_t sleep_check_time = 0;

    if (millis() - sleep_check_time < 2000) {
        return; /* Check every 2 seconds */
    }
    sleep_check_time = millis();

    /* Only sleep if on battery power (not USB) */
    if (telemetry.is_usb_power) {
        return;
    }

    /* Check if idle (no activity for >2 seconds) */
    uint32_t idle_time = millis() - last_activity_time;
    if (idle_time <= 2000) {
        return;
    }

/* Enter light sleep for 100ms */
/* Radio interrupt will wake us up if packet arrives */
#if defined(ARDUINO_ARCH_ESP32)
    /* ESP32: Hardware light sleep */
    esp_sleep_enable_timer_wakeup(100000); /* 100ms in microseconds */
    esp_light_sleep_start();
#elif defined(ARDUINO_ARCH_NRF52)
    /* nRF52840: Low-power mode */
    __WFI(); /* Wait For Interrupt - lowest power until interrupt */
#elif defined(ARDUINO_ARCH_RP2040)
    /* RP2040: Sleep briefly */
    delay(100); /* RP2040 will enter low-power during delay */
#endif
}
