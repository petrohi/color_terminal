#include "terminal_internal.h"

#include "usbh_hid_keybd.h"

#define FIRST_REPEAT_COUNTER 500
#define NEXT_REPEAT_COUNTER 33

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
  terminal_uart_transmit_character(terminal, '\r');
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
    terminal_uart_transmit_character(terminal, key->data.character);
    break;
  case STR:
    terminal_uart_transmit_string(terminal, key->data.string);
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

void terminal_keyboard_handle_key(struct terminal *terminal, uint8_t key_code) {
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

void terminal_keyboard_handle_shift(struct terminal *terminal, bool shift) {
  terminal->shift_state = shift;
}

void terminal_keyboard_handle_alt(struct terminal *terminal, bool alt) {
  terminal->alt_state = alt;
}

void terminal_keyboard_handle_ctrl(struct terminal *terminal, bool ctrl) {
  terminal->ctrl_state = ctrl;
}

void terminal_keyboard_update_repeat_counter(struct terminal *terminal) {
  if (terminal->repeat_counter) {
    terminal->repeat_counter--;

    if (!terminal->repeat_counter && !terminal->repeat_pressed_key)
      terminal->repeat_pressed_key = true;
  }
}

void terminal_keyboard_repeat_key(struct terminal *terminal) {
  if (terminal->repeat_pressed_key) {
    terminal->repeat_counter = NEXT_REPEAT_COUNTER;
    terminal->repeat_pressed_key = false;

    handle_key(terminal, &entries[terminal->pressed_key_code]);
  }
}

void terminal_keyboard_init(struct terminal *terminal) {
  terminal->pressed_key_code = 0;
  terminal->repeat_counter = 0;
  terminal->repeat_pressed_key = false;

  terminal->lock_state.caps = 0;
  terminal->lock_state.scroll = 0;
  terminal->lock_state.num = 0;
}
