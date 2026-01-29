/**
 * UI Layout Framework - Simple grid system for OLED displays
 *
 * Automatically handles positioning, margins, and spacing to prevent
 * layout errors like text overflow and footer overlap.
 */

#ifndef MESHGRID_UI_LAYOUT_H
#define MESHGRID_UI_LAYOUT_H

#include <Adafruit_SSD1306.h>

/**
 * UI Layout Context - tracks current position and available space
 */
struct ui_layout {
    Adafruit_SSD1306* display;
    int current_y;          /* Current Y position for next element */
    int content_start_y;    /* Top of content area (below header) */
    int content_end_y;      /* Bottom of content area (above footer) */
    int left_margin;        /* Left margin for all content */
    int right_margin;       /* Right margin for all content */
    int line_height;        /* Height of one text line */
    int line_spacing;       /* Spacing between lines */
};

/**
 * Initialize layout context for a screen
 * Call this after drawing header, before adding content
 */
static inline void ui_layout_init(struct ui_layout* layout, Adafruit_SSD1306* display) {
    layout->display = display;
    layout->content_start_y = 12;  /* Below header (11px header + 1px gap) */
    layout->content_end_y = 56;    /* Above footer (64 - 8px footer) */
    layout->left_margin = 2;
    layout->right_margin = 2;
    layout->line_height = 8;       /* Standard font height */
    layout->line_spacing = 10;     /* Spacing between lines */
    layout->current_y = layout->content_start_y;
}

/**
 * Check if there's enough space for another line
 * Returns: true if line will fit, false if it would overlap footer
 */
static inline bool ui_layout_can_fit_line(struct ui_layout* layout) {
    return (layout->current_y + layout->line_height) <= layout->content_end_y;
}

/**
 * Add a text line at current position and advance
 * Returns: true if added, false if no space
 */
static inline bool ui_layout_add_line(struct ui_layout* layout, const char* text) {
    if (!ui_layout_can_fit_line(layout)) {
        return false; /* No space */
    }

    layout->display->setCursor(layout->left_margin, layout->current_y);
    layout->display->print(text);
    layout->current_y += layout->line_spacing;
    return true;
}

/**
 * Add a text line with custom spacing
 */
static inline bool ui_layout_add_line_spaced(struct ui_layout* layout, const char* text, int spacing) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    layout->display->setCursor(layout->left_margin, layout->current_y);
    layout->display->print(text);
    layout->current_y += spacing;
    return true;
}

/**
 * Add vertical spacing (gap)
 */
static inline void ui_layout_add_gap(struct ui_layout* layout, int pixels) {
    layout->current_y += pixels;
}

/**
 * Get remaining vertical space in pixels
 */
static inline int ui_layout_remaining_space(struct ui_layout* layout) {
    return layout->content_end_y - layout->current_y;
}

/**
 * Reset position to top of content area
 */
static inline void ui_layout_reset(struct ui_layout* layout) {
    layout->current_y = layout->content_start_y;
}

/* ========================================================================= */
/* Grid / Column Layout Functions                                            */
/* ========================================================================= */

/**
 * Add text in a two-column layout (label: value)
 * Example: "Freq:"  "915.0 MHz"
 */
static inline bool ui_layout_add_label_value(struct ui_layout* layout, const char* label, const char* value) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    /* Label on left */
    layout->display->setCursor(layout->left_margin, layout->current_y);
    layout->display->print(label);

    /* Value on right (after label) */
    int label_width = strlen(label) * 6; /* 6px per char for size 1 font */
    layout->display->setCursor(layout->left_margin + label_width, layout->current_y);
    layout->display->print(value);

    layout->current_y += layout->line_spacing;
    return true;
}

/**
 * Add text aligned to the right edge
 * Useful for status indicators, values, etc.
 */
static inline bool ui_layout_add_line_right(struct ui_layout* layout, const char* text) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    int text_width = strlen(text) * 6;
    int x = 128 - text_width - layout->right_margin;
    layout->display->setCursor(x, layout->current_y);
    layout->display->print(text);
    layout->current_y += layout->line_spacing;
    return true;
}

/**
 * Add text centered horizontally
 */
static inline bool ui_layout_add_line_centered(struct ui_layout* layout, const char* text) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    int text_width = strlen(text) * 6;
    int x = (128 - text_width) / 2;
    layout->display->setCursor(x, layout->current_y);
    layout->display->print(text);
    layout->current_y += layout->line_spacing;
    return true;
}

/**
 * Start a grid row with multiple columns
 * positions: array of X positions for each column (0 = left margin)
 * texts: array of text strings for each column
 * count: number of columns
 */
static inline bool ui_layout_add_grid_row(struct ui_layout* layout, const int* positions, const char** texts, int count) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    for (int i = 0; i < count; i++) {
        int x = (positions[i] == 0) ? layout->left_margin : positions[i];
        layout->display->setCursor(x, layout->current_y);
        layout->display->print(texts[i]);
    }

    layout->current_y += layout->line_spacing;
    return true;
}

/**
 * Add text at a specific column (0 = left, 1 = middle, 2 = right third)
 */
static inline bool ui_layout_add_text_col(struct ui_layout* layout, const char* text, int column) {
    if (!ui_layout_can_fit_line(layout)) {
        return false;
    }

    int x;
    if (column == 0) {
        x = layout->left_margin; /* Left */
    } else if (column == 1) {
        x = 42; /* Middle (128/3) */
    } else {
        x = 84; /* Right third */
    }

    layout->display->setCursor(x, layout->current_y);
    layout->display->print(text);
    /* Don't auto-advance Y - allows multiple columns on same line */
    return true;
}

/**
 * Advance to next line (call after using ui_layout_add_text_col)
 */
static inline void ui_layout_next_line(struct ui_layout* layout) {
    layout->current_y += layout->line_spacing;
}

#endif // MESHGRID_UI_LAYOUT_H

