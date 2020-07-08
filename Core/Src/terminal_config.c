#include "terminal_config.h"

void terminal_config_init(struct terminal_config *terminal_config,
                          struct terminal *terminal) {
  terminal_config->terminal = terminal;
  terminal_config->in_config = false;
}

void terminal_config_enter(struct terminal_config *terminal_config) {
  terminal_uart_xon_off(terminal_config->terminal, XOFF);
  terminal_config->in_config = true;

  terminal_uart_receive_string(terminal_config->terminal, "\x1b[2J");
  terminal_uart_receive_string(terminal_config->terminal, "\x1b[H");
  terminal_uart_receive_string(terminal_config->terminal, "Hello config!");
}

static void leave(struct terminal_config *terminal_config) {
  terminal_uart_receive_string(terminal_config->terminal, "\x1b[2J");
  terminal_uart_receive_string(terminal_config->terminal, "\x1b[H");
  terminal_uart_receive_string(terminal_config->terminal,
                               "Bye. Press Enter...\r\n");

  terminal_config->in_config = false;
  terminal_uart_xon_off(terminal_config->terminal, XON);
}

void terminal_config_receive_character(struct terminal_config *terminal_config,
                                       character_t character) {
  if (character == 'X' || character == 'x')
    leave(terminal_config);
}
