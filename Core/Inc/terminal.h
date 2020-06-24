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
  FONT_THIN,
};

enum scroll { SCROLL_UP, SCROLL_DOWN };
enum blink { NO_BLINK, SLOW_BLINK, RAPID_BLINK };
typedef uint8_t color_t;
typedef uint8_t character_t;

#define DEFAULT_ACTIVE_COLOR 0xf
#define DEFAULT_INACTIVE_COLOR 0
#define CHARACTER_MAX UINT8_MAX

#define COLS 80
#define ROWS 24

struct terminal_callbacks {
  void (*keyboard_set_leds)(struct lock_state state);
  void (*uart_transmit)(void *data, uint16_t size);
  void (*uart_receive)(void *data, uint16_t size);
  void (*screen_draw_character)(size_t row, size_t col, character_t character,
                                enum font font, bool italic, bool underlined,
                                bool crossedout, color_t active,
                                color_t inactive);
  void (*screen_draw_cursor)(size_t row, size_t col, color_t color);
  void (*screen_clear_rows)(size_t from_row, size_t to_row, color_t inactive);
  void (*screen_clear_cols)(size_t row, size_t from_col, size_t to_col,
                            color_t inactive);
  void (*screen_scroll)(enum scroll scroll, size_t from_row, size_t to_row,
                        size_t rows, color_t inactive);
  void (*system_reset)();
};

#define TRANSMIT_BUFFER_SIZE 256
#define RECEIVE_BUFFER_SIZE 256

struct terminal;

#define DEFAULT_RECEIVE CHARACTER_MAX

typedef void (*receive_t)(struct terminal *, character_t);
typedef receive_t receive_table_t[CHARACTER_MAX + 1];

#define CSI_MAX_PARAMS_COUNT 8
#define CSI_MAX_PARAM_LENGTH 16

struct visual_state {
  int16_t cursor_row;
  int16_t cursor_col;
  bool cursor_last_col;

  enum font font;
  bool italic;
  bool underlined;
  enum blink blink;
  bool negative;
  bool concealed;
  bool crossedout;
  color_t active_color;
  color_t inactive_color;
};

struct terminal {
  const struct terminal_callbacks *callbacks;

  uint8_t pressed_key_code;
  volatile uint16_t repeat_counter;
  volatile bool repeat_pressed_key;

  struct lock_state lock_state;

  uint8_t shift_state : 1;
  uint8_t alt_state : 1;
  uint8_t ctrl_state : 1;

  bool new_line_mode;
  bool cursor_key_mode; // TODO
  bool keyboard_action_mode;
  bool auto_repeat_mode;

  bool auto_wrap_mode;
  bool scrolling_mode; // TODO
  bool column_mode; // TODO
  bool screen_mode; // TODO
  bool origin_mode; // TODO

  struct visual_state vs;
  struct visual_state saved_vs;

  volatile uint16_t cursor_counter;
  volatile bool cursor_on;
  bool cursor_inverted;

  uint32_t uart_receive_count;

  const receive_table_t *receive_table;

  character_t csi_params[CSI_MAX_PARAMS_COUNT][CSI_MAX_PARAM_LENGTH];
  size_t csi_params_count;
  size_t csi_last_param_length;

  character_t transmit_buffer[TRANSMIT_BUFFER_SIZE];
  character_t receive_buffer[RECEIVE_BUFFER_SIZE];
};

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks);
void terminal_keyboard_handle_key(struct terminal *terminal, uint8_t key);
void terminal_keyboard_handle_shift(struct terminal *terminal, bool shift);
void terminal_keyboard_handle_alt(struct terminal *terminal, bool alt);
void terminal_keyboard_handle_ctrl(struct terminal *terminal, bool ctrl);

void terminal_uart_receive(struct terminal *terminal, uint32_t count);
void terminal_timer_tick(struct terminal *terminal);
void terminal_screen_update_cursor(struct terminal *terminal);
void terminal_keyboard_repeat_key(struct terminal *terminal);

#endif
