#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct lock_state {
  uint8_t caps : 1;
  uint8_t scroll : 1;
  uint8_t num : 1;
};

enum font {
  FONT_NORMAL = 0,
  FONT_BOLD = 1,
  FONT_THIN = 2,
};

enum scroll { SCROLL_UP, SCROLL_DOWN };
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
  void (*screen_clear_rows)(size_t from_row, size_t to_row, color_t inactive);
  void (*screen_clear_cols)(size_t row, size_t from_col, size_t to_col,
                            color_t inactive);
  void (*screen_scroll)(enum scroll scroll, size_t from_row, size_t to_row,
                        size_t rows, color_t inactive);
  void (*screen_shift_characters_right)(size_t row, size_t col, size_t cols,
                                        color_t inactive);
  void (*screen_shift_characters_left)(size_t row, size_t col, size_t cols,
                                       color_t inactive);
  void (*system_reset)();
};

struct terminal;

#define DEFAULT_RECEIVE CHARACTER_MAX

typedef void (*receive_t)(struct terminal *, character_t);
typedef receive_t receive_table_t[CHARACTER_MAX + 1];

#define ESC_MAX_PARAMS_COUNT 16
#define ESC_MAX_PARAM_LENGTH 16

struct visual_props {
  uint8_t font : 2;
  uint8_t blink : 1;
  uint8_t italic : 1;
  uint8_t underlined : 1;
  uint8_t negative : 1;
  uint8_t concealed : 1;
  uint8_t crossedout : 1;

  color_t active_color;
  color_t inactive_color;
};

struct visual_cell {
  character_t c;
  struct visual_props p;
};

struct visual_state {
  int16_t cursor_row;
  int16_t cursor_col;
  bool cursor_last_col;

  struct visual_props p;
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
  bool cursor_key_mode;
  bool keyboard_action_mode;
  bool auto_repeat_mode;
  bool send_receive_mode; // TODO
  bool ansi_mode;

  bool auto_wrap_mode;
  bool scrolling_mode; // TODO
  bool column_mode;    // TODO
  bool screen_mode;
  bool origin_mode;
  bool insert_mode;

  struct visual_state vs;
  struct visual_state saved_vs;

  int16_t margin_top;
  int16_t margin_bottom;

  bool tab_stops[COLS];

  volatile uint16_t cursor_counter;
  volatile bool cursor_on;
  bool cursor_drawn;

  volatile uint16_t blink_counter;
  volatile bool blink_on;
  bool blink_drawn;

  struct visual_cell cells[ROWS * COLS];

  uint32_t uart_receive_count;

  const receive_table_t *receive_table;

  character_t esc_params[ESC_MAX_PARAMS_COUNT][ESC_MAX_PARAM_LENGTH];
  size_t esc_params_count;
  size_t esc_last_param_length;

  character_t vt52_move_cursor_row;

  character_t *transmit_buffer;
  size_t transmit_buffer_size;

  character_t *receive_buffer;
  size_t receive_buffer_size;

#ifdef DEBUG
#define DEBUG_BUFFER_LENGTH 128
  character_t debug_buffer[DEBUG_BUFFER_LENGTH];
  uint8_t debug_buffer_length;
  bool unhandled;
#endif
};

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks,
                   character_t *transmit_buffer, size_t transmit_buffer_size,
                   character_t *receive_buffer, size_t receive_buffer_size);
void terminal_keyboard_handle_key(struct terminal *terminal, uint8_t key);
void terminal_keyboard_handle_shift(struct terminal *terminal, bool shift);
void terminal_keyboard_handle_alt(struct terminal *terminal, bool alt);
void terminal_keyboard_handle_ctrl(struct terminal *terminal, bool ctrl);

void terminal_uart_receive(struct terminal *terminal, uint32_t count);
void terminal_timer_tick(struct terminal *terminal);
void terminal_screen_update(struct terminal *terminal);
void terminal_keyboard_repeat_key(struct terminal *terminal);
