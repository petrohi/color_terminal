#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal.h"

#define CHAR_WIDTH 7
#define CHAR_HEIGHT 14

#define SCREEN_WIDTH (COLS * CHAR_WIDTH)
#define SCREEN_HEIGHT (ROWS * CHAR_HEIGHT)

struct screen {
  color_t buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
};

void screen_clear(struct screen *screen, color_t inactive);

void screen_draw_character(struct screen *screen, size_t row, size_t col,
                           uint8_t character, enum font font, bool underlined,
                           color_t active, color_t inactive);

void screen_draw_cursor(struct screen *screen, size_t row, size_t col,
                        color_t color);

void screen_test_fonts(struct screen *screen);

void screen_test_mandelbrot(struct screen *screen, float window_x,
                            float window_y, float window_r, bool (*cancel)());

void screen_test_colors(struct screen *screen);

#endif
