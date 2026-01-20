/**
 * Display screen rendering for Meshgrid firmware
 *
 * Handles rendering of all UI screens including status, neighbors,
 * messages, statistics, device info, and radio configuration.
 */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <Adafruit_SSD1306.h>
#include "utils/types.h"

/* Display screens */
enum display_screen {
    SCREEN_STATUS = 0,    /* Main status overview */
    SCREEN_NEIGHBORS = 1, /* Neighbor list */
    SCREEN_MESSAGES = 2,  /* Message inbox */
    SCREEN_STATS = 3,     /* Detailed statistics */
    SCREEN_INFO = 4,      /* Device info */
    SCREEN_RADIO = 5,     /* Radio configuration */
    SCREEN_SECURITY = 6,  /* Security (PIN) */
    SCREEN_COUNT = 7
};

/* Display state */
struct display_state {
    enum display_screen current_screen;
    bool dirty;
    uint8_t neighbor_scroll;
    uint8_t message_scroll;
};

/* Forward declarations for external data structures */
/* Note: Enums and structs are defined in their respective modules */

/* Display API */
int display_init(Adafruit_SSD1306** display_out);
void display_state_init(struct display_state* state);
void display_update(Adafruit_SSD1306* display, struct display_state* state);

/* Screen rendering functions - called internally by display_update */
void draw_screen_status(Adafruit_SSD1306* display);
void draw_screen_neighbors(Adafruit_SSD1306* display, struct display_state* state);
void draw_screen_messages(Adafruit_SSD1306* display, struct display_state* state);
void draw_screen_stats(Adafruit_SSD1306* display);
void draw_screen_info(Adafruit_SSD1306* display);
void draw_screen_radio(Adafruit_SSD1306* display);
void draw_screen_security(Adafruit_SSD1306* display);

/* Navigation functions */
void display_next_screen(struct display_state* state);
void display_scroll_up(struct display_state* state);
void display_scroll_down(struct display_state* state);

#endif /* UI_SCREENS_H */
