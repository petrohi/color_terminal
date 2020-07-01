#include "terminal_internal.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

#define PRINTF_BUFFER_SIZE 64

static void clear_csi_params(struct terminal *terminal) {
  memset(terminal->csi_params, 0, CSI_MAX_PARAMS_COUNT * CSI_MAX_PARAM_LENGTH);
  terminal->csi_params_count = 0;
  terminal->csi_last_param_length = 0;
}

static uint16_t get_csi_param(struct terminal *terminal, size_t index) {
  return atoi((const char *)terminal->csi_params[index]);
}

static const receive_table_t default_receive_table;

static void clear_receive_table(struct terminal *terminal) {
  terminal->receive_table = &default_receive_table;

#ifdef DEBUG
  if (terminal->unhandled)
    printf("*%s\r\n", terminal->debug_buffer);
  else
    printf("%s\r\n", terminal->debug_buffer);
#endif
}

static const receive_table_t esc_receive_table;

static void receive_esc(struct terminal *terminal, character_t character) {
  if (terminal->receive_table != &default_receive_table) {
#ifdef DEBUG
    terminal->unhandled = true;
#endif
    clear_receive_table(terminal);
  }

  terminal->receive_table = &esc_receive_table;

#ifdef DEBUG
  memset(terminal->debug_buffer, 0, DEBUG_BUFFER_LENGTH);
  terminal->debug_buffer_length = 0;
  terminal->unhandled = false;
#endif
}

static void receive_cr(struct terminal *terminal, character_t character) {
  terminal_screen_carriage_return(terminal);
}

static void receive_lf(struct terminal *terminal, character_t character) {
  if (terminal->new_line_mode)
    terminal_screen_carriage_return(terminal);

  terminal_screen_index(terminal, 1);
}

static void receive_tab(struct terminal *terminal, character_t character) {
  int16_t col = get_terminal_screen_cursor_col(terminal);

  while (col < COLS - 1 && !terminal->tab_stops[++col])
    ;
  terminal_screen_move_cursor_absolute(
      terminal, get_terminal_screen_cursor_row(terminal), col);
}

static void receive_bs(struct terminal *terminal, character_t character) {
  terminal_screen_move_cursor(terminal, 0, -1);
}

static void receive_nel(struct terminal *terminal, character_t character) {
  terminal_screen_carriage_return(terminal);
  terminal_screen_index(terminal, 1);
  clear_receive_table(terminal);
}

static void receive_ind(struct terminal *terminal, character_t character) {
  terminal_screen_index(terminal, 1);
  clear_receive_table(terminal);
}

static void receive_hts(struct terminal *terminal, character_t character) {
  terminal->tab_stops[get_terminal_screen_cursor_col(terminal)] = true;
  clear_receive_table(terminal);
}

static void receive_ri(struct terminal *terminal, character_t character) {
  terminal_screen_reverse_index(terminal, 1);
  clear_receive_table(terminal);
}

static void receive_decsc(struct terminal *terminal, character_t character) {
  terminal_screen_save_visual_state(terminal);
  clear_receive_table(terminal);
}

static void receive_decrc(struct terminal *terminal, character_t character) {
  terminal_screen_restore_visual_state(terminal);
  clear_receive_table(terminal);
}

static const receive_table_t csi_receive_table;

static void receive_csi(struct terminal *terminal, character_t character) {
  terminal->receive_table = &csi_receive_table;
  clear_csi_params(terminal);
}

static const receive_table_t esc_hash_receive_table;

static void receive_hash(struct terminal *terminal, character_t character) {
  terminal->receive_table = &esc_hash_receive_table;
}

static void receive_deckpam(struct terminal *terminal, character_t character) {
  terminal->lock_state.num = 0;
  terminal_update_keyboard_leds(terminal);
  clear_receive_table(terminal);
}

static void receive_deckpnm(struct terminal *terminal, character_t character) {
  terminal->lock_state.num = 1;
  terminal_update_keyboard_leds(terminal);
  clear_receive_table(terminal);
}

static void receive_ris(struct terminal *terminal, character_t character) {
  terminal->callbacks->system_reset();
}

static const receive_table_t si_receive_table;

static void receive_si(struct terminal *terminal, character_t character) {
  terminal->receive_table = &si_receive_table;
}

static const receive_table_t so_receive_table;

static void receive_so(struct terminal *terminal, character_t character) {
  terminal->receive_table = &so_receive_table;
}

static void receive_scs_g0(struct terminal *terminal, character_t character) {
  clear_receive_table(terminal);
}

static void receive_scs_g1(struct terminal *terminal, character_t character) {
  clear_receive_table(terminal);
}

static void receive_csi_param(struct terminal *terminal,
                              character_t character) {
  if (!terminal->csi_last_param_length) {
    if (terminal->csi_params_count == CSI_MAX_PARAMS_COUNT)
      return;

    terminal->csi_params_count++;
  }

  // Keep zero for the end of the string for atoi
  if (terminal->csi_last_param_length == CSI_MAX_PARAM_LENGTH - 1)
    return;

  terminal->csi_last_param_length++;
  terminal->csi_params[terminal->csi_params_count - 1]
                      [terminal->csi_last_param_length - 1] = character;
}

static void receive_csi_param_delimiter(struct terminal *terminal,
                                        character_t character) {
  if (!terminal->csi_last_param_length) {
    if (terminal->csi_params_count == CSI_MAX_PARAMS_COUNT)
      return;

    terminal->csi_params_count++;
    return;
  }

  terminal->csi_last_param_length = 0;
}

static void receive_da(struct terminal *terminal, character_t character) {
  terminal_uart_transmit_string(terminal, "\x1b[?1;0c");
  clear_receive_table(terminal);
}

static void receive_hvp(struct terminal *terminal, character_t character) {
  int16_t row = get_csi_param(terminal, 0);
  int16_t col = get_csi_param(terminal, 1);

  terminal_screen_move_cursor_absolute(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static void receive_tbc(struct terminal *terminal, character_t character) {
  int16_t mode = get_csi_param(terminal, 0);

  if (mode == 0)
    terminal->tab_stops[get_terminal_screen_cursor_col(terminal)] = false;
  else if (mode == 3)
    memset(terminal->tab_stops, 0, COLS);
#ifdef DEBUG
  else
    terminal->unhandled = true;
#endif

  clear_receive_table(terminal);
}

static void receive_hpa(struct terminal *terminal, character_t character) {
  int16_t col = get_csi_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal, get_terminal_screen_cursor_row(terminal), col - 1);
  clear_receive_table(terminal);
}

static void receive_vpa(struct terminal *terminal, character_t character) {
  int16_t row = get_csi_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal, row - 1, get_terminal_screen_cursor_col(terminal));
  clear_receive_table(terminal);
}

static void receive_sm(struct terminal *terminal, character_t character) {
  int16_t mode = get_csi_param(terminal, 0);

  switch (mode) {
  case 2: // KAM
    terminal->keyboard_action_mode = true;
    break;

  case 4: // IRM
    terminal->insert_mode = true;
    break;

  case 12: // SRM
    terminal->send_receive_mode = true;
    break;

  case 20: // LNM
    terminal->new_line_mode = true;
    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }
  clear_receive_table(terminal);
}

static void receive_rm(struct terminal *terminal, character_t character) {
  int16_t mode = get_csi_param(terminal, 0);

  switch (mode) {
  case 2: // KAM
    terminal->keyboard_action_mode = false;
    break;

  case 4: // IRM
    terminal->insert_mode = false;
    break;

  case 12: // SRM
    terminal->send_receive_mode = false;
    break;

  case 20: // LNM
    terminal->new_line_mode = false;
    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }
  clear_receive_table(terminal);
}

static void receive_dsr(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 5:
    terminal_uart_transmit_string(terminal, "\x1b[0n");
    break;

  case 6: {
    terminal_uart_transmit_printf(terminal, "\x1b[%d;%dR",
                                  get_terminal_screen_cursor_row(terminal) + 1,
                                  get_terminal_screen_cursor_col(terminal) + 1);
  } break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }

  clear_receive_table(terminal);
}

static void receive_dectst(struct terminal *terminal, character_t character) {
  clear_receive_table(terminal);
}

static void receive_cup(struct terminal *terminal, character_t character) {
  int16_t row = get_csi_param(terminal, 0);
  int16_t col = get_csi_param(terminal, 1);

  terminal_screen_move_cursor_absolute(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static color_t get_sgr_color(struct terminal *terminal, size_t *i) {
  uint16_t code = get_csi_param(terminal, (*i)++);

  if (code == 5) {
    return get_csi_param(terminal, (*i)++);
  } else if (code == 2) {
    get_csi_param(terminal, (*i)++);
    get_csi_param(terminal, (*i)++);
    get_csi_param(terminal, (*i)++);

    // TODO: get the closest color from CLUT
#ifdef DEBUG
    terminal->unhandled = true;
#endif
  }
  return DEFAULT_ACTIVE_COLOR;
}

static void handle_sgr(struct terminal *terminal, size_t *i) {
  uint16_t code = get_csi_param(terminal, (*i)++);
  bool unhandled = false;

  switch (code) {
  case 0:
    terminal->vs.p.font = FONT_NORMAL;
    terminal->vs.p.italic = false;
    terminal->vs.p.underlined = false;
    terminal->vs.p.blink = false;
    terminal->vs.p.negative = false;
    terminal->vs.p.concealed = false;
    terminal->vs.p.crossedout = false;
    terminal->vs.p.active_color = DEFAULT_ACTIVE_COLOR;
    terminal->vs.p.inactive_color = DEFAULT_INACTIVE_COLOR;
    break;

  case 1:
    terminal->vs.p.font = FONT_BOLD;
    break;

  case 2:
    terminal->vs.p.font = FONT_THIN;
    break;

  case 3:
    terminal->vs.p.italic = true;
    break;

  case 4:
    terminal->vs.p.underlined = true;
    break;

  case 5:
  case 6:
    terminal->vs.p.blink = true;
    break;

  case 7:
    terminal->vs.p.negative = true;
    break;

  case 8:
    terminal->vs.p.concealed = true;
    break;

  case 9:
    terminal->vs.p.crossedout = true;
    break;

  case 10:
  case 21:
  case 22:
    terminal->vs.p.font = FONT_NORMAL;
    break;

  case 23:
    terminal->vs.p.italic = false;
    break;

  case 24:
    terminal->vs.p.underlined = false;
    break;

  case 25:
    terminal->vs.p.blink = false;
    break;

  case 27:
    terminal->vs.p.negative = false;
    break;

  case 28:
    terminal->vs.p.concealed = false;
    break;

  case 29:
    terminal->vs.p.crossedout = false;
    break;

  case 38:
    terminal->vs.p.active_color = get_sgr_color(terminal, i);
    break;

  case 39:
    terminal->vs.p.active_color = DEFAULT_ACTIVE_COLOR;
    break;

  case 48:
    terminal->vs.p.inactive_color = get_sgr_color(terminal, i);
    break;

  case 49:
    terminal->vs.p.inactive_color = DEFAULT_INACTIVE_COLOR;
    break;

  default:
    unhandled = true;
    break;
  }

  if (unhandled) {
    if (code >= 30 && code < 38)
      terminal->vs.p.active_color = code - 30;
    else if (code >= 40 && code < 48)
      terminal->vs.p.inactive_color = code - 40;
    else if (code >= 90 && code < 98)
      terminal->vs.p.active_color = code - 90 + 8;
    else if (code >= 100 && code < 108)
      terminal->vs.p.inactive_color = code - 100 + 8;
#ifdef DEBUG
    else
      terminal->unhandled = true;
#endif
  }
}

static void receive_sgr(struct terminal *terminal, character_t character) {
  size_t i = 0;
  if (terminal->csi_params_count) {
    while (i < terminal->csi_params_count)
      handle_sgr(terminal, &i);
  } else
    handle_sgr(terminal, &i);

  clear_receive_table(terminal);
}

static void receive_cuu(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_move_cursor(terminal, -rows, 0);
  clear_receive_table(terminal);
}

static void receive_cud(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_move_cursor(terminal, rows, 0);
  clear_receive_table(terminal);
}

static void receive_cuf(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_move_cursor(terminal, 0, cols);
  clear_receive_table(terminal);
}

static void receive_cub(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_move_cursor(terminal, 0, -cols);
  clear_receive_table(terminal);
}

static void receive_cnl(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_carriage_return(terminal);
  terminal_screen_index(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cpl(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_carriage_return(terminal);
  terminal_screen_reverse_index(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cha(struct terminal *terminal, character_t character) {
  int16_t col = get_csi_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal, get_terminal_screen_cursor_row(terminal), col - 1);
  clear_receive_table(terminal);
}

static void receive_sd(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_DOWN, 0, rows);
  clear_receive_table(terminal);
}

static void receive_su(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_UP, 0, rows);
  clear_receive_table(terminal);
}

static void receive_ed(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 0:
    terminal_screen_clear_to_right(terminal);
    terminal_screen_clear_to_bottom(terminal);

    break;

  case 1:
    terminal_screen_clear_to_left(terminal);
    terminal_screen_clear_to_top(terminal);

    break;

  case 2:
  case 3:
    terminal_screen_clear_all(terminal);

    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }

  clear_receive_table(terminal);
}

static void receive_el(struct terminal *terminal, character_t character) {
  uint16_t code = get_csi_param(terminal, 0);

  switch (code) {
  case 0:
    terminal_screen_clear_to_right(terminal);
    break;

  case 1:
    terminal_screen_clear_to_left(terminal);
    break;

  case 2:
    terminal_screen_clear_row(terminal);

    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }

  clear_receive_table(terminal);
}

static void receive_ich(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_insert(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_dch(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_delete(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_ech(struct terminal *terminal, character_t character) {
  int16_t cols = get_csi_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_erase(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_il(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_DOWN,
                         get_terminal_screen_cursor_row(terminal), rows);
  clear_receive_table(terminal);
}

static void receive_dl(struct terminal *terminal, character_t character) {
  int16_t rows = get_csi_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_UP,
                         get_terminal_screen_cursor_row(terminal), rows);
  clear_receive_table(terminal);
}

static void receive_decstbm(struct terminal *terminal, character_t character) {
  int16_t top = get_csi_param(terminal, 0);
  int16_t bottom = get_csi_param(terminal, 1);

  if (top)
    top--;

  if (!bottom)
    bottom = ROWS;

  if (top >= 0 && bottom >= top && bottom <= ROWS) {
    terminal->margin_top = top;
    terminal->margin_bottom = bottom;
    terminal_screen_move_cursor_absolute(terminal, 0, 0);
  }

  clear_receive_table(terminal);
}

static void receive_decaln(struct terminal *terminal, character_t character) {
  for (size_t row = 0; row < ROWS; ++row)
    for (size_t col = 0; col < COLS; ++col) {
      terminal_screen_move_cursor_absolute(terminal, row, col);
      terminal_screen_put_character(terminal, 'E');
    }

  terminal_screen_move_cursor_absolute(terminal, 0, 0);
  clear_receive_table(terminal);
}

static const receive_table_t csi_decmod_receive_table;

static void receive_csi_decmod(struct terminal *terminal,
                               character_t character) {
  terminal->receive_table = &csi_decmod_receive_table;
}

static void receive_decsm(struct terminal *terminal, character_t character) {
  int16_t mode = get_csi_param(terminal, 0);

  switch (mode) {
  case 1: // DECCKM
    terminal->cursor_key_mode = true;
    break;

  case 2: // DECANM
    terminal->ansi_mode = true;
    break;

  case 3: // DECCOLM
    terminal->column_mode = true;
    terminal_screen_clear_all(terminal);
    terminal_screen_move_cursor_absolute(terminal, 0, 0);
    break;

  case 4: // DECSCKL
    terminal->scrolling_mode = true;
    break;

  case 5: // DECSCNM
    terminal_screen_set_screen_mode(terminal, true);
    break;

  case 6: // DECCOM
    terminal->origin_mode = true;
    terminal_screen_move_cursor_absolute(terminal, 0, 0);
    break;

  case 7: // DECAWM
    terminal->auto_wrap_mode = true;
    break;

  case 8: // DECARM
    terminal->auto_repeat_mode = true;
    break;

  case 9: // DECINLM
    break;

  case 25: // DECTCEM
    terminal_screen_enable_cursor(terminal, true);
    break;

  case 66: // DECNKM
    terminal->lock_state.num = 0;
    terminal_update_keyboard_leds(terminal);
    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }
  clear_receive_table(terminal);
}

static void receive_decrm(struct terminal *terminal, character_t character) {
  int16_t mode = get_csi_param(terminal, 0);

  switch (mode) {
  case 1: // DECCKM
    terminal->cursor_key_mode = false;
    break;

  case 2: // DECANM
    terminal->ansi_mode = false;
    break;

  case 3: // DECCOLM
    terminal->column_mode = false;
    terminal_screen_clear_all(terminal);
    terminal_screen_move_cursor_absolute(terminal, 0, 0);
    break;

  case 4: // DECSCKL
    terminal->scrolling_mode = false;
    break;

  case 5: // DECSCNM
    terminal_screen_set_screen_mode(terminal, false);
    break;

  case 6: // DECCOM
    terminal->origin_mode = false;
    terminal_screen_move_cursor_absolute(terminal, 0, 0);
    break;

  case 7: // DECAWM
    terminal->auto_wrap_mode = false;
    break;

  case 8: // DECARM
    terminal->auto_repeat_mode = false;
    break;

  case 9: // DECINLM
    break;

  case 25: // DECTCEM
    terminal_screen_enable_cursor(terminal, false);
    break;

  case 66: // DECNKM
    terminal->lock_state.num = 1;
    terminal_update_keyboard_leds(terminal);
    break;

#ifdef DEBUG
  default:
    terminal->unhandled = true;
    break;
#endif
  }
  clear_receive_table(terminal);
}

static const receive_table_t osc_receive_table;

static void receive_osc(struct terminal *terminal, character_t character) {
  terminal->receive_table = &osc_receive_table;
}

static void receive_osc_data(struct terminal *terminal, character_t character) {
  if (character == '\x07')
    clear_receive_table(terminal);
}

static void receive_printable(struct terminal *terminal,
                              character_t character) {
  terminal_screen_put_character(terminal, character);
}

static void receive_ignore(struct terminal *terminal, character_t character) {}

static void receive_unexpected(struct terminal *terminal,
                               character_t character) {
#ifdef DEBUG
  terminal->unhandled = true;
#endif
  clear_receive_table(terminal);
}

static void receive_character(struct terminal *terminal,
                              character_t character) {
  receive_t receive = (*terminal->receive_table)[character];

  if (!receive) {
    receive = (*terminal->receive_table)[DEFAULT_RECEIVE];
  }

#ifdef DEBUG
  // Keep zero for the end of the string for printf
  if (terminal->receive_table != &default_receive_table &&
      terminal->debug_buffer_length < DEBUG_BUFFER_LENGTH - 1) {
    if (character < 0x20 || (character >= 0x7f && character <= 0xa0))
      terminal->debug_buffer_length += snprintf(
          (char *)terminal->debug_buffer + terminal->debug_buffer_length,
          DEBUG_BUFFER_LENGTH - terminal->debug_buffer_length - 1, "\\x%x",
          character);
    else
      terminal->debug_buffer_length += snprintf(
          (char *)terminal->debug_buffer + terminal->debug_buffer_length,
          DEBUG_BUFFER_LENGTH - terminal->debug_buffer_length - 1, "%c",
          character);
  }
#endif
  receive(terminal, character);
}

#define RECEIVE_HANDLER(c, h) [c] = h
#define DEFAULT_RECEIVE_HANDLER(h) [DEFAULT_RECEIVE] = h

#define DEFAULT_RECEIVE_TABLE                                                  \
  RECEIVE_HANDLER('\x00', receive_ignore),                                     \
      RECEIVE_HANDLER('\x01', receive_ignore),                                 \
      RECEIVE_HANDLER('\x02', receive_ignore),                                 \
      RECEIVE_HANDLER('\x03', receive_ignore),                                 \
      RECEIVE_HANDLER('\x04', receive_ignore),                                 \
      RECEIVE_HANDLER('\x05', receive_ignore),                                 \
      RECEIVE_HANDLER('\x06', receive_ignore),                                 \
      RECEIVE_HANDLER('\x07', receive_ignore),                                 \
      RECEIVE_HANDLER('\x08', receive_bs),                                     \
      RECEIVE_HANDLER('\x09', receive_tab),                                    \
      RECEIVE_HANDLER('\x0a', receive_lf),                                     \
      RECEIVE_HANDLER('\x0b', receive_lf),                                     \
      RECEIVE_HANDLER('\x0c', receive_lf),                                     \
      RECEIVE_HANDLER('\x0d', receive_cr),                                     \
      RECEIVE_HANDLER('\x0e', receive_ignore),                                 \
      RECEIVE_HANDLER('\x0f', receive_ignore),                                 \
      RECEIVE_HANDLER('\x10', receive_ignore),                                 \
      RECEIVE_HANDLER('\x11', receive_ignore),                                 \
      RECEIVE_HANDLER('\x12', receive_ignore),                                 \
      RECEIVE_HANDLER('\x13', receive_ignore),                                 \
      RECEIVE_HANDLER('\x14', receive_ignore),                                 \
      RECEIVE_HANDLER('\x15', receive_ignore),                                 \
      RECEIVE_HANDLER('\x16', receive_ignore),                                 \
      RECEIVE_HANDLER('\x17', receive_ignore),                                 \
      RECEIVE_HANDLER('\x18', receive_ignore),                                 \
      RECEIVE_HANDLER('\x19', receive_ignore),                                 \
      RECEIVE_HANDLER('\x1a', receive_ignore),                                 \
      RECEIVE_HANDLER('\x1b', receive_esc),                                    \
      RECEIVE_HANDLER('\x1c', receive_ignore),                                 \
      RECEIVE_HANDLER('\x1d', receive_ignore),                                 \
      RECEIVE_HANDLER('\x1e', receive_ignore),                                 \
      RECEIVE_HANDLER('\x1f', receive_ignore),                                 \
      RECEIVE_HANDLER('\x7f', receive_bs)

static const receive_table_t default_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    DEFAULT_RECEIVE_HANDLER(receive_printable),
};

static const receive_table_t esc_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('[', receive_csi),
    RECEIVE_HANDLER(']', receive_osc),
    RECEIVE_HANDLER('#', receive_hash),
    RECEIVE_HANDLER('=', receive_deckpam),
    RECEIVE_HANDLER('>', receive_deckpnm),
    RECEIVE_HANDLER('c', receive_ris),
    RECEIVE_HANDLER('E', receive_nel),
    RECEIVE_HANDLER('D', receive_ind),
    RECEIVE_HANDLER('H', receive_hts),
    RECEIVE_HANDLER('M', receive_ri),
    RECEIVE_HANDLER('7', receive_decsc),
    RECEIVE_HANDLER('8', receive_decrc),
    RECEIVE_HANDLER('(', receive_si),
    RECEIVE_HANDLER(')', receive_so),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

#define CSI_RECEIVE_TABLE                                                      \
  RECEIVE_HANDLER('0', receive_csi_param),                                     \
      RECEIVE_HANDLER('1', receive_csi_param),                                 \
      RECEIVE_HANDLER('2', receive_csi_param),                                 \
      RECEIVE_HANDLER('3', receive_csi_param),                                 \
      RECEIVE_HANDLER('4', receive_csi_param),                                 \
      RECEIVE_HANDLER('5', receive_csi_param),                                 \
      RECEIVE_HANDLER('6', receive_csi_param),                                 \
      RECEIVE_HANDLER('7', receive_csi_param),                                 \
      RECEIVE_HANDLER('8', receive_csi_param),                                 \
      RECEIVE_HANDLER('9', receive_csi_param),                                 \
      RECEIVE_HANDLER(';', receive_csi_param_delimiter)

static const receive_table_t csi_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    CSI_RECEIVE_TABLE,
    RECEIVE_HANDLER('`', receive_hpa),
    RECEIVE_HANDLER('@', receive_ich),
    RECEIVE_HANDLER('?', receive_csi_decmod),
    RECEIVE_HANDLER('c', receive_da),
    RECEIVE_HANDLER('d', receive_vpa),
    RECEIVE_HANDLER('f', receive_hvp),
    RECEIVE_HANDLER('g', receive_tbc),
    RECEIVE_HANDLER('h', receive_sm),
    RECEIVE_HANDLER('l', receive_rm),
    RECEIVE_HANDLER('m', receive_sgr),
    RECEIVE_HANDLER('n', receive_dsr),
    RECEIVE_HANDLER('r', receive_decstbm),
    RECEIVE_HANDLER('y', receive_dectst),
    RECEIVE_HANDLER('A', receive_cuu),
    RECEIVE_HANDLER('B', receive_cud),
    RECEIVE_HANDLER('C', receive_cuf),
    RECEIVE_HANDLER('D', receive_cub),
    RECEIVE_HANDLER('E', receive_cnl),
    RECEIVE_HANDLER('F', receive_cpl),
    RECEIVE_HANDLER('G', receive_cha),
    RECEIVE_HANDLER('H', receive_cup),
    RECEIVE_HANDLER('J', receive_ed),
    RECEIVE_HANDLER('K', receive_el),
    RECEIVE_HANDLER('L', receive_il),
    RECEIVE_HANDLER('M', receive_dl),
    RECEIVE_HANDLER('P', receive_dch),
    RECEIVE_HANDLER('S', receive_su),
    RECEIVE_HANDLER('T', receive_sd),
    RECEIVE_HANDLER('X', receive_ech),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t csi_decmod_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    CSI_RECEIVE_TABLE,
    RECEIVE_HANDLER('h', receive_decsm),
    RECEIVE_HANDLER('l', receive_decrm),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t esc_hash_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('8', receive_decaln),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t si_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('A', receive_scs_g0),
    RECEIVE_HANDLER('B', receive_scs_g0),
    RECEIVE_HANDLER('0', receive_scs_g0),
    RECEIVE_HANDLER('1', receive_scs_g0),
    RECEIVE_HANDLER('2', receive_scs_g0),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t so_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('A', receive_scs_g1),
    RECEIVE_HANDLER('B', receive_scs_g1),
    RECEIVE_HANDLER('0', receive_scs_g1),
    RECEIVE_HANDLER('1', receive_scs_g1),
    RECEIVE_HANDLER('2', receive_scs_g1),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t osc_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_osc_data),
};

void terminal_uart_receive_string(struct terminal *terminal,
                                  const char *string) {
  while (*string) {
    receive_character(terminal, *string);
    string++;
  }
}

void terminal_uart_receive(struct terminal *terminal, uint32_t count) {
  if (terminal->uart_receive_count == count)
    return;

  uint32_t i = terminal->uart_receive_count;

  while (i != count) {
    character_t character =
        terminal->receive_buffer[terminal->receive_buffer_size - i];
    receive_character(terminal, character);
    i--;

    if (i == 0)
      i = terminal->receive_buffer_size;
  }

  terminal->uart_receive_count = count;
}

void terminal_uart_transmit_character(struct terminal *terminal,
                                      character_t character) {
  memcpy(terminal->transmit_buffer, &character, 1);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, 1);
}

void terminal_uart_transmit_string(struct terminal *terminal,
                                   const char *string) {
  size_t len = strlen(string);
  memcpy(terminal->transmit_buffer, string, len);
  terminal->callbacks->uart_transmit(terminal->transmit_buffer, len);

  // TODO: len > transmit_buffer_size
}

void terminal_uart_transmit_printf(struct terminal *terminal,
                                   const char *format, ...) {
  va_list args;
  va_start(args, format);

  char buffer[PRINTF_BUFFER_SIZE];
  vsnprintf(buffer, PRINTF_BUFFER_SIZE, format, args);
  terminal_uart_transmit_string(terminal, buffer);

  va_end(args);
}

void terminal_uart_init(struct terminal *terminal) {
  terminal->receive_table = &default_receive_table;

  memset(terminal->csi_params, 0, CSI_MAX_PARAMS_COUNT * CSI_MAX_PARAM_LENGTH);
  terminal->csi_params_count = 0;
  terminal->csi_last_param_length = 0;

  terminal->uart_receive_count = terminal->receive_buffer_size;
  terminal->callbacks->uart_receive(terminal->receive_buffer,
                                    terminal->receive_buffer_size);
#ifdef DEBUG
  memset(terminal->debug_buffer, 0, DEBUG_BUFFER_LENGTH);
  terminal->debug_buffer_length = 0;
  terminal->unhandled = false;
#endif
}
