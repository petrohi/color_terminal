#include "terminal.h"

#include <memory.h>
#include <stdio.h>

#define CURSOR_ON_COUNTER 1000
#define CURSOR_OFF_COUNTER 200

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks) {
  terminal->callbacks = callbacks;

  terminal->pressed_key_code = 0;

  terminal->lock_state.caps = 0;
  terminal->lock_state.scroll = 0;
  terminal->lock_state.num = 0;

  terminal->cursor_row = 0;
  terminal->cursor_col = 0;

  terminal->cursor_on_counter = CURSOR_ON_COUNTER;
  terminal->cursor_off_counter = CURSOR_OFF_COUNTER;
  terminal->cursor_state = false;
  terminal->last_cursor_state = false;

  terminal->uart_receive_count = RECEIVE_BUFFER_SIZE;
  terminal->callbacks->uart_receive(terminal->receive_buffer,
                                    sizeof(terminal->receive_buffer));
}

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
  uint8_t chr;
  void (*handler)(struct terminal *);
  struct keys_router *router;
};

struct keys_entry {
  enum keys_entry_type type;
  union keys_variant data;
};

size_t get_case(struct terminal *terminal) {
  return terminal->lock_state.caps ^ terminal->shift_state;
}

void update_keyboard_leds(struct terminal *terminal) {
  terminal->callbacks->keyboard_set_leds(terminal->lock_state);
}

void handle_caps_lock(struct terminal *terminal) {
  terminal->lock_state.caps ^= 1;
  update_keyboard_leds(terminal);
}

#define KEY_IGNORE                                                             \
  { IGNORE }
#define KEY_CHR(c)                                                             \
  {                                                                            \
    CHR, { .chr = c }                                                          \
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
    KEY_IGNORE,                                       // GRAVE ACCENT AND TILDE
    KEY_IGNORE,                                       // COMMA_AND_LESS
    KEY_IGNORE,                                       // DOT_GREATER
    KEY_IGNORE,                                       // SLASH_QUESTION
    KEY_HANDLER(handle_caps_lock),                    // CAPS LOCK
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
    KEY_IGNORE,                                       // SCROLL LOCK
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

void update_cursor(struct terminal *terminal) {
  terminal->callbacks->screen_draw_cursor(terminal->cursor_row,
                                          terminal->cursor_col, 0xf);
}

void clear_cursor(struct terminal *terminal) {
  if (terminal->last_cursor_state) {
    terminal->last_cursor_state = false;
    update_cursor(terminal);
  }
}

void transmit_chr(struct terminal *terminal, uint8_t chr) {
  memcpy(terminal->transmit_buffer, &chr, 1);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, 1);
  terminal->callbacks->screen_draw_character(terminal->cursor_row,
                                             terminal->cursor_col, chr,
                                             FONT_NORMAL, false, 0xf, 0);

  clear_cursor(terminal);
  terminal->cursor_col++;

  if (terminal->cursor_col == COLS) {
    terminal->cursor_col = 0;
    terminal->cursor_row++;
  }

  if (terminal->cursor_row == ROWS)
    terminal->cursor_row = 0;
}

void handle_key(struct terminal *terminal, const struct keys_entry *key) {
  struct keys_router *router;

  switch (key->type) {
  case IGNORE:
    break;
  case CHR:
    transmit_chr(terminal, key->data.chr);
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

void terminal_uart_receive(struct terminal *terminal, uint32_t count) {
  if (terminal->uart_receive_count == count)
    return;

  uint32_t i = terminal->uart_receive_count;

  while (i != count) {
    uint8_t chr = terminal->receive_buffer[RECEIVE_BUFFER_SIZE - i];

    transmit_chr(terminal, chr);
    i--;

    if (i == 0)
      i = RECEIVE_BUFFER_SIZE;
  }

  terminal->uart_receive_count = count;
}

void terminal_timer_tick(struct terminal *terminal) {
  if (terminal->cursor_on_counter != 0) {
    terminal->cursor_on_counter--;
    if (!terminal->cursor_state)
      terminal->cursor_state = true;

    return;
  }

  if (terminal->cursor_off_counter != 0) {
    terminal->cursor_off_counter--;
    if (terminal->cursor_state)
      terminal->cursor_state = false;

    return;
  }

  terminal->cursor_on_counter = CURSOR_ON_COUNTER;
  terminal->cursor_off_counter = CURSOR_OFF_COUNTER;
}

void terminal_update_cursor(struct terminal *terminal) {
  if (terminal->cursor_state && !terminal->last_cursor_state) {
    terminal->last_cursor_state = true;
    update_cursor(terminal);
    return;
  }
  
  if (!terminal->cursor_state && terminal->last_cursor_state) {
    terminal->last_cursor_state = false;
    update_cursor(terminal);
  }
}
