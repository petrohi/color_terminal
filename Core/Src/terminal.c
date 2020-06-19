#include "terminal.h"

#include <memory.h>
#include <stdio.h>

#include "usbh_hid_keybd.h"

#define CURSOR_ON_COUNTER 650
#define CURSOR_OFF_COUNTER 350

#define FIRST_REPEAT_COUNTER 500
#define NEXT_REPEAT_COUNTER 33

#define PRODUCT_VERSION "ASCII Terminal Version 3.0\r\n"
#define PRODUCT_COPYRIGHT "Copyright (C) 2020 Peter Hizalev\r\n"

enum keys_entry_type {
  IGNORE,
  CHR,
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
  void (*handler)(struct terminal *);
  struct keys_router *router;
};

struct keys_entry {
  enum keys_entry_type type;
  union keys_variant data;
};

static size_t get_case(struct terminal *terminal) {
  return terminal->lock_state.caps ^ terminal->shift_state;
}

static void update_keyboard_leds(struct terminal *terminal) {
  terminal->callbacks->keyboard_set_leds(terminal->lock_state);
}

static void handle_caps_lock(struct terminal *terminal) {
  terminal->lock_state.caps ^= 1;
  update_keyboard_leds(terminal);
}

#define KEY_IGNORE                                                             \
  { IGNORE }
#define KEY_CHR(c)                                                             \
  {                                                                            \
    CHR, { .character = c }                                                    \
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
    KEY_IGNORE,                                       // NONE
    KEY_IGNORE,                                       // ERRORROLLOVER
    KEY_IGNORE,                                       // POSTFAIL
    KEY_IGNORE,                                       // ERRORUNDEFINED
    KEY_ROUTER(get_case, KEY_CHR('a'), KEY_CHR('A')), // A
    KEY_IGNORE,                                       // B
    KEY_IGNORE,                                       // C
    KEY_IGNORE,                                       // D
    KEY_IGNORE,                                       // E
    KEY_IGNORE,                                       // F
    KEY_IGNORE,                                       // G
    KEY_IGNORE,                                       // H
    KEY_IGNORE,                                       // I
    KEY_IGNORE,                                       // J
    KEY_IGNORE,                                       // K
    KEY_IGNORE,                                       // L
    KEY_IGNORE,                                       // M
    KEY_IGNORE,                                       // N
    KEY_IGNORE,                                       // O
    KEY_IGNORE,                                       // P
    KEY_IGNORE,                                       // Q
    KEY_IGNORE,                                       // R
    KEY_IGNORE,                                       // S
    KEY_IGNORE,                                       // T
    KEY_IGNORE,                                       // U
    KEY_IGNORE,                                       // V
    KEY_IGNORE,                                       // W
    KEY_IGNORE,                                       // X
    KEY_IGNORE,                                       // Y
    KEY_IGNORE,                                       // Z
    KEY_IGNORE,                                       // 1_EXCLAMATION_MARK
    KEY_IGNORE,                                       // 2_AT
    KEY_IGNORE,                                       // 3_NUMBER_SIGN
    KEY_IGNORE,                                       // 4_DOLLAR
    KEY_IGNORE,                                       // 5_PERCENT
    KEY_IGNORE,                                       // 6_CARET
    KEY_IGNORE,                                       // 7_AMPERSAND
    KEY_IGNORE,                                       // 8_ASTERISK
    KEY_IGNORE,                                       // 9_OPARENTHESIS
    KEY_IGNORE,                                       // 0_CPARENTHESIS
    KEY_IGNORE,                                       // ENTER
    KEY_IGNORE,                                       // ESCAPE
    KEY_IGNORE,                                       // BACKSPACE
    KEY_IGNORE,                                       // TAB
    KEY_IGNORE,                                       // SPACEBAR
    KEY_IGNORE,                                       // MINUS_UNDERSCORE
    KEY_IGNORE,                                       // EQUAL_PLUS
    KEY_IGNORE,                                       // OBRACKET_AND_OBRACE
    KEY_IGNORE,                                       // CBRACKET_AND_CBRACE
    KEY_IGNORE,                                       // BACKSLASH_VERTICAL_BAR
    KEY_IGNORE,                                       // NONUS_NUMBER_SIGN_TILDE
    KEY_IGNORE,                                       // SEMICOLON_COLON
    KEY_IGNORE,                                       // SINGLE_AND_DOUBLE_QUOTE
    KEY_IGNORE,                                       // GRAVE_ACCENT_AND_TILDE
    KEY_IGNORE,                                       // COMMA_AND_LESS
    KEY_IGNORE,                                       // DOT_GREATER
    KEY_IGNORE,                                       // SLASH_QUESTION
    KEY_HANDLER(handle_caps_lock),                    // CAPS_LOCK
    KEY_IGNORE,                                       // F1
    KEY_IGNORE,                                       // F2
    KEY_IGNORE,                                       // F3
    KEY_IGNORE,                                       // F4
    KEY_IGNORE,                                       // F5
    KEY_IGNORE,                                       // F6
    KEY_IGNORE,                                       // F7
    KEY_IGNORE,                                       // F8
    KEY_IGNORE,                                       // F9
    KEY_IGNORE,                                       // F10
    KEY_IGNORE,                                       // F11
    KEY_IGNORE,                                       // F12
    KEY_IGNORE,                                       // PRINTSCREEN
    KEY_IGNORE,                                       // SCROLL_LOCK
    KEY_IGNORE,                                       // PAUSE
    KEY_IGNORE,                                       // INSERT
    KEY_IGNORE,                                       // HOME
    KEY_IGNORE,                                       // PAGEUP
    KEY_IGNORE,                                       // DELETE
    KEY_IGNORE,                                       // END1
    KEY_IGNORE,                                       // PAGEDOWN
    KEY_IGNORE,                                       // RIGHTARROW
    KEY_IGNORE,                                       // LEFTARROW
    KEY_IGNORE,                                       // DOWNARROW
    KEY_IGNORE,                                       // UPARROW
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

static void transmit_character(struct terminal *terminal,
                               character_t character) {
  memcpy(terminal->transmit_buffer, &character, 1);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, 1);
}

static void handle_key(struct terminal *terminal,
                       const struct keys_entry *key) {
  struct keys_router *router;

  switch (key->type) {
  case IGNORE:
    break;
  case CHR:
    transmit_character(terminal, key->data.character);
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

static void move_cursor(struct terminal *terminal, uint8_t row, uint8_t col) {
  clear_cursor(terminal);

  terminal->cursor_col = col;
  terminal->cursor_row = row;
  terminal->cursor_last_col = false;

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

static void line_feed(struct terminal *terminal) {
  clear_cursor(terminal);

  if (terminal->cursor_row == ROWS - 1)
    terminal->callbacks->screen_scroll(SCROLL_DOWN, 0, ROWS, 1, 0);
  else
    terminal->cursor_row++;
}

static void put_character(struct terminal *terminal, character_t character) {
  clear_cursor(terminal);

  if (terminal->cursor_last_col) {
    carriage_return(terminal);
    line_feed(terminal);
  }

  terminal->callbacks->screen_draw_character(
      terminal->cursor_row, terminal->cursor_col, character, terminal->font,
      terminal->underlined, terminal->active_color, terminal->inactive_color);

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
  line_feed(terminal);
}

static const receive_table_t csi_receive_table;

static void receive_csi(struct terminal *terminal, character_t character) {
  terminal->receive_table = &csi_receive_table;
  clear_csi_params(terminal);
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

static void receive_hvp(struct terminal *terminal, character_t character) {
  uint8_t row = get_csi_param(terminal, 0);
  uint8_t col = get_csi_param(terminal, 1);

  if (row)
    row--;
  if (col)
    col--;

  move_cursor(terminal, row, col);
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
    RECEIVE_HANDLER('f', receive_hvp),
};

static void receive_string(struct terminal *terminal, char *string) {
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
  if (terminal->repeat_counter != 0) {
    terminal->repeat_counter--;

    if (!terminal->repeat_counter && !terminal->repeat_pressed_key)
      terminal->repeat_pressed_key = true;
  }
}

static void update_cursor_counter(struct terminal *terminal) {
  if (terminal->cursor_counter != 0) {
    terminal->cursor_counter--;

    return;
  }

  if (terminal->cursor_on) {
    terminal->cursor_on = false;
    terminal->cursor_counter = CURSOR_OFF_COUNTER;
  } else {
    terminal->cursor_on = true;
    terminal->cursor_counter = CURSOR_ON_COUNTER;
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
  terminal->underlined = false;
  terminal->active_color = 0xf;
  terminal->inactive_color = 0;

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

  terminal->callbacks->screen_clear(0, ROWS, 0);
  receive_string(terminal, PRODUCT_VERSION PRODUCT_COPYRIGHT "\r\n");
}
