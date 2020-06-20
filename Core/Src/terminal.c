#include "terminal_internal.h"

#define PRODUCT_NAME                                                           \
  "\33[1;91mA\33[92mS\33[93mC\33[94mI\33[95mI\33[39m Terminal\33[m\r\n"
#define PRODUCT_VERSION "Version 3.0-alpha\r\n"
#define PRODUCT_COPYRIGHT "Copyright (C) 2020 Peter Hizalev\r\n"

void terminal_timer_tick(struct terminal *terminal) {
  terminal_keyboard_update_repeat_counter(terminal);
  terminal_screen_update_cursor_counter(terminal);
}

void terminal_init(struct terminal *terminal,
                   const struct terminal_callbacks *callbacks) {
  terminal->callbacks = callbacks;

  terminal_keyboard_init(terminal);
  terminal_screen_init(terminal);
  terminal_uart_init(terminal);

  terminal_uart_receive_string(
      terminal, PRODUCT_NAME PRODUCT_VERSION PRODUCT_COPYRIGHT "\r\n");
}
