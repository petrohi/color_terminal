#include "terminal_internal.h"

#define CURSOR_ON_COUNTER 650
#define CURSOR_OFF_COUNTER 350

static void invert_cursor(struct terminal *terminal) {
  terminal->callbacks->screen_draw_cursor(terminal->cursor_row,
                                          terminal->cursor_col, 0xf);
}

static void clear_cursor(struct terminal *terminal) {
  if (terminal->cursor_counter) {
    if (terminal->cursor_inverted) {
      invert_cursor(terminal);
    }
    terminal->cursor_counter = CURSOR_ON_COUNTER;
    terminal->cursor_on = true;
    terminal->cursor_inverted = false;
  }
}

void terminal_screen_move_cursor(struct terminal *terminal, int16_t row,
                                 int16_t col) {
  clear_cursor(terminal);

  terminal->cursor_col = col;
  terminal->cursor_row = row;
  terminal->cursor_last_col = false;

  if (terminal->cursor_col < 0)
    terminal->cursor_col = 0;

  if (terminal->cursor_row < 0)
    terminal->cursor_row = 0;

  if (terminal->cursor_col >= COLS)
    terminal->cursor_col = COLS - 1;

  if (terminal->cursor_row >= ROWS)
    terminal->cursor_row = ROWS - 1;
}

void terminal_screen_carriage_return(struct terminal *terminal) {
  clear_cursor(terminal);

  terminal->cursor_col = 0;
  terminal->cursor_last_col = false;
}

void terminal_screen_scroll(struct terminal *terminal, enum scroll scroll,
                            size_t from_row, size_t to_row, size_t rows) {
  clear_cursor(terminal);
  terminal->callbacks->screen_scroll(scroll, from_row, to_row, rows,
                                     DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_clear_rows(struct terminal *terminal, size_t from_row,
                                size_t to_row) {
  clear_cursor(terminal);
  terminal->callbacks->screen_clear_rows(from_row, to_row,
                                         DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_clear_cols(struct terminal *terminal, size_t row,
                                size_t from_col, size_t to_col) {
  clear_cursor(terminal);
  terminal->callbacks->screen_clear_cols(row, from_col, to_col,
                                         DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_line_feed(struct terminal *terminal, int16_t rows) {
  clear_cursor(terminal);

  if (terminal->cursor_row >= ROWS - rows) {
    terminal_screen_scroll(terminal, SCROLL_UP, 0, ROWS,
                           rows - (ROWS - 1 - terminal->cursor_row));
    terminal->cursor_row = ROWS - 1;
  } else
    terminal->cursor_row += rows;
}

void terminal_screen_reverse_line_feed(struct terminal *terminal,
                                       int16_t rows) {
  clear_cursor(terminal);

  if (terminal->cursor_row < rows) {
    terminal_screen_scroll(terminal, SCROLL_DOWN, 0, ROWS,
                           rows - terminal->cursor_row);
    terminal->cursor_row = 0;
  } else
    terminal->cursor_row -= rows;
}

void terminal_screen_put_character(struct terminal *terminal,
                                   character_t character) {
  clear_cursor(terminal);

  if (terminal->cursor_last_col) {
    terminal_screen_carriage_return(terminal);
    terminal_screen_line_feed(terminal, 1);
  }

  color_t active =
      terminal->negative ? terminal->inactive_color : terminal->active_color;
  color_t inactive =
      terminal->negative ? terminal->active_color : terminal->inactive_color;

  if (!terminal->concealed)
    terminal->callbacks->screen_draw_character(
        terminal->cursor_row, terminal->cursor_col, character, terminal->font,
        terminal->italic, terminal->underlined, terminal->crossedout, active,
        inactive);

  if (terminal->cursor_col == COLS - 1)
    terminal->cursor_last_col = true;
  else
    terminal->cursor_col++;
}

void terminal_screen_enable_cursor(struct terminal *terminal, bool enable) {
  clear_cursor(terminal);

  if (enable) {
    terminal->cursor_counter = CURSOR_ON_COUNTER;
    terminal->cursor_on = true;
  }
  else {
    terminal->cursor_counter = 0;
    terminal->cursor_on = false;
  }

  terminal->cursor_inverted = false;
}

void terminal_screen_update_cursor_counter(struct terminal *terminal) {
  if (terminal->cursor_counter) {
    terminal->cursor_counter--;

    if (!terminal->cursor_counter) {
      if (terminal->cursor_on) {
        terminal->cursor_on = false;
        terminal->cursor_counter = CURSOR_OFF_COUNTER;
      } else {
        terminal->cursor_on = true;
        terminal->cursor_counter = CURSOR_ON_COUNTER;
      }
    }
  }
}

void terminal_screen_update_cursor(struct terminal *terminal) {
  if (terminal->cursor_on != terminal->cursor_inverted) {
    terminal->cursor_inverted = terminal->cursor_on;
    invert_cursor(terminal);
  }
}

void terminal_screen_init(struct terminal *terminal) {
  terminal->cursor_row = 0;
  terminal->cursor_col = 0;
  terminal->cursor_last_col = false;

  terminal->font = FONT_NORMAL;
  terminal->italic = false;
  terminal->underlined = false;
  terminal->blink = NO_BLINK;
  terminal->negative = false;
  terminal->concealed = false;
  terminal->crossedout = false;
  terminal->active_color = DEFAULT_ACTIVE_COLOR;
  terminal->inactive_color = DEFAULT_INACTIVE_COLOR;

  terminal->cursor_counter = CURSOR_ON_COUNTER;
  terminal->cursor_on = true;
  terminal->cursor_inverted = false;

  terminal_screen_clear_rows(terminal, 0, ROWS);
}
