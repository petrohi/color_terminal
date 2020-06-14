#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct lock_state {
  uint8_t caps : 1;
  uint8_t scroll : 1;
  uint8_t num : 1;
};

enum font {
  FONT_NORMAL,
  FONT_BOLD,
  FONT_ITALIC,
};

typedef uint8_t color_t;

#define COLS 80
#define ROWS 24

struct terminal_callbacks {
  void (*keyboard_set_leds)(struct lock_state state);
  void (*uart_transmit)(uint8_t *data, uint16_t size);
  void (*uart_receive)(uint8_t *data, uint16_t size);
  void (*screen_draw_character)(size_t row, size_t col, uint8_t character,
                                enum font font, bool underlined, color_t active,
                                color_t inactive);
  void (*screen_draw_cursor)(size_t row, size_t col, color_t color);
};

#define TRANSMIT_BUFFER_SIZE 256
#define RECEIVE_BUFFER_SIZE 256

struct terminal {
  const struct terminal_callbacks *callbacks;
  uint8_t pressed_key_code;
  struct lock_state lock_state;
  uint8_t shift_state : 1;
  uint8_t alt_state : 1;
  uint8_t ctrl_state : 1;
  uint8_t cursor_row;
  uint8_t cursor_col;
  uint32_t uart_receive_count;

  uint16_t cursor_on_counter;
  uint16_t cursor_off_counter;
  bool cursor_state;
  bool last_cursor_state;

  uint8_t transmit_buffer[TRANSMIT_BUFFER_SIZE];
  uint8_t receive_buffer[RECEIVE_BUFFER_SIZE];
};

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks);
void terminal_handle_key(struct terminal *terminal, uint8_t key);
void terminal_handle_shift(struct terminal *terminal, bool shift);
void terminal_handle_alt(struct terminal *terminal, bool alt);
void terminal_handle_ctrl(struct terminal *terminal, bool ctrl);

void terminal_uart_receive(struct terminal *terminal, uint32_t count);
void terminal_timer_tick(struct terminal *terminal);
void terminal_update_cursor(struct terminal *terminal);

#endif
