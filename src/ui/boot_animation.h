/**
 * Boot Animation - Animated meshgrid boot logo
 */

#ifndef MESHGRID_BOOT_ANIMATION_H
#define MESHGRID_BOOT_ANIMATION_H

#include <Adafruit_SSD1306.h>
#include "version.h"

/**
 * Show animated boot sequence with radio waves and mesh network
 * Loops continuously for specified duration
 */
static inline void show_boot_animation(Adafruit_SSD1306* display, uint32_t duration_ms = 2000) {
    if (!display) return;

    const int frame_delay = 50; // ms per frame
    const int frames_per_cycle = 30; // Frames before loop repeats
    uint32_t start_time = millis();
    int frame = 0;

    /* Loop animation until duration expires */
    while (millis() - start_time < duration_ms) {
        display->clearDisplay();

        int cycle_frame = frame % frames_per_cycle;

        /* Animated radio waves expanding from center */
        int center_x = 64;
        int center_y = 32;
        int radius = (cycle_frame * 2) % 25;

        /* Draw 3 expanding wave rings */
        for (int r = 0; r < 3; r++) {
            int wave_radius = radius + (r * 8);
            if (wave_radius < 35 && wave_radius > 2) {
                display->drawCircle(center_x, center_y, wave_radius, SSD1306_WHITE);
            }
        }

        /* Mesh nodes always visible after first few frames */
        bool show_nodes = (frame > 5);
        bool show_links = (frame > 10);

        if (show_nodes) {
            /* 4 corner nodes */
            display->fillCircle(20, 15, 2, SSD1306_WHITE);
            display->fillCircle(108, 15, 2, SSD1306_WHITE);
            display->fillCircle(20, 49, 2, SSD1306_WHITE);
            display->fillCircle(108, 49, 2, SSD1306_WHITE);

            /* Center node (this device) - pulsing */
            int center_radius = 3 + (cycle_frame % 4) / 2;
            display->fillCircle(center_x, center_y, center_radius, SSD1306_WHITE);
        }

        if (show_links) {
            /* Mesh links to center */
            display->drawLine(20, 15, center_x, center_y, SSD1306_WHITE);
            display->drawLine(108, 15, center_x, center_y, SSD1306_WHITE);
            display->drawLine(20, 49, center_x, center_y, SSD1306_WHITE);
            display->drawLine(108, 49, center_x, center_y, SSD1306_WHITE);
        }

        /* MESHGRID text at bottom - typing effect */
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);

        const char* text = "MESHGRID";
        int chars_to_show = (frame / 2) > 8 ? 8 : (frame / 2); /* Type 1 char every 2 frames */
        display->setCursor(32, 54);
        for (int i = 0; i < chars_to_show; i++) {
            display->print(text[i]);
        }

        /* Version number - static, top right corner */
        display->setTextSize(1);
        display->setCursor(90, 2);
        display->print("v");
        display->print(MESHGRID_VERSION);

        display->display();
        delay(frame_delay);
        frame++;
    }
}

#endif // MESHGRID_BOOT_ANIMATION_H
