/**
 * Generic Responsive UI Library for small OLED displays (128x64)
 *
 * Provides reusable components for building clean, responsive UIs
 * with consistent spacing and layout.
 */

#ifndef UI_LIB_H
#define UI_LIB_H

#include <Adafruit_SSD1306.h>

/* Display dimensions */
#define UI_SCREEN_WIDTH 128
#define UI_SCREEN_HEIGHT 64

/* Layout constants - can be adjusted for different densities */
#define UI_HEADER_HEIGHT 11
#define UI_FOOTER_HEIGHT 8
#define UI_CONTENT_TOP (UI_HEADER_HEIGHT + 1) /* Start below header */
#define UI_CONTENT_BOTTOM (UI_SCREEN_HEIGHT - UI_FOOTER_HEIGHT - 1)
#define UI_CONTENT_HEIGHT (UI_CONTENT_BOTTOM - UI_CONTENT_TOP)

/* Spacing presets */
#define UI_SPACING_TIGHT 10   /* Tight spacing between items */
#define UI_SPACING_NORMAL 12  /* Normal spacing */
#define UI_SPACING_RELAXED 14 /* Relaxed spacing */

/* Text truncation */
#define UI_MAX_SHORT_TEXT 16
#define UI_MAX_MEDIUM_TEXT 21
#define UI_MAX_LONG_TEXT 32

/**
 * UI List Item - Generic structure for list entries
 */
struct ui_list_item {
    char primary_text[UI_MAX_MEDIUM_TEXT];  /* Main text (left aligned) */
    char secondary_text[UI_MAX_SHORT_TEXT]; /* Secondary text (right aligned or below) */
    char badge[4];                          /* Small badge/icon (e.g., "C", "MC") */
    int value;                              /* Numeric value (e.g., RSSI) */
    bool show_value;                        /* Whether to display value */
};

/**
 * UI Label-Value Pair
 */
struct ui_label_value {
    const char* label;
    char value[UI_MAX_MEDIUM_TEXT];
    int y_pos; /* Auto-calculated if 0 */
};

/**
 * Draw a standard header bar with title and optional battery indicator
 */
static inline void ui_draw_header(Adafruit_SSD1306* display, const char* title, int battery_pct = -1) {
    display->setTextSize(1);

    /* Header background */
    display->fillRect(0, 0, UI_SCREEN_WIDTH, UI_HEADER_HEIGHT, SSD1306_WHITE);
    display->setTextColor(SSD1306_BLACK, SSD1306_WHITE);

    /* Title */
    display->setCursor(2, 2);
    display->print(title);

    /* Battery indicator on right */
    if (battery_pct >= 0) {
        char bat_str[8];
        snprintf(bat_str, sizeof(bat_str), "%d%%", battery_pct);
        display->setCursor(100, 2);
        display->print(bat_str);
    }

    display->setTextColor(SSD1306_WHITE); /* Reset to normal */
}

/**
 * Draw a footer with page indicator and optional hint
 */
static inline void ui_draw_footer(Adafruit_SSD1306* display, const char* hint, int current_page, int total_pages) {
    display->setTextSize(1);
    int y = UI_SCREEN_HEIGHT - UI_FOOTER_HEIGHT;

    /* Hint on left */
    if (hint) {
        display->setCursor(0, y);
        display->print(hint);
    }

    /* Page indicator on right */
    char page_str[8];
    snprintf(page_str, sizeof(page_str), "%d/%d", current_page, total_pages);
    int text_width = strlen(page_str) * 6; /* 6px per char at size 1 */
    display->setCursor(UI_SCREEN_WIDTH - text_width, y);
    display->print(page_str);
}

/**
 * Draw a label-value pair at specified position
 * Returns the Y position for the next item
 */
static inline int ui_draw_label_value(Adafruit_SSD1306* display, int y, const char* label, const char* value) {
    char line[UI_MAX_LONG_TEXT];
    snprintf(line, sizeof(line), "%s %s", label, value);
    display->setCursor(0, y);
    display->print(line);
    return y + UI_SPACING_NORMAL;
}

/**
 * Draw a list item with primary/secondary text layout
 * Layout: [Badge] Primary Text        Secondary
 * Returns the Y position for the next item
 */
static inline int ui_draw_list_item(Adafruit_SSD1306* display, int y, const struct ui_list_item* item) {
    char line[UI_MAX_LONG_TEXT];

    /* Left side: Badge + Primary text */
    if (item->badge[0] != '\0') {
        snprintf(line, sizeof(line), "%s %s", item->badge, item->primary_text);
    } else {
        snprintf(line, sizeof(line), "%s", item->primary_text);
    }
    display->setCursor(0, y);
    display->print(line);

    /* Right side: Value or secondary text */
    if (item->show_value) {
        snprintf(line, sizeof(line), "%d", item->value);
        int text_width = strlen(line) * 6;
        display->setCursor(UI_SCREEN_WIDTH - text_width - 2, y);
        display->print(line);
    } else if (item->secondary_text[0] != '\0') {
        int text_width = strlen(item->secondary_text) * 6;
        display->setCursor(UI_SCREEN_WIDTH - text_width - 2, y);
        display->print(item->secondary_text);
    }

    return y + UI_SPACING_TIGHT;
}

/**
 * Draw multiple label-value pairs with automatic spacing
 */
static inline void ui_draw_info_screen(Adafruit_SSD1306* display, struct ui_label_value* items, int item_count) {
    int y = UI_CONTENT_TOP + 2;
    for (int i = 0; i < item_count; i++) {
        y = ui_draw_label_value(display, y, items[i].label, items[i].value);
    }
}

/**
 * Draw a scrollable list with items
 * Returns true if there are more items to scroll
 */
static inline bool ui_draw_list(Adafruit_SSD1306* display, struct ui_list_item* items, int total_items,
                                int scroll_offset, int max_visible) {
    int y = UI_CONTENT_TOP;
    int end = (total_items < scroll_offset + max_visible) ? total_items : scroll_offset + max_visible;

    for (int i = scroll_offset; i < end; i++) {
        y = ui_draw_list_item(display, y, &items[i]);
    }

    /* Show scroll indicator if there are more items */
    if (total_items > max_visible) {
        char scroll_info[16];
        snprintf(scroll_info, sizeof(scroll_info), "%d-%d/%d", scroll_offset + 1, end, total_items);
        int text_width = strlen(scroll_info) * 6;
        display->setCursor(UI_SCREEN_WIDTH - text_width - 2, UI_SCREEN_HEIGHT - 8);
        display->print(scroll_info);
    }

    return (scroll_offset + max_visible) < total_items;
}

/**
 * Truncate text to fit within max_chars, adding "..." if needed
 */
static inline void ui_truncate_text(char* dest, const char* src, int max_chars) {
    if (strlen(src) <= max_chars) {
        strcpy(dest, src);
    } else {
        strncpy(dest, src, max_chars - 3);
        dest[max_chars - 3] = '.';
        dest[max_chars - 2] = '.';
        dest[max_chars - 1] = '.';
        dest[max_chars] = '\0';
    }
}

/**
 * Draw centered text at specified Y position
 */
static inline void ui_draw_centered_text(Adafruit_SSD1306* display, int y, const char* text) {
    int text_width = strlen(text) * 6; /* 6px per char at size 1 */
    int x = (UI_SCREEN_WIDTH - text_width) / 2;
    display->setCursor(x, y);
    display->print(text);
}

/**
 * Draw a progress bar
 */
static inline void ui_draw_progress_bar(Adafruit_SSD1306* display, int y, int value, int max_value, int width = 100,
                                        int height = 6) {
    int x = (UI_SCREEN_WIDTH - width) / 2;

    /* Border */
    display->drawRect(x, y, width, height, SSD1306_WHITE);

    /* Fill based on percentage */
    int fill_width = (value * (width - 2)) / max_value;
    if (fill_width > 0) {
        display->fillRect(x + 1, y + 1, fill_width, height - 2, SSD1306_WHITE);
    }
}

/**
 * Format time duration into human readable string
 */
static inline void ui_format_duration(char* dest, int max_len, uint32_t seconds) {
    if (seconds < 60) {
        snprintf(dest, max_len, "%lus", seconds);
    } else if (seconds < 3600) {
        snprintf(dest, max_len, "%lum", seconds / 60);
    } else if (seconds < 86400) {
        snprintf(dest, max_len, "%luh%lum", seconds / 3600, (seconds % 3600) / 60);
    } else {
        snprintf(dest, max_len, "%lud", seconds / 86400);
    }
}

/**
 * Format byte count into human readable string (B, KB, MB)
 */
static inline void ui_format_bytes(char* dest, int max_len, unsigned long bytes) {
    if (bytes < 1024) {
        snprintf(dest, max_len, "%lu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(dest, max_len, "%lu KB", bytes / 1024);
    } else {
        snprintf(dest, max_len, "%lu MB", bytes / (1024 * 1024));
    }
}

#endif /* UI_LIB_H */
