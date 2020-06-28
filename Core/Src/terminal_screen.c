#include "terminal_internal.h"

#define CURSOR_ON_COUNTER 650
#define CURSOR_OFF_COUNTER 350

static void invert_cursor(struct terminal *terminal) {
  terminal->callbacks->screen_draw_cursor(terminal->vs.cursor_row,
                                          terminal->vs.cursor_col, 0xf);
}

static void draw_character(struct terminal *terminal, uint8_t character) {
  color_t active = terminal->vs.negative ? terminal->vs.inactive_color
                                         : terminal->vs.active_color;
  color_t inactive = terminal->vs.negative ? terminal->vs.active_color
                                           : terminal->vs.inactive_color;

  terminal->callbacks->screen_draw_character(
      terminal->vs.cursor_row, terminal->vs.cursor_col, character,
      terminal->vs.font, terminal->vs.italic, terminal->vs.underlined,
      terminal->vs.crossedout, active, inactive);
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

static void clear_rows(struct terminal *terminal, size_t from_row,
                       size_t to_row) {
  clear_cursor(terminal);
  terminal->callbacks->screen_clear_rows(from_row, to_row,
                                         DEFAULT_INACTIVE_COLOR);
}

static void clear_cols(struct terminal *terminal, size_t row, size_t from_col,
                       size_t to_col) {
  clear_cursor(terminal);
  terminal->callbacks->screen_clear_cols(row, from_col, to_col,
                                         DEFAULT_INACTIVE_COLOR);
}

static void screen_scroll(struct terminal *terminal, enum scroll scroll,
                          size_t from_row, size_t rows) {
  if (from_row < terminal->margin_bottom)
    terminal->callbacks->screen_scroll(scroll, from_row,
                                       terminal->margin_bottom, rows,
                                       DEFAULT_INACTIVE_COLOR);
}

static bool inside_margins(struct terminal *terminal) {
  return (terminal->vs.cursor_row >= terminal->margin_top &&
          terminal->vs.cursor_row < terminal->margin_bottom);
}

void terminal_screen_move_cursor_absolute(struct terminal *terminal,
                                          int16_t row, int16_t col) {
  clear_cursor(terminal);

  if (terminal->origin_mode) {
    row = terminal->margin_top + row;

    if (row < terminal->margin_top)
      row = terminal->margin_top;

    if (row >= terminal->margin_bottom)
      row = terminal->margin_bottom - 1;
  } else {
    if (row < 0)
      row = 0;

    if (row >= ROWS)
      row = ROWS - 1;
  }

  if (col < 0)
    col = 0;

  if (col >= COLS)
    col = COLS - 1;

  terminal->vs.cursor_row = row;
  terminal->vs.cursor_col = col;
  terminal->vs.cursor_last_col = false;
}

int16_t terminal_screen_cursor_row(struct terminal *terminal) {
  return terminal->vs.cursor_row -
         (terminal->origin_mode ? terminal->margin_top : 0);
}

int16_t terminal_screen_cursor_col(struct terminal *terminal) {
  return terminal->vs.cursor_col;
}

void terminal_screen_move_cursor(struct terminal *terminal, int16_t rows,
                                 int16_t cols) {
  clear_cursor(terminal);

  int16_t row = terminal->vs.cursor_row + rows;
  int16_t col = terminal->vs.cursor_col + cols;

  if (inside_margins(terminal) || terminal->origin_mode) {
    if (row < terminal->margin_top)
      row = terminal->margin_top;

    if (row >= terminal->margin_bottom)
      row = terminal->margin_bottom - 1;
  } else {
    if (row < 0)
      row = 0;

    if (row >= ROWS)
      row = ROWS - 1;
  }

  if (col < 0)
    col = 0;

  if (col >= COLS)
    col = COLS - 1;

  terminal->vs.cursor_row = row;
  terminal->vs.cursor_col = col;
  terminal->vs.cursor_last_col = false;
}

void terminal_screen_carriage_return(struct terminal *terminal) {
  clear_cursor(terminal);

  terminal->vs.cursor_col = 0;
  terminal->vs.cursor_last_col = false;
}

void terminal_screen_scroll(struct terminal *terminal, enum scroll scroll,
                            size_t from_row, size_t rows) {
  clear_cursor(terminal);

  if (terminal->origin_mode) {
    from_row = terminal->margin_top + from_row;
  }

  screen_scroll(terminal, scroll, from_row, rows);
}

void terminal_screen_clear_to_right(struct terminal *terminal) {
  clear_cols(terminal, terminal->vs.cursor_row, terminal->vs.cursor_col, COLS);
}

void terminal_screen_clear_to_left(struct terminal *terminal) {
  clear_cols(terminal, terminal->vs.cursor_row, 0, terminal->vs.cursor_col + 1);
}

void terminal_screen_clear_to_top(struct terminal *terminal) {
  clear_rows(terminal, 0, terminal->vs.cursor_row);
}

void terminal_screen_clear_row(struct terminal *terminal) {
  clear_cols(terminal, terminal->vs.cursor_row, 0, COLS);
}

void terminal_screen_clear_to_bottom(struct terminal *terminal) {
  clear_rows(terminal, terminal->vs.cursor_row + 1, ROWS);
}

void terminal_screen_clear_all(struct terminal *terminal) {
  clear_rows(terminal, 0, ROWS);
}

void terminal_screen_index(struct terminal *terminal, int16_t rows) {
  clear_cursor(terminal);

  if (inside_margins(terminal)) {
    if (terminal->vs.cursor_row + rows >= terminal->margin_bottom) {
      screen_scroll(
          terminal, SCROLL_UP, terminal->margin_top,
          rows - (terminal->margin_bottom - 1 - terminal->vs.cursor_row));
      terminal->vs.cursor_row = terminal->margin_bottom - 1;
    } else
      terminal->vs.cursor_row += rows;
  } else
    terminal_screen_move_cursor_absolute(
        terminal, terminal->vs.cursor_row + rows, terminal->vs.cursor_col);
}

void terminal_screen_reverse_index(struct terminal *terminal, int16_t rows) {
  clear_cursor(terminal);

  if (inside_margins(terminal)) {
    if (terminal->vs.cursor_row - rows < terminal->margin_top) {
      screen_scroll(terminal, SCROLL_DOWN, terminal->margin_top,
                    rows - (terminal->vs.cursor_row - terminal->margin_top));
      terminal->vs.cursor_row = terminal->margin_top;
    } else
      terminal->vs.cursor_row -= rows;
  } else
    terminal_screen_move_cursor_absolute(
        terminal, terminal->vs.cursor_row - rows, terminal->vs.cursor_col);
}

void terminal_screen_put_character(struct terminal *terminal,
                                   character_t character) {
  clear_cursor(terminal);

  if (terminal->vs.cursor_last_col) {
    terminal_screen_carriage_return(terminal);
    terminal_screen_index(terminal, 1);
  }

  if (terminal->insert_mode)
    terminal_screen_insert(terminal, 1);

  if (!terminal->vs.concealed)
    draw_character(terminal, character);

  if (terminal->vs.cursor_col == COLS - 1) {
    if (terminal->auto_wrap_mode)
      terminal->vs.cursor_last_col = true;
  } else
    terminal->vs.cursor_col++;
}

void terminal_screen_insert(struct terminal *terminal, size_t cols) {
  clear_cursor(terminal);

  while (cols--)
    terminal->callbacks->screen_shift_characters_right(terminal->vs.cursor_row,
                                                       terminal->vs.cursor_col,
                                                       DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_delete(struct terminal *terminal, size_t cols) {
  clear_cursor(terminal);

  while (cols--)
    terminal->callbacks->screen_shift_characters_left(terminal->vs.cursor_row,
                                                      terminal->vs.cursor_col,
                                                      DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_erase(struct terminal *terminal, size_t cols) {
  clear_cursor(terminal);

  terminal->callbacks->screen_clear_cols(
      terminal->vs.cursor_row, terminal->vs.cursor_col,
      terminal->vs.cursor_col + cols, DEFAULT_INACTIVE_COLOR);
}

void terminal_screen_enable_cursor(struct terminal *terminal, bool enable) {
  clear_cursor(terminal);

  if (enable) {
    terminal->cursor_counter = CURSOR_ON_COUNTER;
    terminal->cursor_on = true;
  } else {
    terminal->cursor_counter = 0;
    terminal->cursor_on = false;
  }

  terminal->cursor_inverted = false;
}

void terminal_screen_save_visual_state(struct terminal *terminal) {
  clear_cursor(terminal);

  terminal->saved_vs = terminal->vs;
}

void terminal_screen_restore_visual_state(struct terminal *terminal) {
  clear_cursor(terminal);

  terminal->vs = terminal->saved_vs;

  if (terminal->origin_mode) {
    if (terminal->vs.cursor_row < terminal->margin_top)
      terminal->vs.cursor_row = terminal->margin_top;

    if (terminal->vs.cursor_row >= terminal->margin_bottom)
      terminal->vs.cursor_row = terminal->margin_bottom - 1;
  }
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
  terminal->auto_wrap_mode = true;
  terminal->scrolling_mode = false;
  terminal->column_mode = false;
  terminal->screen_mode = false;
  terminal->origin_mode = false;
  terminal->insert_mode = false;

  terminal->vs.cursor_row = 0;
  terminal->vs.cursor_col = 0;
  terminal->vs.cursor_last_col = false;

  terminal->vs.font = FONT_NORMAL;
  terminal->vs.italic = false;
  terminal->vs.underlined = false;
  terminal->vs.blink = NO_BLINK;
  terminal->vs.negative = false;
  terminal->vs.concealed = false;
  terminal->vs.crossedout = false;
  terminal->vs.active_color = DEFAULT_ACTIVE_COLOR;
  terminal->vs.inactive_color = DEFAULT_INACTIVE_COLOR;

  terminal->margin_top = 0;
  terminal->margin_bottom = ROWS;

  for (size_t i = 0; i < COLS; ++i)
    terminal->tab_stops[i] = ((i + 1) % 8) == 0;

  terminal->cursor_counter = CURSOR_ON_COUNTER;
  terminal->cursor_on = true;
  terminal->cursor_inverted = false;

  terminal_screen_clear_all(terminal);
}
