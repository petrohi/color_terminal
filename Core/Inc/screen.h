#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal.h"
#include "font.h"

struct screen {
  const uint8_t char_width;
  const uint8_t char_height;
  const uint8_t cols;
  const uint8_t rows;
  const struct bitmap_font *normal_bitmap_font;
  const struct bitmap_font *bold_bitmap_font;
  uint8_t* buffer;
};

void screen_clear_rows(struct screen *screen, size_t from_row, size_t to_row,
                       color_t inactive);

void screen_clear_cols(struct screen *screen, size_t row, size_t from_col,
                       size_t to_col, color_t inactive);

void screen_scroll(struct screen *screen, enum scroll scroll, size_t from_row,
                   size_t to_row, size_t rows, color_t inactive);

void screen_draw_codepoint(struct screen *screen, size_t row, size_t col,
                           codepoint_t codepoint, enum font font, bool italic,
                           bool underlined, bool crossedout, color_t active,
                           color_t inactive);

void screen_test_fonts(struct screen *screen, enum font font);

void screen_test_mandelbrot(struct screen *screen, float window_x,
                            float window_y, float window_r, bool (*cancel)());

void screen_test_colors(struct screen *screen);

void screen_shift_right(struct screen *screen, size_t row, size_t col,
                        size_t cols, color_t inactive);

void screen_shift_left(struct screen *screen, size_t row, size_t col,
                       size_t cols, color_t inactive);

#endif
