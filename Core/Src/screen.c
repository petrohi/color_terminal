#include "screen.h"

#include "font.h"
#include <complex.h>
#include <memory.h>

#define REPLACEMENT_CODEPOINT 0xfffd

void screen_clear_rows(struct screen *screen, size_t from_row, size_t to_row,
                       color_t inactive) {
  if (to_row <= from_row)
    return;

  if (to_row > ROWS)
    return;

  size_t size = SCREEN_WIDTH * CHAR_HEIGHT * (to_row - from_row);
  size_t offset = SCREEN_WIDTH * CHAR_HEIGHT * from_row;

  memset(screen->buffer + offset, inactive, size);
}

void screen_clear_cols(struct screen *screen, size_t row, size_t from_col,
                       size_t to_col, color_t inactive) {
  if (row >= ROWS)
    return;

  if (to_col <= from_col)
    return;

  if (to_col > COLS)
    return;

  size_t size = CHAR_WIDTH * (to_col - from_col);
  size_t offset = SCREEN_WIDTH * CHAR_HEIGHT * row + CHAR_WIDTH * from_col;

  for (size_t i = 0; i < CHAR_HEIGHT; ++i)
    memset(screen->buffer + offset + (SCREEN_WIDTH * i), inactive, size);
}

void screen_shift_right(struct screen *screen, size_t row, size_t col,
                        size_t cols, color_t inactive) {
  if (row >= ROWS)
    return;

  if (col >= COLS)
    return;

  if (col + cols > COLS)
    return;

  if (col + cols < COLS) {
    size_t size = CHAR_WIDTH * (COLS - col - cols);
    size_t offset = SCREEN_WIDTH * CHAR_HEIGHT * row + CHAR_WIDTH * col;
    size_t disp = CHAR_WIDTH * cols;

    for (size_t i = 0; i < CHAR_HEIGHT; ++i)
      memmove(screen->buffer + offset + (SCREEN_WIDTH * i) + disp,
              screen->buffer + offset + (SCREEN_WIDTH * i), size);
  }

  screen_clear_cols(screen, row, col, col + cols, inactive);
}

void screen_shift_left(struct screen *screen, size_t row, size_t col,
                       size_t cols, color_t inactive) {
  if (row >= ROWS)
    return;

  if (col >= COLS)
    return;

  if (col + cols > COLS)
    return;

  if (col + cols < COLS) {
    size_t size = CHAR_WIDTH * (COLS - col - cols);
    size_t offset = SCREEN_WIDTH * CHAR_HEIGHT * row + CHAR_WIDTH * col;
    size_t disp = CHAR_WIDTH * cols;

    for (size_t i = 0; i < CHAR_HEIGHT; ++i)
      memcpy(screen->buffer + offset + (SCREEN_WIDTH * i),
             screen->buffer + offset + (SCREEN_WIDTH * i) + disp, size);
  }

  screen_clear_cols(screen, row, COLS - cols, COLS, inactive);
}

void screen_scroll(struct screen *screen, enum scroll scroll, size_t from_row,
                   size_t to_row, size_t rows, color_t inactive) {
  if (to_row <= from_row)
    return;

  if (to_row > ROWS)
    return;

  if (to_row <= from_row + rows) {
    screen_clear_rows(screen, from_row, to_row, inactive);
    return;
  }

  size_t disp = SCREEN_WIDTH * CHAR_HEIGHT * rows;
  size_t size = SCREEN_WIDTH * CHAR_HEIGHT * (to_row - from_row - rows);
  size_t offset = SCREEN_WIDTH * CHAR_HEIGHT * from_row;

  if (scroll == SCROLL_DOWN) {
    memmove((void *)screen->buffer + offset + disp,
            (void *)screen->buffer + offset, size);

    screen_clear_rows(screen, from_row, from_row + rows, inactive);
  } else if (scroll == SCROLL_UP) {
    memcpy((void *)screen->buffer + offset,
           (void *)screen->buffer + offset + disp, size);

    screen_clear_rows(screen, to_row - rows, to_row, inactive);
  }
}

void screen_draw_codepoint(struct screen *screen, size_t row, size_t col,
                           codepoint_t codepoint, enum font font, bool italic,
                           bool underlined, bool crossedout, color_t active,
                           color_t inactive) {
  if (row >= ROWS)
    return;

  if (col >= COLS)
    return;

  size_t base = ((row * COLS * CHAR_HEIGHT) + col) * CHAR_WIDTH;
  const struct bitmap_font *bitmap_font;
  if (font == FONT_BOLD) {
    bitmap_font = &bold_font;
  } else {
    bitmap_font = &normal_font;
  }

  const unsigned char *glyph = find_glyph(bitmap_font, codepoint);

  if (!glyph)
    glyph = find_glyph(bitmap_font, REPLACEMENT_CODEPOINT);

  for (size_t char_y = 0; char_y < CHAR_HEIGHT; char_y++) {
    for (size_t char_x = 0; char_x < CHAR_WIDTH; char_x++) {

      size_t i = base + COLS * CHAR_WIDTH * char_y + char_x;

      color_t color = inactive;

      if (glyph &&
          ((underlined && char_y == CHAR_HEIGHT - 2) ||
           (crossedout && char_y == CHAR_HEIGHT / 2) ||

           (char_x < bitmap_font->width && char_y < bitmap_font->height &&

            glyph[char_y] & (1 << char_x)))) {

        color = active;
      }

      screen->buffer[i] = color;
    }
  }
}

void screen_test_fonts(struct screen *screen, enum font font) {
  for (size_t row = 0; row < 24; row++) {
    for (size_t col = 0; col < 64; col++) {
      codepoint_t codepoint = ((row * 64) + col);

      screen_draw_codepoint(screen, row, col, codepoint, font, false, false,
                            false, 0xf, 0);
    }
  }
}

#define MANDELBROT_ITERATIONS 200

static float mandelbrot(float complex z) {
  float complex v = 0;

  for (size_t n = 0; n < MANDELBROT_ITERATIONS; ++n) {
    v = v * v + z;
    if (cabsf(v) > 2.0) {
      return (float)n / (float)MANDELBROT_ITERATIONS;
    }
  }

  return 0.0;
}

#define MARGIN_X ((SCREEN_WIDTH - SCREEN_HEIGHT) / 2)

void screen_test_mandelbrot(struct screen *screen, float window_x,
                            float window_y, float window_r, bool (*cancel)()) {

  float x_min = window_x - window_r;
  float y_min = window_y - window_r;
  float x_max = window_x + window_r;
  float y_max = window_y + window_r;

  for (size_t screen_x = MARGIN_X; screen_x < SCREEN_WIDTH - MARGIN_X;
       ++screen_x) {

    float x =
        ((float)screen_x / (float)SCREEN_HEIGHT) * (x_max - x_min) + x_min;

    for (size_t screen_y = 0; screen_y < SCREEN_HEIGHT; ++screen_y) {

      float y =
          ((float)screen_y / (float)SCREEN_HEIGHT) * (y_max - y_min) + y_min;

      color_t c = (color_t)(mandelbrot(x + y * I) * 6.0 * 6.0 * 6.0) + 16;

      screen->buffer[screen_y * SCREEN_WIDTH + screen_x] = c;

      if (cancel && cancel()) {
        return;
      }
    }
  }
}

#define COLOR_TEST_BASE_COLORS 16
#define COLOR_TEST_CUBE_SIZE 6
#define COLOR_TEST_GRAYSCALE 24

#define COLOR_TEST_ROWS (2 + COLOR_TEST_CUBE_SIZE)
#define COLOR_TEST_ROW_HEIGHT (SCREEN_HEIGHT / COLOR_TEST_ROWS)
#define COLOR_TEST_ROW_PADDING 2

#define COLOR_TEST_BASE_COLORS_WIDTH (SCREEN_WIDTH / COLOR_TEST_BASE_COLORS)
#define COLOR_TEST_GRAYSCALE_WIDTH (SCREEN_WIDTH / COLOR_TEST_GRAYSCALE)
#define COLOR_TEST_GRAYSCALE_OFFSET 4
#define COLOR_TEST_CUBE_WIDTH                                                  \
  (SCREEN_WIDTH / (COLOR_TEST_CUBE_SIZE * COLOR_TEST_CUBE_SIZE))

#define COLOR_TEST_CUBE_OFFSET 10

void screen_test_colors(struct screen *screen) {
  size_t y = 0;

  for (size_t i = 0; i < COLOR_TEST_ROW_HEIGHT - COLOR_TEST_ROW_PADDING; ++i) {

    for (size_t j = 0; j < COLOR_TEST_BASE_COLORS; ++j) {
      size_t pos = SCREEN_WIDTH * (y + i) + COLOR_TEST_BASE_COLORS_WIDTH * j;

      memset(screen->buffer + pos, j, COLOR_TEST_BASE_COLORS_WIDTH);
    }
  }

  y += COLOR_TEST_ROW_HEIGHT;

  for (size_t k = 0; k < COLOR_TEST_CUBE_SIZE; ++k) {

    for (size_t i = 0; i < COLOR_TEST_ROW_HEIGHT - COLOR_TEST_ROW_PADDING;
         ++i) {

      for (size_t j = 0; j < COLOR_TEST_CUBE_SIZE * COLOR_TEST_CUBE_SIZE; ++j) {

        size_t pos = COLOR_TEST_CUBE_OFFSET + SCREEN_WIDTH * (y + i) +
                     COLOR_TEST_CUBE_WIDTH * j;

        memset(screen->buffer + pos,
               COLOR_TEST_BASE_COLORS +
                   (k * COLOR_TEST_CUBE_SIZE * COLOR_TEST_CUBE_SIZE) + j,
               COLOR_TEST_CUBE_WIDTH);
      }
    }

    y += COLOR_TEST_ROW_HEIGHT;
  }

  for (size_t i = 0; i < COLOR_TEST_ROW_HEIGHT - COLOR_TEST_ROW_PADDING; ++i) {

    for (size_t j = 0; j < COLOR_TEST_GRAYSCALE; ++j) {

      size_t pos = COLOR_TEST_GRAYSCALE_OFFSET + SCREEN_WIDTH * (y + i) +
                   COLOR_TEST_GRAYSCALE_WIDTH * j;

      memset(screen->buffer + pos,
             COLOR_TEST_BASE_COLORS +
                 (COLOR_TEST_CUBE_SIZE * COLOR_TEST_CUBE_SIZE *
                  COLOR_TEST_CUBE_SIZE) +
                 j,
             COLOR_TEST_GRAYSCALE_WIDTH);
    }
  }
}
