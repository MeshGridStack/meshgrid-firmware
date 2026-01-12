/**
 * Button handling for UI navigation
 */

#include "button.h"
#include <Arduino.h>

/* Global button state */
static struct button_state btn_state = {false, 0, 0};
static struct button_config btn_config = {-1, 50, 1000, nullptr, nullptr};

void button_init(const struct button_config *config) {
    if (!config || config->pin < 0) return;

    btn_config = *config;
    btn_state.pressed = false;
    btn_state.press_time = 0;
    btn_state.last_check = 0;

    pinMode(btn_config.pin, INPUT_PULLUP);
}

void button_set_config(const struct button_config *config) {
    if (config) {
        btn_config = *config;
    }
}

void button_check(void) {
    if (btn_config.pin < 0) return;
    if (millis() - btn_state.last_check < btn_config.debounce_ms) return;
    btn_state.last_check = millis();

    bool pressed = (digitalRead(btn_config.pin) == LOW);

    if (pressed && !btn_state.pressed) {
        /* Button just pressed */
        btn_state.pressed = true;
        btn_state.press_time = millis();
    } else if (!pressed && btn_state.pressed) {
        /* Button released */
        uint32_t press_duration = millis() - btn_state.press_time;
        btn_state.pressed = false;

        if (press_duration >= btn_config.long_press_ms) {
            /* Long press */
            if (btn_config.on_long_press) {
                btn_config.on_long_press();
            }
        } else {
            /* Short press */
            if (btn_config.on_short_press) {
                btn_config.on_short_press();
            }
        }
    }
}
