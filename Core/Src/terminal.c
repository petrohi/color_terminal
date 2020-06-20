#include "terminal.h"

#include <memory.h>
#include <stdio.h>
#include <stdarg.h>

#include "usbh_hid_keybd.h"

#define CURSOR_ON_COUNTER 650
#define CURSOR_OFF_COUNTER 350

#define FIRST_REPEAT_COUNTER 500
#define NEXT_REPEAT_COUNTER 33

#define PRODUCT_NAME                                                           \
  "\33[1;91mA\33[92mS\33[93mC\33[94mI\33[95mI\33[39m Terminal\33[m\r\n"
#define PRODUCT_VERSION "Version 3.0-alpha\r\n"
#define PRODUCT_COPYRIGHT "Copyright (C) 2020 Peter Hizalev\r\n"

enum keys_entry_type {
  IGNORE,
  CHR,
  STR,
  HANDLER,
  ROUTER,
};

struct keys_entry;

struct keys_router {
  size_t (*router)(struct terminal *);
  struct keys_entry *entries;
};

union keys_variant {
  character_t character;
  const char *string;
  void (*handler)(struct terminal *);
  struct keys_router *router;
};

struct keys_entry {
  enum keys_entry_type type;
  union keys_variant data;
};

static void transmit_character(struct terminal *terminal,
                               character_t character) {
  memcpy(terminal->transmit_buffer, &character, 1);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, 1);
}

static void transmit_string(struct terminal *terminal, const char *string) {
  size_t len = strlen(string);
  memcpy(terminal->transmit_buffer, string, len);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, len);
}

#define PRINTF_BUFFER_SIZE 64

static int transmit_printf(struct terminal *terminal, const char *format, ...) {
  va_list args;
  va_start(args, format);

  char buffer[PRINTF_BUFFER_SIZE];
  int len = snprintf(buffer, PRINTF_BUFFER_SIZE, format, args);

  va_end(args);
  return len;
}

static size_t get_case(struct terminal *terminal) {
  return terminal->lock_state.caps ^ terminal->shift_state;
}

static size_t get_shift(struct terminal *terminal) {
  return terminal->shift_state;
}

static void update_keyboard_leds(struct terminal *terminal) {
  terminal->callbacks->keyboard_set_leds(terminal->lock_state);
}

static void handle_caps_lock(struct terminal *terminal) {
  terminal->lock_state.caps ^= 1;
  update_keyboard_leds(terminal);
}

static void handle_enter(struct terminal *terminal) {
  transmit_character(terminal, '\r');
  //receive_string("\r\n");
}

#define KEY_IGNORE                                                             \
  { IGNORE }
#define KEY_CHR(c)                                                             \
  {                                                                            \
    CHR, { .character = c }                                                    \
  }
#define KEY_STR(s)                                                             \
  {                                                                            \
    STR, { .string = s }                                                       \
  }
#define KEY_HANDLER(h)                                                         \
  {                                                                            \
    HANDLER, { .handler = h }                                                  \
  }
#define KEY_ROUTER(r, ...)                                                     \
  {                                                                            \
    ROUTER, {                                                                  \
      .router = &(struct keys_router) {                                        \
        r, (struct keys_entry[]) { __VA_ARGS__ }                               \
      }                                                                        \
    }                                                                          \
  }

static const struct keys_entry *entries = (struct keys_entry[]){
    KEY_IGNORE,                                        // NONE
    KEY_IGNORE,                                        // ERRORROLLOVER
    KEY_IGNORE,                                        // POSTFAIL
    KEY_IGNORE,                                        // ERRORUNDEFINED
    KEY_ROUTER(get_case, KEY_CHR('a'), KEY_CHR('A')),  // A
    KEY_ROUTER(get_case, KEY_CHR('b'), KEY_CHR('B')),  // B
    KEY_ROUTER(get_case, KEY_CHR('c'), KEY_CHR('C')),  // C
    KEY_ROUTER(get_case, KEY_CHR('d'), KEY_CHR('D')),  // D
    KEY_ROUTER(get_case, KEY_CHR('e'), KEY_CHR('E')),  // E
    KEY_ROUTER(get_case, KEY_CHR('f'), KEY_CHR('F')),  // F
    KEY_ROUTER(get_case, KEY_CHR('g'), KEY_CHR('G')),  // G
    KEY_ROUTER(get_case, KEY_CHR('h'), KEY_CHR('H')),  // H
    KEY_ROUTER(get_case, KEY_CHR('i'), KEY_CHR('I')),  // I
    KEY_ROUTER(get_case, KEY_CHR('j'), KEY_CHR('J')),  // J
    KEY_ROUTER(get_case, KEY_CHR('k'), KEY_CHR('K')),  // K
    KEY_ROUTER(get_case, KEY_CHR('l'), KEY_CHR('L')),  // L
    KEY_ROUTER(get_case, KEY_CHR('m'), KEY_CHR('M')),  // M
    KEY_ROUTER(get_case, KEY_CHR('n'), KEY_CHR('N')),  // N
    KEY_ROUTER(get_case, KEY_CHR('o'), KEY_CHR('O')),  // O
    KEY_ROUTER(get_case, KEY_CHR('p'), KEY_CHR('P')),  // P
    KEY_ROUTER(get_case, KEY_CHR('q'), KEY_CHR('Q')),  // Q
    KEY_ROUTER(get_case, KEY_CHR('r'), KEY_CHR('R')),  // R
    KEY_ROUTER(get_case, KEY_CHR('s'), KEY_CHR('S')),  // S
    KEY_ROUTER(get_case, KEY_CHR('t'), KEY_CHR('T')),  // T
    KEY_ROUTER(get_case, KEY_CHR('u'), KEY_CHR('U')),  // U
    KEY_ROUTER(get_case, KEY_CHR('v'), KEY_CHR('V')),  // V
    KEY_ROUTER(get_case, KEY_CHR('w'), KEY_CHR('W')),  // W
    KEY_ROUTER(get_case, KEY_CHR('x'), KEY_CHR('X')),  // X
    KEY_ROUTER(get_case, KEY_CHR('y'), KEY_CHR('Y')),  // Y
    KEY_ROUTER(get_case, KEY_CHR('z'), KEY_CHR('Z')),  // Z
    KEY_ROUTER(get_shift, KEY_CHR('1'), KEY_CHR('!')), // 1_EXCLAMATION_MARK
    KEY_ROUTER(get_shift, KEY_CHR('2'), KEY_CHR('@')), // 2_AT
    KEY_ROUTER(get_shift, KEY_CHR('3'), KEY_CHR('#')), // 3_NUMBER_SIGN
    KEY_ROUTER(get_shift, KEY_CHR('4'), KEY_CHR('$')), // 4_DOLLAR
    KEY_ROUTER(get_shift, KEY_CHR('5'), KEY_CHR('%')), // 5_PERCENT
    KEY_ROUTER(get_shift, KEY_CHR('6'), KEY_CHR('^')), // 6_CARET
    KEY_ROUTER(get_shift, KEY_CHR('7'), KEY_CHR('&')), // 7_AMPERSAND
    KEY_ROUTER(get_shift, KEY_CHR('8'), KEY_CHR('*')), // 8_ASTERISK
    KEY_ROUTER(get_shift, KEY_CHR('9'), KEY_CHR('(')), // 9_OPARENTHESIS
    KEY_ROUTER(get_shift, KEY_CHR('0'), KEY_CHR(')')), // 0_CPARENTHESIS
    KEY_HANDLER(handle_enter),                         // ENTER
    KEY_CHR('\33'),                                    // ESCAPE
    KEY_CHR('\10'),                                    // BACKSPACE
    KEY_CHR('\11'),                                    // TAB
    KEY_CHR(' '),                                      // SPACEBAR
    KEY_ROUTER(get_shift, KEY_CHR('-'), KEY_CHR('_')), // MINUS_UNDERSCORE
    KEY_ROUTER(get_shift, KEY_CHR('='), KEY_CHR('+')), // EQUAL_PLUS
    KEY_ROUTER(get_shift, KEY_CHR('['), KEY_CHR('{')), // OBRACKET_AND_OBRACE
    KEY_ROUTER(get_shift, KEY_CHR(']'), KEY_CHR('}')), // CBRACKET_AND_CBRACE
    KEY_ROUTER(get_shift, KEY_CHR('\\'),
               KEY_CHR('|')), // BACKSLASH_VERTICAL_BAR
    KEY_IGNORE,               // NONUS_NUMBER_SIGN_TILDE
    KEY_ROUTER(get_shift, KEY_CHR(';'), KEY_CHR(':')), // SEMICOLON_COLON
    KEY_ROUTER(get_shift, KEY_CHR('\''),
               KEY_CHR('"')), // SINGLE_AND_DOUBLE_QUOTE
    KEY_ROUTER(get_shift, KEY_CHR('`'), KEY_CHR('~')), // GRAVE_ACCENT_AND_TILDE
    KEY_ROUTER(get_shift, KEY_CHR(','), KEY_CHR('<')), // COMMA_AND_LESS
    KEY_ROUTER(get_shift, KEY_CHR('.'), KEY_CHR('>')), // DOT_GREATER
    KEY_ROUTER(get_shift, KEY_CHR('/'), KEY_CHR('?')), // SLASH_QUESTION
    KEY_HANDLER(handle_caps_lock),                     // CAPS_LOCK
    KEY_STR("\033[11~"),                               // F1
    KEY_STR("\033[12~"),                               // F2
    KEY_STR("\033[13~"),                               // F3
    KEY_STR("\033[14~"),                               // F4
    KEY_STR("\033[15~"),                               // F5
    KEY_STR("\033[17~"),                               // F6
    KEY_STR("\033[18~"),                               // F7
    KEY_STR("\033[19~"),                               // F8
    KEY_STR("\033[20~"),                               // F9
    KEY_STR("\033[21~"),                               // F10
    KEY_STR("\033[23~"),                               // F11
    KEY_STR("\033[24~"),                               // F12
    KEY_IGNORE,                                        // PRINTSCREEN
    KEY_IGNORE,                                        // SCROLL_LOCK
    KEY_IGNORE,                                        // PAUSE
    KEY_IGNORE,                                        // INSERT
    KEY_IGNORE,                                        // HOME
    KEY_IGNORE,                                        // PAGEUP
    KEY_IGNORE,                                        // DELETE
    KEY_IGNORE,                                        // END1
    KEY_IGNORE,                                        // PAGEDOWN
    KEY_STR("\033[C"),                                 // RIGHTARROW
    KEY_STR("\033[D"),                                 // LEFTARROW
    KEY_STR("\033[B"),                                 // DOWNARROW
    KEY_STR("\033[A"),                                 // UPARROW
    KEY_IGNORE, // KEYPAD_NUM_LOCK_AND_CLEAR
    KEY_IGNORE, // KEYPAD_SLASH
    KEY_IGNORE, // KEYPAD_ASTERIKS
    KEY_IGNORE, // KEYPAD_MINUS
    KEY_IGNORE, // KEYPAD_PLUS
    KEY_IGNORE, // KEYPAD_ENTER
    KEY_IGNORE, // KEYPAD_1_END
    KEY_IGNORE, // KEYPAD_2_DOWN_ARROW
    KEY_IGNORE, // KEYPAD_3_PAGEDN
    KEY_IGNORE, // KEYPAD_4_LEFT_ARROW
    KEY_IGNORE, // KEYPAD_5
    KEY_IGNORE, // KEYPAD_6_RIGHT_ARROW
    KEY_IGNORE, // KEYPAD_7_HOME
    KEY_IGNORE, // KEYPAD_8_UP_ARROW
    KEY_IGNORE, // KEYPAD_9_PAGEUP
    KEY_IGNORE, // KEYPAD_0_INSERT
    KEY_IGNORE, // KEYPAD_DECIMAL_SEPARATOR_DELETE
};

static void handle_key(struct terminal *terminal,
                       const struct keys_entry *key) {
  struct keys_router *router;

  switch (key->type) {
  case IGNORE:
    break;
  case CHR:
    transmit_character(terminal, key->data.character);
    break;
  case STR:
    transmit_string(terminal, key->data.string);
    break;
  case HANDLER:
    key->data.handler(terminal);
    break;
  case ROUTER:
    router = key->data.router;
    handle_key(terminal, &router->entries[router->router(terminal)]);
    break;
  }
}

void terminal_handle_key(struct terminal *terminal, uint8_t key_code) {
  if (terminal->pressed_key_code == key_code)
    return;

  terminal->pressed_key_code = key_code;
  terminal->repeat_pressed_key = false;

  if (key_code != KEY_NONE && key_code != KEY_ESCAPE && key_code != KEY_TAB &&
      key_code != KEY_RETURN && key_code != KEY_CAPS_LOCK &&
      key_code != KEY_KEYPAD_NUM_LOCK_AND_CLEAR &&
      key_code != KEY_SCROLL_LOCK && !terminal->ctrl_state) {
    terminal->repeat_counter = FIRST_REPEAT_COUNTER;
  } else {
    terminal->repeat_counter = 0;
  }

  handle_key(terminal, &entries[key_code]);
}

void terminal_handle_shift(struct terminal *terminal, bool shift) {
  terminal->shift_state = shift;
}

void terminal_handle_alt(struct terminal *terminal, bool alt) {
  terminal->alt_state = alt;
}

void terminal_handle_ctrl(struct terminal *terminal, bool ctrl) {
  terminal->ctrl_state = ctrl;
}

static void invert_cursor(struct terminal *terminal) {
  terminal->callbacks->screen_draw_cursor(terminal->cursor_row,
                                          terminal->cursor_col, 0xf);
}

static void clear_cursor(struct terminal *terminal) {
  if (terminal->cursor_inverted) {
    invert_cursor(terminal);
  }
  terminal->cursor_counter = CURSOR_ON_COUNTER;
  terminal->cursor_on = true;
  terminal->cursor_inverted = false;
}

static void move_cursor(struct terminal *terminal, int16_t row, int16_t col) {
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

static void carriage_return(struct terminal *terminal) {
  clear_cursor(terminal);

  terminal->cursor_col = 0;
  terminal->cursor_last_col = false;
}

static void scroll(struct terminal *terminal, enum scroll scroll,
                   size_t from_row, size_t to_row, size_t rows) {
  clear_cursor(terminal);
  terminal->callbacks->screen_scroll(scroll, from_row, to_row, rows,
                                     DEFAULT_INACTIVE_COLOR);
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

static void line_feed(struct terminal *terminal, int16_t rows) {
  clear_cursor(terminal);

  if (terminal->cursor_row >= ROWS - rows) {
    scroll(terminal, SCROLL_UP, 0, ROWS,
           rows - (ROWS - 1 - terminal->cursor_row));
    terminal->cursor_row = ROWS - 1;
  } else
    terminal->cursor_row += rows;
}

static void reverse_line_feed(struct terminal *terminal, int16_t rows) {
  clear_cursor(terminal);

  if (terminal->cursor_row < rows) {
    scroll(terminal, SCROLL_DOWN, 0, ROWS, rows - terminal->cursor_row);
    terminal->cursor_row = 0;
  } else
    terminal->cursor_row -= rows;
}

static void put_character(struct terminal *terminal, character_t character) {
  clear_cursor(terminal);

  if (terminal->cursor_last_col) {
    carriage_return(terminal);
    line_feed(terminal, 1);
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

static void clear_csi_params(struct terminal *terminal) {
  memset(terminal->csi_params, 0, CSI_MAX_PARAMS_COUNT * CSI_MAX_PARAM_LENGTH);
  terminal->csi_params_count = 0;
  terminal->csi_last_param_length = 0;
}

static uint16_t get_csi_param(struct terminal *terminal, size_t index) {
  return atoi((const char *)terminal->csi_params[index]);
}

static const receive_table_t default_receive_table;

static void clear_receive_table(struct terminal *terminal) {
  terminal->receive_table = &default_receive_table;
}

static void receive_unexpected(struct terminal *terminal,
                               character_t character) {
  clear_receive_table(terminal);
}

static const receive_table_t esc_receive_table;

static void receive_esc(struct terminal *terminal, character_t character) {
  terminal->receive_table = &esc_receive_table;
}

static void receive_cr(struct terminal *terminal, character_t character) {
  carriage_return(terminal);
}

static void receive_lf(struct terminal *terminal, character_t character) {
  line_feed(terminal, 1);
}

static const receive_table_t csi_receive_table;

static void receive_csi(struct terminal *terminal, character_t character) {
  terminal->receive_table = &csi_receive_table;
  clear_csi_params(terminal);
}

static const receive_table_t hash_receive_table;

static void receive_hash(struct terminal *terminal, character_t character) {
  terminal->receive_table = &hash_receive_table;
}

static void receive_ris(struct terminal *terminal, character_t character) {
  HAL_NVIC_SystemReset();
}

static void receive_csi_param(struct terminal *terminal,
                              character_t character) {
  if (!terminal->csi_last_param_length) {
    if (terminal->csi_params_count == CSI_MAX_PARAMS_COUNT)
      return;

    terminal->csi_params_count++;
  }

  // Keep zero for the end of the string for atoi
  if (terminal->csi_last_param_length == CSI_MAX_PARAM_LENGTH - 1)
    return;

  terminal->csi_last_param_length++;
  terminal->csi_params[terminal->csi_params_count - 1]
                      [terminal->csi_last_param_length - 1] = character;
}

static void receive_csi_param_delimiter(struct terminal *terminal,
                                        character_t character) {
  if (!terminal->csi_last_param_length) {
    if (terminal->csi_params_count == CSI_MAX_PARAMS_COUNT)
      return;

    terminal->csi_params_count++;
    return;
  }

  terminal->csi_last_param_length = 0;
}

static void receive_da(struct terminal *terminal, character_t character) {
  transmit_string(terminal, "\33[?1;0c");
  clear_receive_table(terminal);
}

static void receive_hvp(struct terminal *terminal, character_t character) {
  int16_t row = get_csi_param(terminal, 0);
  int16_t col = get_csi_param(terminal, 1);

  move_cursor(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static void receive_dsr(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 5:
    transmit_string(terminal, "\33[0n");
    break;

  case 6:
    transmit_printf(terminal, "\33[%d;%dR", terminal->cursor_row + 1,
                    terminal->cursor_col + 1);
    break;
  }

  clear_receive_table(terminal);
}

static void receive_dectst(struct terminal *terminal, character_t character) {
  clear_receive_table(terminal);
}

static void receive_cup(struct terminal *terminal, character_t character) {
  int16_t row = get_csi_param(terminal, 0);
  int16_t col = get_csi_param(terminal, 1);

  move_cursor(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static color_t get_sgr_color(struct terminal *terminal, size_t *i) {
  uint16_t code = get_csi_param(terminal, (*i)++);

  if (code == 5) {
    return get_csi_param(terminal, (*i)++);
  } else if (code == 2) {
    (*i) += 3;
  }
  return DEFAULT_ACTIVE_COLOR;
}

static void handle_sgr(struct terminal *terminal, size_t *i) {
  uint16_t code = get_csi_param(terminal, (*i)++);

  switch (code) {
  case 0:
    terminal->font = FONT_NORMAL;
    terminal->italic = false;
    terminal->underlined = false;
    terminal->blink = NO_BLINK;
    terminal->negative = false;
    terminal->concealed = false;
    terminal->crossedout = false;
    terminal->active_color = DEFAULT_ACTIVE_COLOR;
    terminal->inactive_color = DEFAULT_INACTIVE_COLOR;
    break;

  case 1:
    terminal->font = FONT_BOLD;
    break;

  case 2:
    terminal->font = FONT_THIN;
    break;

  case 3:
    terminal->italic = true;
    break;

  case 4:
    terminal->underlined = true;
    break;

  case 5:
    terminal->blink = SLOW_BLINK;
    break;

  case 6:
    terminal->blink = RAPID_BLINK;
    break;

  case 7:
    terminal->negative = true;
    break;

  case 8:
    terminal->concealed = true;
    break;

  case 9:
    terminal->crossedout = true;
    break;

  case 10:
  case 21:
  case 22:
  case 23:
    terminal->font = FONT_NORMAL;
    break;

  case 24:
    terminal->underlined = false;
    break;

  case 25:
    terminal->blink = NO_BLINK;
    break;

  case 27:
    terminal->negative = true;
    break;

  case 28:
    terminal->concealed = false;
    break;

  case 29:
    terminal->crossedout = false;
    break;

  case 38:
    terminal->active_color = get_sgr_color(terminal, i);
    break;

  case 39:
    terminal->active_color = DEFAULT_ACTIVE_COLOR;
    break;

  case 48:
    terminal->inactive_color = get_sgr_color(terminal, i);
    break;

  case 49:
    terminal->inactive_color = DEFAULT_INACTIVE_COLOR;
    break;
  }

  if (code >= 30 && code < 38)
    terminal->active_color = code - 30;

  if (code >= 40 && code < 48)
    terminal->inactive_color = code - 40;

  if (code >= 90 && code < 98)
    terminal->active_color = code - 90 + 8;

  if (code >= 100 && code < 108)
    terminal->inactive_color = code - 100 + 8;
}

static void receive_sgr(struct terminal *terminal, character_t character) {
  size_t i = 0;
  if (terminal->csi_params_count) {
    while (i < terminal->csi_params_count)
      handle_sgr(terminal, &i);
  } else
    handle_sgr(terminal, &i);

  clear_receive_table(terminal);
}

static void receive_cuu(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  move_cursor(terminal, terminal->cursor_row - rows, terminal->cursor_col);
  clear_receive_table(terminal);
}

static void receive_cud(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  move_cursor(terminal, terminal->cursor_row + rows, terminal->cursor_col);
  clear_receive_table(terminal);
}

static void receive_cuf(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  move_cursor(terminal, terminal->cursor_row, terminal->cursor_col + cols);
  clear_receive_table(terminal);
}

static void receive_cub(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  move_cursor(terminal, terminal->cursor_row, terminal->cursor_col - cols);
  clear_receive_table(terminal);
}

static void receive_cnl(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  carriage_return(terminal);
  line_feed(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cpl(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  carriage_return(terminal);
  reverse_line_feed(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cha(struct terminal *terminal, character_t character) {
  int16_t col = get_csi_param(terminal, 0);

  move_cursor(terminal, terminal->cursor_row, col - 1);
  clear_receive_table(terminal);
}

static void receive_sd(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  scroll(terminal, SCROLL_DOWN, 0, ROWS, rows);
  clear_receive_table(terminal);
}

static void receive_su(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  scroll(terminal, SCROLL_UP, 0, ROWS, rows);
  clear_receive_table(terminal);
}

static void receive_ed(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 0:
    if (terminal->cursor_col != COLS - 1)
      clear_cols(terminal, terminal->cursor_row, terminal->cursor_col + 1,
                 COLS);

    if (terminal->cursor_row != ROWS - 1)
      clear_rows(terminal, terminal->cursor_row + 1, ROWS);

    break;

  case 1:
    if (terminal->cursor_col)
      clear_cols(terminal, terminal->cursor_row, 0, terminal->cursor_col);

    if (terminal->cursor_row)
      clear_rows(terminal, 0, terminal->cursor_row);

    break;

  case 2:
  case 3:
    clear_rows(terminal, 0, ROWS);

    break;
  }

  clear_receive_table(terminal);
}

static void receive_el(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 0:
    if (terminal->cursor_col != COLS - 1)
      clear_cols(terminal, terminal->cursor_row, terminal->cursor_col + 1,
                 COLS);
    break;

  case 1:
    if (terminal->cursor_col)
      clear_cols(terminal, terminal->cursor_row, 0, terminal->cursor_col);

    break;

  case 2:
    clear_cols(terminal, terminal->cursor_row, 0, COLS);

    break;
  }

  clear_receive_table(terminal);
}

static void receive_decaln(struct terminal *terminal, character_t character) {
  for (size_t row = 0; row < ROWS; ++row)
    for (size_t col = 0; col < COLS; ++col) {
      move_cursor(terminal, row, col);
      put_character(terminal, 'E');
    }

  move_cursor(terminal, 0, 0);
  clear_receive_table(terminal);
}

static void receive_printable(struct terminal *terminal,
                              character_t character) {
  put_character(terminal, character);
}

static void receive_character(struct terminal *terminal,
                              character_t character) {
  receive_t receive = (*terminal->receive_table)[character];

  if (!receive) {
    receive = (*terminal->receive_table)[DEFAULT_RECEIVE];
  }

  receive(terminal, character);
}

#define RECEIVE_HANDLER(c, h) [c] = h
#define DEFAULT_RECEIVE_HANDLER(h) [DEFAULT_RECEIVE] = h

#define DEFAULT_RECEIVE_TABLE(h)                                               \
  RECEIVE_HANDLER('\15', receive_cr), RECEIVE_HANDLER('\12', receive_lf),      \
      RECEIVE_HANDLER('\33', receive_esc), DEFAULT_RECEIVE_HANDLER(h)

static const receive_table_t default_receive_table = {
    DEFAULT_RECEIVE_TABLE(receive_printable),
};

static const receive_table_t esc_receive_table = {
    DEFAULT_RECEIVE_TABLE(receive_unexpected),
    RECEIVE_HANDLER('[', receive_csi),
    RECEIVE_HANDLER('#', receive_hash),
    RECEIVE_HANDLER('c', receive_ris),
};

static const receive_table_t csi_receive_table = {
    DEFAULT_RECEIVE_TABLE(receive_unexpected),
    RECEIVE_HANDLER('0', receive_csi_param),
    RECEIVE_HANDLER('1', receive_csi_param),
    RECEIVE_HANDLER('2', receive_csi_param),
    RECEIVE_HANDLER('3', receive_csi_param),
    RECEIVE_HANDLER('4', receive_csi_param),
    RECEIVE_HANDLER('5', receive_csi_param),
    RECEIVE_HANDLER('6', receive_csi_param),
    RECEIVE_HANDLER('7', receive_csi_param),
    RECEIVE_HANDLER('8', receive_csi_param),
    RECEIVE_HANDLER('9', receive_csi_param),
    RECEIVE_HANDLER(';', receive_csi_param_delimiter),
    RECEIVE_HANDLER('c', receive_da),
    RECEIVE_HANDLER('f', receive_hvp),
    RECEIVE_HANDLER('n', receive_dsr),
    RECEIVE_HANDLER('m', receive_sgr),
    RECEIVE_HANDLER('y', receive_dectst),
    RECEIVE_HANDLER('A', receive_cuu),
    RECEIVE_HANDLER('B', receive_cud),
    RECEIVE_HANDLER('C', receive_cuf),
    RECEIVE_HANDLER('D', receive_cub),
    RECEIVE_HANDLER('E', receive_cnl),
    RECEIVE_HANDLER('F', receive_cpl),
    RECEIVE_HANDLER('G', receive_cha),
    RECEIVE_HANDLER('H', receive_cup),
    RECEIVE_HANDLER('J', receive_ed),
    RECEIVE_HANDLER('K', receive_el),
    RECEIVE_HANDLER('S', receive_su),
    RECEIVE_HANDLER('T', receive_sd),
};

static const receive_table_t hash_receive_table = {
    DEFAULT_RECEIVE_TABLE(receive_unexpected),
    RECEIVE_HANDLER('8', receive_decaln),
};

static void receive_string(struct terminal *terminal, const char *string) {
  while (*string) {
    receive_character(terminal, *string);
    string++;
  }
}

void terminal_uart_receive(struct terminal *terminal, uint32_t count) {
  if (terminal->uart_receive_count == count)
    return;

  uint32_t i = terminal->uart_receive_count;

  while (i != count) {
    character_t character = terminal->receive_buffer[RECEIVE_BUFFER_SIZE - i];
    receive_character(terminal, character);
    i--;

    if (i == 0)
      i = RECEIVE_BUFFER_SIZE;
  }

  terminal->uart_receive_count = count;
}

static void update_repeat_counter(struct terminal *terminal) {
  if (terminal->repeat_counter) {
    terminal->repeat_counter--;

    if (!terminal->repeat_counter && !terminal->repeat_pressed_key)
      terminal->repeat_pressed_key = true;
  }
}

static void update_cursor_counter(struct terminal *terminal) {
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

void terminal_timer_tick(struct terminal *terminal) {
  update_repeat_counter(terminal);
  update_cursor_counter(terminal);
}

void terminal_update_cursor(struct terminal *terminal) {
  if (terminal->cursor_on != terminal->cursor_inverted) {
    terminal->cursor_inverted = terminal->cursor_on;
    invert_cursor(terminal);
  }
}

void terminal_repeat_key(struct terminal *terminal) {
  if (terminal->repeat_pressed_key) {
    terminal->repeat_counter = NEXT_REPEAT_COUNTER;
    terminal->repeat_pressed_key = false;

    handle_key(terminal, &entries[terminal->pressed_key_code]);
  }
}

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks) {
  terminal->callbacks = callbacks;

  terminal->pressed_key_code = 0;
  terminal->repeat_counter = 0;
  terminal->repeat_pressed_key = false;

  terminal->lock_state.caps = 0;
  terminal->lock_state.scroll = 0;
  terminal->lock_state.num = 0;

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

  terminal->receive_table = &default_receive_table;

  memset(terminal->csi_params, 0, CSI_MAX_PARAMS_COUNT * CSI_MAX_PARAM_LENGTH);
  terminal->csi_params_count = 0;
  terminal->csi_last_param_length = 0;

  terminal->uart_receive_count = RECEIVE_BUFFER_SIZE;
  terminal->callbacks->uart_receive(terminal->receive_buffer,
                                    sizeof(terminal->receive_buffer));

  clear_rows(terminal, 0, ROWS);
  receive_string(terminal, PRODUCT_NAME PRODUCT_VERSION PRODUCT_COPYRIGHT "\r\n");
}
