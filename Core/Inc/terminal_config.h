#pragma once

#include "terminal.h"

struct terminal_config {
  struct terminal *terminal;
  bool in_config;
};

void terminal_config_init(struct terminal_config *terminal_config,
                          struct terminal *terminal);
void terminal_config_enter(struct terminal_config *terminal_config);
void terminal_config_receive_character(struct terminal_config *terminal_config,
                                       character_t character);
