#include "terminal.h"

#include <memory.h>
#include <stdio.h>

void Terminal_Init(Terminal_t *terminal, const TerminalCallbacks_t *callbacks) {
  terminal->callbacks = callbacks;
  terminal->pressed_key_code = 0;
  terminal->lock_state.caps = 0;
  terminal->lock_state.scroll = 0;
  terminal->lock_state.num = 0;
}

typedef enum {
  IGNORE,
  CHR,
  HANDLER,
  ROUTER,
} KeyType_t;

typedef struct Key Key_t;

typedef struct KeyRouter {
  size_t (*router)(Terminal_t *);
  Key_t *keys;
} KeyRouter_t;

typedef union KeyData {
  uint8_t chr;
  void (*handler)(Terminal_t *);
  KeyRouter_t *router;
} KeyData_t;

typedef struct Key {
  KeyType_t type;
  KeyData_t data;
} Key_t;

size_t GetCase(Terminal_t *terminal) {
  return terminal->lock_state.caps ^ terminal->shift_state;
}

void update_keyboard_leds(Terminal_t *terminal) {
  terminal->callbacks->keyboard_set_leds(terminal->lock_state.caps,
                                         terminal->lock_state.scroll,
                                         terminal->lock_state.num);
}

void DoCapsLock(Terminal_t *terminal) {
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
      .router = &(KeyRouter_t) {                                               \
        r, (Key_t[]) { __VA_ARGS__ }                                           \
      }                                                                        \
    }                                                                          \
  }

static const Key_t *keys = (Key_t[]){
    KEY_IGNORE, // NONE
    KEY_IGNORE, // ERRORROLLOVER
    KEY_IGNORE, // POSTFAIL
    KEY_IGNORE, // ERRORUNDEFINED
    KEY_ROUTER(GetCase, KEY_CHR('a'), KEY_CHR('A')), // A
    KEY_IGNORE, // B
    KEY_IGNORE, // C
    KEY_IGNORE, // D
    KEY_IGNORE, // E
    KEY_IGNORE, // F
    KEY_IGNORE, // G
    KEY_IGNORE, // H
    KEY_IGNORE, // I
    KEY_IGNORE, // J
    KEY_IGNORE, // K
    KEY_IGNORE, // L
    KEY_IGNORE, // M
    KEY_IGNORE, // N
    KEY_IGNORE, // O
    KEY_IGNORE, // P
    KEY_IGNORE, // Q
    KEY_IGNORE, // R
    KEY_IGNORE, // S
    KEY_IGNORE, // T
    KEY_IGNORE, // U
    KEY_IGNORE, // V
    KEY_IGNORE, // W
    KEY_IGNORE, // X
    KEY_IGNORE, // Y
    KEY_IGNORE, // Z
    KEY_IGNORE, // 1_EXCLAMATION_MARK
    KEY_IGNORE, // 2_AT
    KEY_IGNORE, // 3_NUMBER_SIGN
    KEY_IGNORE, // 4_DOLLAR
    KEY_IGNORE, // 5_PERCENT
    KEY_IGNORE, // 6_CARET
    KEY_IGNORE, // 7_AMPERSAND
    KEY_IGNORE, // 8_ASTERISK
    KEY_IGNORE, // 9_OPARENTHESIS
    KEY_IGNORE, // 0_CPARENTHESIS
    KEY_IGNORE, // ENTER
    KEY_IGNORE, // ESCAPE
    KEY_IGNORE, // BACKSPACE
    KEY_IGNORE, // TAB
    KEY_IGNORE, // SPACEBAR
    KEY_IGNORE, // MINUS_UNDERSCORE
    KEY_IGNORE, // EQUAL_PLUS
    KEY_IGNORE, // OBRACKET_AND_OBRACE
    KEY_IGNORE, // CBRACKET_AND_CBRACE
    KEY_IGNORE, // BACKSLASH_VERTICAL_BAR
    KEY_IGNORE, // NONUS_NUMBER_SIGN_TILDE
    KEY_IGNORE, // SEMICOLON_COLON
    KEY_IGNORE, // SINGLE_AND_DOUBLE_QUOTE
    KEY_IGNORE, // GRAVE ACCENT AND TILDE
    KEY_IGNORE, // COMMA_AND_LESS
    KEY_IGNORE, // DOT_GREATER
    KEY_IGNORE, // SLASH_QUESTION
    KEY_HANDLER(DoCapsLock), // CAPS LOCK
    KEY_IGNORE, // F1
    KEY_IGNORE, // F2
    KEY_IGNORE, // F3
    KEY_IGNORE, // F4
    KEY_IGNORE, // F5
    KEY_IGNORE, // F6
    KEY_IGNORE, // F7
    KEY_IGNORE, // F8
    KEY_IGNORE, // F9
    KEY_IGNORE, // F10
    KEY_IGNORE, // F11
    KEY_IGNORE, // F12
    KEY_IGNORE, // PRINTSCREEN
    KEY_IGNORE, // SCROLL LOCK
    KEY_IGNORE, // PAUSE
    KEY_IGNORE, // INSERT
    KEY_IGNORE, // HOME
    KEY_IGNORE, // PAGEUP
    KEY_IGNORE, // DELETE
    KEY_IGNORE, // END1
    KEY_IGNORE, // PAGEDOWN
    KEY_IGNORE, // RIGHTARROW
    KEY_IGNORE, // LEFTARROW
    KEY_IGNORE, // DOWNARROW
    KEY_IGNORE, // UPARROW
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

void transmit_chr(Terminal_t *terminal, uint8_t chr) {
  memcpy(terminal->transmit_buffer, &chr, 1);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, 1);
}

void handle_key(Terminal_t *terminal, const Key_t *key) {
  KeyRouter_t *router;

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
    handle_key(terminal, &router->keys[router->router(terminal)]);
    break;
  }
}

void Terminal_HandleKey(Terminal_t *terminal, uint8_t key_code) {
  if (terminal->pressed_key_code == key_code)
    return;

  terminal->pressed_key_code = key_code;

  printf("key: 0x%x\n", key_code);

  handle_key(terminal, &keys[key_code]);
}

void Terminal_HandleShift(Terminal_t *terminal, bool shift) {
  terminal->shift_state = shift;
}

void Terminal_HandleAlt(Terminal_t *terminal, bool alt) {
  terminal->alt_state = alt;
}

void Terminal_HandleCtrl(Terminal_t *terminal, bool ctrl) {
  terminal->ctrl_state = ctrl;
}
