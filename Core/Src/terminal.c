#include "terminal_internal.h"

#define PRODUCT_NAME                                                           \
  "\33[1;91mA\33[92mS\33[93mC\33[94mI\33[95mI\33[39m Terminal\33[m\r\n"
#define PRODUCT_VERSION "Version 3.0-alpha\r\n"
#define PRODUCT_COPYRIGHT "Copyright (C) 2020 Peter Hizalev\r\n"

void terminal_timer_tick(struct terminal *terminal) {
  terminal_keyboard_update_repeat_counter(terminal);
  terminal_screen_update_cursor_counter(terminal);
  terminal_screen_update_blink_counter(terminal);
}

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks,
                   character_t *transmit_buffer, size_t transmit_buffer_size,
                   character_t *receive_buffer, size_t receive_buffer_size) {
  terminal->callbacks = callbacks;

  terminal->transmit_buffer = transmit_buffer;
  terminal->transmit_buffer_size = transmit_buffer_size;

  terminal->receive_buffer = receive_buffer;
  terminal->receive_buffer_size = receive_buffer_size;

  terminal_keyboard_init(terminal);
  terminal_screen_init(terminal);
  terminal_uart_init(terminal);

  terminal_uart_receive_string(
      terminal, PRODUCT_NAME PRODUCT_VERSION PRODUCT_COPYRIGHT "\r\n");
}
