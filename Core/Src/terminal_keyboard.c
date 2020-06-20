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

static size_t get_ctrl(struct terminal *terminal) {
  return terminal->ctrl_state;
}

static size_t get_new_line_mode(struct terminal *terminal) {
  return terminal->new_line_mode;
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
    KEY_IGNORE, // NONE
    KEY_IGNORE, // ERRORROLLOVER
    KEY_IGNORE, // POSTFAIL
    KEY_IGNORE, // ERRORUNDEFINED
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('a'), KEY_CHR('A')),
               KEY_CHR('\x01')), // A
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('b'), KEY_CHR('B')),
               KEY_CHR('\x02')), // B
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('c'), KEY_CHR('C')),
               KEY_CHR('\x03')), // C
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('d'), KEY_CHR('D')),
               KEY_CHR('\x04')), // D
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('e'), KEY_CHR('E')),
               KEY_CHR('\x05')), // E
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('f'), KEY_CHR('F')),
               KEY_CHR('\x06')), // F
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('g'), KEY_CHR('G')),
               KEY_CHR('\x07')), // G
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('h'), KEY_CHR('H')),
               KEY_CHR('\x08')), // H
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('i'), KEY_CHR('I')),
               KEY_CHR('\x09')), // I
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('j'), KEY_CHR('J')),
               KEY_CHR('\x0a')), // J
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('k'), KEY_CHR('K')),
               KEY_CHR('\x0b')), // K
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('l'), KEY_CHR('L')),
               KEY_CHR('\x0c')), // L
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('m'), KEY_CHR('M')),
               KEY_CHR('\x0d')), // M
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('n'), KEY_CHR('N')),
               KEY_CHR('\x0e')), // N
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('o'), KEY_CHR('O')),
               KEY_CHR('\x0f')), // O
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('p'), KEY_CHR('P')),
               KEY_CHR('\x10')), // P
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('q'), KEY_CHR('Q')),
               KEY_CHR('\x11')), // Q
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('r'), KEY_CHR('R')),
               KEY_CHR('\x12')), // R
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('s'), KEY_CHR('S')),
               KEY_CHR('\x13')), // S
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('t'), KEY_CHR('T')),
               KEY_CHR('\x14')), // T
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('u'), KEY_CHR('U')),
               KEY_CHR('\x15')), // U
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('v'), KEY_CHR('V')),
               KEY_CHR('\x16')), // V
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('w'), KEY_CHR('W')),
               KEY_CHR('\x17')), // W
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('x'), KEY_CHR('X')),
               KEY_CHR('\x18')), // X
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('y'), KEY_CHR('Y')),
               KEY_CHR('\x19')), // Y
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_case, KEY_CHR('z'), KEY_CHR('Z')),
               KEY_CHR('\x1a')),                       // Z
    KEY_ROUTER(get_shift, KEY_CHR('1'), KEY_CHR('!')), // 1_EXCLAMATION_MARK
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('2'), KEY_CHR('@')),
               KEY_IGNORE), // 2_AT
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('3'), KEY_CHR('#')),
               KEY_CHR('\x1b')), // 3_NUMBER_SIGN
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('4'), KEY_CHR('$')),
               KEY_CHR('\x1c')), // 4_DOLLAR
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('5'), KEY_CHR('%')),
               KEY_CHR('\x1d')), // 5_PERCENT
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('6'), KEY_CHR('^')),
               KEY_CHR('\x1e')), // 6_CARET
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('7'), KEY_CHR('&')),
               KEY_CHR('\x1f')), // 7_AMPERSAND
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('8'), KEY_CHR('*')),
               KEY_CHR('\x7f')),                       // 8_ASTERISK
    KEY_ROUTER(get_shift, KEY_CHR('9'), KEY_CHR('(')), // 9_OPARENTHESIS
    KEY_ROUTER(get_shift, KEY_CHR('0'), KEY_CHR(')')), // 0_CPARENTHESIS
    KEY_ROUTER(get_new_line_mode, KEY_CHR('\x0d'),
               KEY_STR("\x0d\x0a")),                        // ENTER
    KEY_CHR('\x1b'),                                        // ESCAPE
    KEY_ROUTER(get_ctrl, KEY_CHR('\x7f'), KEY_CHR('\x18')), // BACKSPACE
    KEY_CHR('\x09'),                                        // TAB
    KEY_ROUTER(get_ctrl, KEY_CHR(' '), KEY_IGNORE),         // SPACEBAR
    KEY_ROUTER(get_shift, KEY_CHR('-'), KEY_CHR('_')),      // MINUS_UNDERSCORE
    KEY_ROUTER(get_shift, KEY_CHR('='), KEY_CHR('+')),      // EQUAL_PLUS
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('['), KEY_CHR('{')),
               KEY_CHR('\x1b')), // OBRACKET_AND_OBRACE
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR(']'), KEY_CHR('}')),
               KEY_CHR('\x1d')), // CBRACKET_AND_CBRACE
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('\\'), KEY_CHR('|')),
               KEY_CHR('\x1c')), // BACKSLASH_VERTICAL_BAR
    KEY_IGNORE,                  // NONUS_NUMBER_SIGN_TILDE
    KEY_ROUTER(get_shift, KEY_CHR(';'), KEY_CHR(':')), // SEMICOLON_COLON
    KEY_ROUTER(get_shift, KEY_CHR('\''),
               KEY_CHR('"')), // SINGLE_AND_DOUBLE_QUOTE
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('`'), KEY_CHR('~')),
               KEY_CHR('\x1e')),                       // GRAVE_ACCENT_AND_TILDE
    KEY_ROUTER(get_shift, KEY_CHR(','), KEY_CHR('<')), // COMMA_AND_LESS
    KEY_ROUTER(get_shift, KEY_CHR('.'), KEY_CHR('>')), // DOT_GREATER
    KEY_ROUTER(get_ctrl, KEY_ROUTER(get_shift, KEY_CHR('/'), KEY_CHR('?')),
               KEY_CHR('\x1f')),   // SLASH_QUESTION
    KEY_HANDLER(handle_caps_lock), // CAPS_LOCK
    KEY_STR("\033[11~"),           // F1
    KEY_STR("\033[12~"),           // F2
    KEY_STR("\033[13~"),           // F3
    KEY_STR("\033[14~"),           // F4
    KEY_STR("\033[15~"),           // F5
    KEY_STR("\033[17~"),           // F6
    KEY_STR("\033[18~"),           // F7
    KEY_STR("\033[19~"),           // F8
    KEY_STR("\033[20~"),           // F9
    KEY_STR("\033[21~"),           // F10
    KEY_STR("\033[23~"),           // F11
    KEY_STR("\033[24~"),           // F12
    KEY_IGNORE,                    // PRINTSCREEN
    KEY_IGNORE,                    // SCROLL_LOCK
    KEY_IGNORE,                    // PAUSE
    KEY_IGNORE,                    // INSERT
    KEY_IGNORE,                    // HOME
    KEY_IGNORE,                    // PAGEUP
    KEY_IGNORE,                    // DELETE
    KEY_IGNORE,                    // END1
    KEY_IGNORE,                    // PAGEDOWN
    KEY_STR("\033[C"),             // RIGHTARROW
    KEY_STR("\033[D"),             // LEFTARROW
    KEY_STR("\033[B"),             // DOWNARROW
    KEY_STR("\033[A"),             // UPARROW
    KEY_IGNORE,                    // KEYPAD_NUM_LOCK_AND_CLEAR
    KEY_IGNORE,                    // KEYPAD_SLASH
    KEY_IGNORE,                    // KEYPAD_ASTERIKS
    KEY_IGNORE,                    // KEYPAD_MINUS
    KEY_IGNORE,                    // KEYPAD_PLUS
    KEY_IGNORE,                    // KEYPAD_ENTER
    KEY_IGNORE,                    // KEYPAD_1_END
    KEY_IGNORE,                    // KEYPAD_2_DOWN_ARROW
    KEY_IGNORE,                    // KEYPAD_3_PAGEDN
    KEY_IGNORE,                    // KEYPAD_4_LEFT_ARROW
    KEY_IGNORE,                    // KEYPAD_5
    KEY_IGNORE,                    // KEYPAD_6_RIGHT_ARROW
    KEY_IGNORE,                    // KEYPAD_7_HOME
    KEY_IGNORE,                    // KEYPAD_8_UP_ARROW
    KEY_IGNORE,                    // KEYPAD_9_PAGEUP
    KEY_IGNORE,                    // KEYPAD_0_INSERT
    KEY_IGNORE,                    // KEYPAD_DECIMAL_SEPARATOR_DELETE
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

  terminal->new_line_mode = false;
}
