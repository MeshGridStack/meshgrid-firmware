/**
 * Button handling for UI navigation
 */

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include <Arduino.h>

/* Button state */
struct button_state {
    bool pressed;
    uint32_t press_time;
    uint32_t last_check;
};

/* Button callback types */
typedef void (*button_short_press_cb)(void);
typedef void (*button_long_press_cb)(void);

/* Button configuration */
struct button_config {
    int pin;                /* GPIO pin for button */
    uint32_t debounce_ms;   /* Debounce time */
    uint32_t long_press_ms; /* Long press threshold */
    button_short_press_cb on_short_press;
    button_long_press_cb on_long_press;
};

/* Button API */
void button_init(const struct button_config* config);
void button_check(void);
void button_set_config(const struct button_config* config);

#endif /* UI_BUTTON_H */
