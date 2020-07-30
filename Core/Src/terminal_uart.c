#include "terminal_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PRINTF_BUFFER_SIZE 64

#define DECRQSS_PREFIX "$q"
#define DECRQSS_PREFIX_LENGTH 2

static void clear_esc_params(struct terminal *terminal) {
  memset(terminal->esc_params, 0, ESC_MAX_PARAMS_COUNT * ESC_MAX_PARAM_LENGTH);
  terminal->esc_params_count = 0;
  terminal->esc_last_param_length = 0;
  terminal->vt52_move_cursor_row = 0;
}

static int16_t get_esc_param(struct terminal *terminal, size_t index) {
  return atoi((const char *)terminal->esc_params[index]);
}

static const receive_table_t utf8_prefix_receive_table;
static const receive_table_t ascii_receive_table;

static bool default_receive_table(struct terminal *terminal) {
  return (terminal->receive_table == &utf8_prefix_receive_table ||
          terminal->receive_table == &ascii_receive_table);
}

static void clear_receive_table(struct terminal *terminal) {
  if (terminal->charset == CHARSET_UTF8)
    terminal->receive_table = &utf8_prefix_receive_table;
  else
    terminal->receive_table = &ascii_receive_table;

  clear_esc_params(terminal);

#ifdef DEBUG
  if (terminal->unhandled)
    printf("UNHANLED ESC: %s\r\n", terminal->debug_buffer);
#ifdef DEBUG_LOG_HANDLED_ESC
  else
    printf("ESC: %s\r\n", terminal->debug_buffer);
#endif
#endif
}

static const receive_table_t esc_receive_table;
static const receive_table_t vt52_esc_receive_table;

static void cancel_esc(struct terminal *terminal) {
  if (!default_receive_table(terminal)) {
#ifdef DEBUG
    terminal->unhandled = true;
#endif
    clear_receive_table(terminal);
  }
}

static void receive_esc(struct terminal *terminal, character_t character) {
  cancel_esc(terminal);

  if (terminal->ansi_mode)
    terminal->receive_table = &esc_receive_table;
  else
    terminal->receive_table = &vt52_esc_receive_table;

#ifdef DEBUG
  memset(terminal->debug_buffer, 0, DEBUG_BUFFER_LENGTH);
  terminal->debug_buffer_length = 0;
  terminal->unhandled = false;
#endif
}

static void receive_sub(struct terminal *terminal, character_t character) {
  cancel_esc(terminal);
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

static void receive_si(struct terminal *terminal, character_t character) {
  terminal->vs.gset_gl = GSET_G0;
}

static void receive_so(struct terminal *terminal, character_t character) {
  terminal->vs.gset_gl = GSET_G1;
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

static void receive_ss2(struct terminal *terminal, character_t character) {
#ifdef DEBUG
  terminal->unhandled = true;
#endif
  clear_receive_table(terminal);
}

static void receive_ss3(struct terminal *terminal, character_t character) {
#ifdef DEBUG
  terminal->unhandled = true;
#endif
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

static void receive_ls2(struct terminal *terminal, character_t character) {
  terminal->vs.gset_gl = GSET_G2;
  clear_receive_table(terminal);
}

static void receive_ls3(struct terminal *terminal, character_t character) {
  terminal->vs.gset_gl = GSET_G3;
  clear_receive_table(terminal);
}

static const receive_table_t csi_receive_table;

static void receive_csi(struct terminal *terminal, character_t character) {
  terminal->receive_table = &csi_receive_table;
}

static const receive_table_t esc_hash_receive_table;

static void receive_hash(struct terminal *terminal, character_t character) {
  terminal->receive_table = &esc_hash_receive_table;
}

static const receive_table_t esc_space_receive_table;

static void receive_space(struct terminal *terminal, character_t character) {
  terminal->receive_table = &esc_space_receive_table;
}

static const receive_table_t esc_percent_receive_table;

static void receive_percent(struct terminal *terminal, character_t character) {
  terminal->receive_table = &esc_percent_receive_table;
}

static void receive_s7c1t(struct terminal *terminal, character_t character) {
  terminal->c1_mode = C1_MODE_7BIT;
  clear_receive_table(terminal);
}

static void receive_s8c1t(struct terminal *terminal, character_t character) {
  terminal->c1_mode = C1_MODE_8BIT;
  clear_receive_table(terminal);
}

static void receive_charset_ascii(struct terminal *terminal,
                                  character_t character) {
  terminal->charset = CHARSET_ASCII;
  clear_receive_table(terminal);
}

static void receive_charset_utf8(struct terminal *terminal,
                                 character_t character) {
  terminal->charset = CHARSET_UTF8;
  clear_receive_table(terminal);
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

static const receive_table_t scs_receive_table;

static const uint8_t scs_gset_decode_table[CHARACTER_DECODER_TABLE_LENGTH] = {
    ['('] = GSET_G0,
    [')'] = GSET_G1,
    ['*'] = GSET_G2,
    ['+'] = GSET_G3,
};

static const codepoint_transformation_table_t dec_special_graphics_table = {
    [0x5f] = 0x00a0, [0x60] = 0x25c6, [0x61] = 0x2592, [0x62] = 0x2409,
    [0x63] = 0x240c, [0x64] = 0x240d, [0x65] = 0x240a, [0x66] = 0x00b0,
    [0x67] = 0x00b1, [0x68] = 0x2424, [0x69] = 0x240b, [0x6a] = 0x2518,
    [0x6b] = 0x2510, [0x6c] = 0x250c, [0x6d] = 0x2514, [0x6e] = 0x253c,
    [0x6f] = 0x23ba, [0x70] = 0x23bb, [0x71] = 0x2500, [0x72] = 0x23bc,
    [0x73] = 0x23bd, [0x74] = 0x251c, [0x75] = 0x2524, [0x76] = 0x2534,
    [0x77] = 0x252c, [0x78] = 0x2502, [0x79] = 0x2264, [0x7a] = 0x2265,
    [0x7b] = 0x03c0, [0x7c] = 0x2260, [0x7d] = 0x00a3, [0x7e] = 0x00b7,
};

static const codepoint_transformation_table_t
    *scs_charset_table[CHARACTER_DECODER_TABLE_LENGTH] = {
        ['0'] = &dec_special_graphics_table};

static void receive_scs(struct terminal *terminal, character_t character) {
  terminal->gset_received = scs_gset_decode_table[character];
  terminal->receive_table = &scs_receive_table;
}

static void receive_scs_set(struct terminal *terminal, character_t character) {
  if (terminal->gset_received != GSET_UNDEFINED &&
      terminal->gset_received <= GSET_MAX) {
    terminal->vs.gset_table[terminal->gset_received - 1] =
        scs_charset_table[character];
  }

  clear_receive_table(terminal);
}

static void receive_esc_param(struct terminal *terminal,
                              character_t character) {
  if (!terminal->esc_last_param_length) {
    if (terminal->esc_params_count == ESC_MAX_PARAMS_COUNT)
      return;

    terminal->esc_params_count++;
  }

  // Keep zero for the end of the string for atoi
  if (terminal->esc_last_param_length == ESC_MAX_PARAM_LENGTH - 1)
    return;

  terminal->esc_last_param_length++;
  terminal->esc_params[terminal->esc_params_count - 1]
                      [terminal->esc_last_param_length - 1] = character;
}

static void receive_esc_param_delimiter(struct terminal *terminal,
                                        character_t character) {
  if (!terminal->esc_last_param_length) {
    if (terminal->esc_params_count == ESC_MAX_PARAMS_COUNT)
      return;

    terminal->esc_params_count++;
    return;
  }

  terminal->esc_last_param_length = 0;
}

static void receive_da(struct terminal *terminal, character_t character) {
  terminal_uart_transmit_string(terminal, "\x1b[?65;1;9c");
  clear_receive_table(terminal);
}

static void receive_hvp(struct terminal *terminal, character_t character) {
  int16_t row = get_esc_param(terminal, 0);
  int16_t col = get_esc_param(terminal, 1);

  terminal_screen_move_cursor_absolute(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static void receive_tbc(struct terminal *terminal, character_t character) {
  int16_t mode = get_esc_param(terminal, 0);

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
  int16_t col = get_esc_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal, get_terminal_screen_cursor_row(terminal), col - 1);
  clear_receive_table(terminal);
}

static void receive_vpa(struct terminal *terminal, character_t character) {
  int16_t row = get_esc_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal, row - 1, get_terminal_screen_cursor_col(terminal));
  clear_receive_table(terminal);
}

static void receive_sm(struct terminal *terminal, character_t character) {
  int16_t mode = get_esc_param(terminal, 0);

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
  int16_t mode = get_esc_param(terminal, 0);

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
  uint16_t code = get_esc_param(terminal, 0);

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
  int16_t row = get_esc_param(terminal, 0);
  int16_t col = get_esc_param(terminal, 1);

  terminal_screen_move_cursor_absolute(terminal, row - 1, col - 1);
  clear_receive_table(terminal);
}

static color_t get_sgr_color(struct terminal *terminal, size_t *i) {
  uint16_t code = get_esc_param(terminal, (*i)++);

  if (code == 5) {
    return get_esc_param(terminal, (*i)++);
  } else if (code == 2) {
    get_esc_param(terminal, (*i)++);
    get_esc_param(terminal, (*i)++);
    get_esc_param(terminal, (*i)++);

    // TODO: get the closest color from CLUT
#ifdef DEBUG
    terminal->unhandled = true;
#endif
  }
  return DEFAULT_ACTIVE_COLOR;
}

static void handle_sgr(struct terminal *terminal, size_t *i) {
  uint16_t code = get_esc_param(terminal, (*i)++);
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
  if (terminal->esc_params_count) {
    while (i < terminal->esc_params_count)
      handle_sgr(terminal, &i);
  } else
    handle_sgr(terminal, &i);

  clear_receive_table(terminal);
}

static void receive_cuu(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_move_cursor(terminal, -rows, 0);
  clear_receive_table(terminal);
}

static void receive_cud(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_move_cursor(terminal, rows, 0);
  clear_receive_table(terminal);
}

static void receive_cuf(struct terminal *terminal, character_t character) {
  int16_t cols = get_esc_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_move_cursor(terminal, 0, cols);
  clear_receive_table(terminal);
}

static void receive_cub(struct terminal *terminal, character_t character) {
  int16_t cols = get_esc_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_move_cursor(terminal, 0, -cols);
  clear_receive_table(terminal);
}

static void receive_cnl(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_carriage_return(terminal);
  terminal_screen_index(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cpl(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_carriage_return(terminal);
  terminal_screen_reverse_index(terminal, rows);
  clear_receive_table(terminal);
}

static void receive_cha(struct terminal *terminal, character_t character) {
  int16_t col = get_esc_param(terminal, 0);

  terminal_screen_move_cursor_absolute(
      terminal,
      get_terminal_screen_cursor_row(terminal) +
          (terminal->vs.cursor_last_col ? 1 : 0),
      col - 1);
  clear_receive_table(terminal);
}

static void receive_sd(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_DOWN, terminal->margin_top, rows);
  clear_receive_table(terminal);
}

static void receive_su(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_UP, terminal->margin_top, rows);
  clear_receive_table(terminal);
}

static void receive_ed(struct terminal *terminal, character_t character) {
  uint16_t code = get_esc_param(terminal, 0);

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
  uint16_t code = get_esc_param(terminal, 0);

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
  int16_t cols = get_esc_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_insert(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_dch(struct terminal *terminal, character_t character) {
  int16_t cols = get_esc_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_delete(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_ech(struct terminal *terminal, character_t character) {
  int16_t cols = get_esc_param(terminal, 0);
  if (!cols)
    cols = 1;

  terminal_screen_erase(terminal, cols);
  clear_receive_table(terminal);
}

static void receive_il(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_DOWN,
                         get_terminal_screen_cursor_row(terminal), rows);
  clear_receive_table(terminal);
}

static void receive_dl(struct terminal *terminal, character_t character) {
  int16_t rows = get_esc_param(terminal, 0);
  if (!rows)
    rows = 1;

  terminal_screen_scroll(terminal, SCROLL_UP,
                         get_terminal_screen_cursor_row(terminal), rows);
  clear_receive_table(terminal);
}

static void receive_decstbm(struct terminal *terminal, character_t character) {
  int16_t top = get_esc_param(terminal, 0);
  int16_t bottom = get_esc_param(terminal, 1);

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

static void receive_decreqtparm(struct terminal *terminal,
                                character_t character) {
  int16_t req = get_esc_param(terminal, 0);

  if (req == 0 || req == 1) {
    // TODO: Get from configuration
    uint8_t par = 1;
    uint8_t nbits = 2;
    uint8_t xspeed = 120;
    uint8_t rspeed = 120;
    uint8_t clkmul = 0;
    terminal_uart_transmit_printf(terminal, "\x1b[%d;%d;%d;%d;%d;%dx",
                                  req == 0 ? 2 : 3, par, nbits, xspeed, rspeed,
                                  clkmul);

    clear_receive_table(terminal);
  }
}

static void receive_decaln(struct terminal *terminal, character_t character) {
  for (size_t row = 0; row < ROWS; ++row)
    for (size_t col = 0; col < COLS; ++col) {
      terminal_screen_move_cursor_absolute(terminal, row, col);
      terminal_screen_put_codepoint(terminal, (codepoint_t)'E');
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
  int16_t mode = get_esc_param(terminal, 0);

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
  int16_t mode = get_esc_param(terminal, 0);

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

static void clear_control_data(struct control_data *control_data) {
  memset(control_data->data, 0, MAX_CONTROL_DATA_LENGTH);
  control_data->length = 0;
}

static bool receive_control_data(struct control_data *control_data,
                                 character_t character) {
  if (control_data->length == MAX_CONTROL_DATA_LENGTH - 1)
    return true;

  control_data->data[control_data->length++] = character;

  if (control_data->data[control_data->length - 1] == 0x07)
    return true;

  if (control_data->data[control_data->length - 1] == 0x9c)
    return true;

  if (control_data->data[control_data->length - 2] == 0x1b &&
      control_data->data[control_data->length - 1] == '\\')
    return true;

  return false;
}

static const receive_table_t osc_receive_table;

static void receive_osc(struct terminal *terminal, character_t character) {
  terminal->receive_table = &osc_receive_table;
  clear_control_data(&terminal->osc);
}

static void receive_osc_data(struct terminal *terminal, character_t character) {
  if (receive_control_data(&terminal->osc, character))
    clear_receive_table(terminal);
}

static const receive_table_t dcs_receive_table;

static void receive_dcs(struct terminal *terminal, character_t character) {
  terminal->receive_table = &dcs_receive_table;
  clear_control_data(&terminal->dcs);
}

static void receive_dcs_data(struct terminal *terminal, character_t character) {
  if (receive_control_data(&terminal->dcs, character)) {
    if (terminal->dcs.length >= DECRQSS_PREFIX_LENGTH &&
        strncmp((char *)terminal->dcs.data, DECRQSS_PREFIX,
                DECRQSS_PREFIX_LENGTH) == 0) {
      terminal_uart_transmit_string(terminal, "\x1bP0$r\x1b\\");
    }
    clear_receive_table(terminal);
  }
}

static const receive_table_t apc_receive_table;

static void receive_apc(struct terminal *terminal, character_t character) {
  terminal->receive_table = &apc_receive_table;
  clear_control_data(&terminal->apc);
}

static void receive_apc_data(struct terminal *terminal, character_t character) {
  if (receive_control_data(&terminal->apc, character))
    clear_receive_table(terminal);
}

static const receive_table_t pm_receive_table;

static void receive_pm(struct terminal *terminal, character_t character) {
  terminal->receive_table = &pm_receive_table;
  clear_control_data(&terminal->pm);
}

static void receive_pm_data(struct terminal *terminal, character_t character) {
  if (receive_control_data(&terminal->pm, character))
    clear_receive_table(terminal);
}

static const receive_table_t vt52_move_cursor_row_receive_table;

static void receive_vt52_move_cursor(struct terminal *terminal,
                                     character_t character) {
  terminal->receive_table = &vt52_move_cursor_row_receive_table;
}

static const receive_table_t vt52_move_cursor_col_receive_table;

static void receive_vt52_move_cursor_row(struct terminal *terminal,
                                         character_t character) {
  terminal->vt52_move_cursor_row = character;
  terminal->receive_table = &vt52_move_cursor_col_receive_table;
}

static void receive_vt52_move_cursor_col(struct terminal *terminal,
                                         character_t character) {
  terminal_screen_move_cursor_absolute(
      terminal, terminal->vt52_move_cursor_row - 32, character - 32);
  clear_receive_table(terminal);
}

static void receive_vt52_id(struct terminal *terminal, character_t character) {
  terminal_uart_transmit_string(terminal, "\x1b/Z");
  clear_receive_table(terminal);
}

static void receive_vt52_ansi(struct terminal *terminal,
                              character_t character) {
  terminal->ansi_mode = true;
  clear_receive_table(terminal);
}

struct utf8_codec_entry {
  character_t mask;
  character_t lead;
  size_t bits_stored;
};

static const struct utf8_codec_entry *utf8_codec[] = {
    [0] = &(struct utf8_codec_entry){0b00111111, 0b10000000, 6},
    [1] = &(struct utf8_codec_entry){0b01111111, 0b00000000, 7},
    [2] = &(struct utf8_codec_entry){0b00011111, 0b11000000, 5},
    [3] = &(struct utf8_codec_entry){0b00001111, 0b11100000, 4},
    [4] = &(struct utf8_codec_entry){0b00000111, 0b11110000, 3},
    &(struct utf8_codec_entry){0},
};

static size_t get_utf8_codepoint_length(character_t character) {
  int length = 0;

  for (const struct utf8_codec_entry **e = utf8_codec; *e; ++e) {
    if ((character & ~(*e)->mask) == (*e)->lead) {
      break;
    }

    ++length;
  }

  if (length > 4)
    return 0;

  return length;
}

static codepoint_t decode_utf8_codepoint(character_t *buffer, size_t length) {
  if (!(length > 0 && length <= 3))
    return 0;

  size_t shift = utf8_codec[0]->bits_stored * (length - 1);
  codepoint_t codepoint = (*buffer++ & utf8_codec[length]->mask) << shift;

  for (size_t i = 1; i < length; ++i, ++buffer) {
    shift -= utf8_codec[0]->bits_stored;
    codepoint |= (*buffer & utf8_codec[0]->mask) << shift;
  }

  return codepoint;
}

static codepoint_t transform_codepoint(struct terminal *terminal,
                                       codepoint_t codepoint) {

  if (terminal->vs.gset_gl != GSET_UNDEFINED &&
      terminal->vs.gset_gl <= GSET_MAX &&
      codepoint < CHARACTER_DECODER_TABLE_LENGTH) {
    const codepoint_transformation_table_t *table =
        terminal->vs.gset_table[terminal->vs.gset_gl - 1];

    if (table) {
      codepoint_t transformed_codepoint = (*table)[codepoint];
      if (transformed_codepoint)
        return transformed_codepoint;
    }
  }

  return codepoint;
}

static void receive_ascii(struct terminal *terminal, character_t character) {
  terminal_screen_put_codepoint(
      terminal, transform_codepoint(terminal, (codepoint_t)character));
}

static const receive_table_t utf8_continuation_receive_table;

static void receive_utf8_prefix(struct terminal *terminal,
                                character_t character) {

  size_t length = get_utf8_codepoint_length(character);

  if (length > 1) {
    terminal->utf8_codepoint_length = length;
    terminal->utf8_buffer[terminal->utf8_buffer_length++] = character;
    terminal->receive_table = &utf8_continuation_receive_table;
  } else if (length == 1)
    terminal_screen_put_codepoint(
        terminal,
        transform_codepoint(terminal, decode_utf8_codepoint(&character, 1)));
}

static void clear_utf8_buffer(struct terminal *terminal) {
  terminal->utf8_codepoint_length = 0;
  terminal->utf8_buffer_length = 0;
  memset(terminal->utf8_buffer, 0, 4);
}

static void receive_utf8_continuation(struct terminal *terminal,
                                      character_t character) {

  terminal->utf8_buffer[terminal->utf8_buffer_length++] = character;
  if (terminal->utf8_buffer_length == terminal->utf8_codepoint_length) {
    terminal_screen_put_codepoint(
        terminal,
        transform_codepoint(
            terminal, decode_utf8_codepoint(terminal->utf8_buffer,
                                            terminal->utf8_codepoint_length)));

    clear_utf8_buffer(terminal);
    clear_receive_table(terminal);
  }
}

static void receive_8bit_control(struct terminal *terminal,
                                 character_t character) {
  receive_esc(terminal, 0x1b);
  terminal_uart_receive_character(terminal, character);
}

static void receive_8bit_ind(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'D');
}

static void receive_8bit_nel(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'E');
}

static void receive_8bit_hts(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'H');
}

static void receive_8bit_ri(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'M');
}

static void receive_8bit_ss2(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'N');
}

static void receive_8bit_ss3(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'O');
}

static void receive_8bit_dcs(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'P');
}

static void receive_8bit_da(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, 'Z');
}

static void receive_8bit_csi(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, '[');
}

static void receive_8bit_osc(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, ']');
}

static void receive_8bit_pm(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, '^');
}

static void receive_8bit_apc(struct terminal *terminal, character_t character) {
  receive_8bit_control(terminal, '_');
}

static void receive_ignore(struct terminal *terminal, character_t character) {}

static void receive_unexpected(struct terminal *terminal,
                               character_t character) {
#ifdef DEBUG
  terminal->unhandled = true;
#endif
  clear_receive_table(terminal);
}

void terminal_uart_receive_character(struct terminal *terminal,
                                     character_t character) {
  receive_t receive = (*terminal->receive_table)[character];

  if (!receive) {
    receive = (*terminal->receive_table)[DEFAULT_RECEIVE];
  }

#ifdef DEBUG
  // Keep zero for the end of the string for printf
  if (!default_receive_table(terminal) &&
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

void terminal_uart_receive_string(struct terminal *terminal,
                                  const char *string) {
  while (*string) {
    terminal_uart_receive_character(terminal, *string);
    string++;
  }
}

#define RECEIVE_HANDLER(c, h) [c] = h
#define DEFAULT_RECEIVE_HANDLER(h) [DEFAULT_RECEIVE] = h

#define DEFAULT_RECEIVE_TABLE                                                  \
  RECEIVE_HANDLER(0x00, receive_ignore),                                       \
      RECEIVE_HANDLER(0x01, receive_ignore),                                   \
      RECEIVE_HANDLER(0x02, receive_ignore),                                   \
      RECEIVE_HANDLER(0x03, receive_ignore),                                   \
      RECEIVE_HANDLER(0x04, receive_ignore),                                   \
      RECEIVE_HANDLER(0x05, receive_ignore),                                   \
      RECEIVE_HANDLER(0x06, receive_ignore),                                   \
      RECEIVE_HANDLER(0x07, receive_ignore),                                   \
      RECEIVE_HANDLER(0x08, receive_bs), RECEIVE_HANDLER(0x09, receive_tab),   \
      RECEIVE_HANDLER(0x0a, receive_lf), RECEIVE_HANDLER(0x0b, receive_lf),    \
      RECEIVE_HANDLER(0x0c, receive_lf), RECEIVE_HANDLER(0x0d, receive_cr),    \
      RECEIVE_HANDLER(0x0e, receive_so), RECEIVE_HANDLER(0x0f, receive_si),    \
      RECEIVE_HANDLER(0x10, receive_ignore),                                   \
      RECEIVE_HANDLER(0x11, receive_ignore),                                   \
      RECEIVE_HANDLER(0x12, receive_ignore),                                   \
      RECEIVE_HANDLER(0x13, receive_ignore),                                   \
      RECEIVE_HANDLER(0x14, receive_ignore),                                   \
      RECEIVE_HANDLER(0x15, receive_ignore),                                   \
      RECEIVE_HANDLER(0x16, receive_ignore),                                   \
      RECEIVE_HANDLER(0x17, receive_ignore),                                   \
      RECEIVE_HANDLER(0x18, receive_ignore),                                   \
      RECEIVE_HANDLER(0x19, receive_ignore),                                   \
      RECEIVE_HANDLER(0x1a, receive_sub), RECEIVE_HANDLER(0x1b, receive_esc),  \
      RECEIVE_HANDLER(0x1c, receive_ignore),                                   \
      RECEIVE_HANDLER(0x1d, receive_ignore),                                   \
      RECEIVE_HANDLER(0x1e, receive_ignore),                                   \
      RECEIVE_HANDLER(0x1f, receive_ignore),                                   \
      RECEIVE_HANDLER(0x7f, receive_bs),                                       \
      RECEIVE_HANDLER(0x80, receive_ignore),                                   \
      RECEIVE_HANDLER(0x81, receive_ignore),                                   \
      RECEIVE_HANDLER(0x82, receive_ignore),                                   \
      RECEIVE_HANDLER(0x83, receive_ignore),                                   \
      RECEIVE_HANDLER(0x84, receive_8bit_ind),                                 \
      RECEIVE_HANDLER(0x85, receive_8bit_nel),                                 \
      RECEIVE_HANDLER(0x86, receive_ignore),                                   \
      RECEIVE_HANDLER(0x87, receive_ignore),                                   \
      RECEIVE_HANDLER(0x88, receive_8bit_hts),                                 \
      RECEIVE_HANDLER(0x89, receive_ignore),                                   \
      RECEIVE_HANDLER(0x8a, receive_ignore),                                   \
      RECEIVE_HANDLER(0x8b, receive_ignore),                                   \
      RECEIVE_HANDLER(0x8c, receive_ignore),                                   \
      RECEIVE_HANDLER(0x8d, receive_8bit_ri),                                  \
      RECEIVE_HANDLER(0x8e, receive_8bit_ss2),                                 \
      RECEIVE_HANDLER(0x8f, receive_8bit_ss3),                                 \
      RECEIVE_HANDLER(0x90, receive_8bit_dcs),                                 \
      RECEIVE_HANDLER(0x91, receive_ignore),                                   \
      RECEIVE_HANDLER(0x92, receive_ignore),                                   \
      RECEIVE_HANDLER(0x93, receive_ignore),                                   \
      RECEIVE_HANDLER(0x94, receive_ignore),                                   \
      RECEIVE_HANDLER(0x95, receive_ignore),                                   \
      RECEIVE_HANDLER(0x96, receive_ignore),                                   \
      RECEIVE_HANDLER(0x97, receive_ignore),                                   \
      RECEIVE_HANDLER(0x98, receive_ignore),                                   \
      RECEIVE_HANDLER(0x99, receive_ignore),                                   \
      RECEIVE_HANDLER(0x9a, receive_8bit_da),                                  \
      RECEIVE_HANDLER(0x9b, receive_8bit_csi),                                 \
      RECEIVE_HANDLER(0x9c, receive_ignore),                                   \
      RECEIVE_HANDLER(0x9d, receive_8bit_osc),                                 \
      RECEIVE_HANDLER(0x9e, receive_8bit_pm),                                  \
      RECEIVE_HANDLER(0x9f, receive_8bit_apc)

static const receive_table_t ascii_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    DEFAULT_RECEIVE_HANDLER(receive_ascii),
};

static const receive_table_t utf8_prefix_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    DEFAULT_RECEIVE_HANDLER(receive_utf8_prefix),
};

static const receive_table_t utf8_continuation_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_utf8_continuation),
};

static const receive_table_t esc_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER(' ', receive_space),
    RECEIVE_HANDLER('(', receive_scs),
    RECEIVE_HANDLER(')', receive_scs),
    RECEIVE_HANDLER('*', receive_scs),
    RECEIVE_HANDLER('+', receive_scs),
    RECEIVE_HANDLER('-', receive_scs),
    RECEIVE_HANDLER('.', receive_scs),
    RECEIVE_HANDLER('/', receive_scs),
    RECEIVE_HANDLER('[', receive_csi),
    RECEIVE_HANDLER(']', receive_osc),
    RECEIVE_HANDLER('#', receive_hash),
    RECEIVE_HANDLER('=', receive_deckpam),
    RECEIVE_HANDLER('>', receive_deckpnm),
    RECEIVE_HANDLER('_', receive_apc),
    RECEIVE_HANDLER('^', receive_pm),
    RECEIVE_HANDLER('%', receive_percent),
    RECEIVE_HANDLER('c', receive_ris),
    RECEIVE_HANDLER('n', receive_ls2),
    RECEIVE_HANDLER('o', receive_ls3),
    RECEIVE_HANDLER('E', receive_nel),
    RECEIVE_HANDLER('D', receive_ind),
    RECEIVE_HANDLER('H', receive_hts),
    RECEIVE_HANDLER('M', receive_ri),
    RECEIVE_HANDLER('N', receive_ss2),
    RECEIVE_HANDLER('O', receive_ss3),
    RECEIVE_HANDLER('P', receive_dcs),
    RECEIVE_HANDLER('Z', receive_da),
    RECEIVE_HANDLER('7', receive_decsc),
    RECEIVE_HANDLER('8', receive_decrc),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t vt52_esc_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('A', receive_cuu),
    RECEIVE_HANDLER('B', receive_cud),
    RECEIVE_HANDLER('C', receive_cuf),
    RECEIVE_HANDLER('D', receive_cub),
    RECEIVE_HANDLER('H', receive_cup),
    RECEIVE_HANDLER('I', receive_ri),
    RECEIVE_HANDLER('J', receive_ed),
    RECEIVE_HANDLER('K', receive_el),
    RECEIVE_HANDLER('Y', receive_vt52_move_cursor),
    RECEIVE_HANDLER('Z', receive_vt52_id),
    RECEIVE_HANDLER('=', receive_deckpam),
    RECEIVE_HANDLER('>', receive_deckpnm),
    RECEIVE_HANDLER('<', receive_vt52_ansi),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t vt52_move_cursor_row_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_vt52_move_cursor_row),
};

static const receive_table_t vt52_move_cursor_col_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_vt52_move_cursor_col),
};

#define ESC_PARAM_RECEIVE_TABLE                                                \
  RECEIVE_HANDLER('0', receive_esc_param),                                     \
      RECEIVE_HANDLER('1', receive_esc_param),                                 \
      RECEIVE_HANDLER('2', receive_esc_param),                                 \
      RECEIVE_HANDLER('3', receive_esc_param),                                 \
      RECEIVE_HANDLER('4', receive_esc_param),                                 \
      RECEIVE_HANDLER('5', receive_esc_param),                                 \
      RECEIVE_HANDLER('6', receive_esc_param),                                 \
      RECEIVE_HANDLER('7', receive_esc_param),                                 \
      RECEIVE_HANDLER('8', receive_esc_param),                                 \
      RECEIVE_HANDLER('9', receive_esc_param),                                 \
      RECEIVE_HANDLER(';', receive_esc_param_delimiter)

static const receive_table_t csi_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    ESC_PARAM_RECEIVE_TABLE,
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
    RECEIVE_HANDLER('x', receive_decreqtparm),
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
    ESC_PARAM_RECEIVE_TABLE,
    RECEIVE_HANDLER('h', receive_decsm),
    RECEIVE_HANDLER('l', receive_decrm),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t esc_hash_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('8', receive_decaln),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t esc_space_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('F', receive_s7c1t),
    RECEIVE_HANDLER('G', receive_s8c1t),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t scs_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('A', receive_scs_set),
    RECEIVE_HANDLER('B', receive_scs_set),
    RECEIVE_HANDLER('0', receive_scs_set),
    RECEIVE_HANDLER('1', receive_scs_set),
    RECEIVE_HANDLER('2', receive_scs_set),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t esc_percent_receive_table = {
    DEFAULT_RECEIVE_TABLE,
    RECEIVE_HANDLER('@', receive_charset_ascii),
    RECEIVE_HANDLER('G', receive_charset_utf8),
    DEFAULT_RECEIVE_HANDLER(receive_unexpected),
};

static const receive_table_t dcs_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_dcs_data),
};

static const receive_table_t osc_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_osc_data),
};

static const receive_table_t apc_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_apc_data),
};

static const receive_table_t pm_receive_table = {
    DEFAULT_RECEIVE_HANDLER(receive_pm_data),
};

void terminal_uart_transmit_character(struct terminal *terminal,
                                      character_t character) {

  character_t *buffer_head =
      terminal->transmit_buffer + terminal->transmit_buffer_head;

  buffer_head[0] = character;
  terminal->transmit_buffer_head++;

  if (terminal->transmit_buffer_head == terminal->transmit_buffer_size)
    terminal->transmit_buffer_head = 0;

  terminal->callbacks->uart_transmit(buffer_head, 1,
                                     terminal->transmit_buffer_head);
}

static const character_t
    eight_bit_character_table[CHARACTER_DECODER_TABLE_LENGTH] = {
        ['D'] = 0x84,  ['E'] = 0x85, ['H'] = 0x88, ['M'] = 0x8d,
        ['N'] = 0x8e,  ['O'] = 0x8f, ['P'] = 0x90, ['V'] = 0x96,
        ['W'] = 0x97,  ['X'] = 0x98, ['Z'] = 0x9a, ['['] = 0x9b,
        ['\\'] = 0x9c, [']'] = 0x9d, ['^'] = 0x9e, ['_'] = 0x9f,
};

void terminal_uart_transmit_string(struct terminal *terminal,
                                   const char *string) {

  character_t *buffer_head =
      terminal->transmit_buffer + terminal->transmit_buffer_head;
  size_t size = 0;

  while (*string) {
    if (terminal->c1_mode == C1_MODE_8BIT && *string == 0x1b) {
      const char *next = string + 1;
      character_t eight_bit_character =
          eight_bit_character_table[(size_t)*next];

      if (eight_bit_character) {
        buffer_head[size] = eight_bit_character;
        string += 2;
      } else {
        buffer_head[size] = *string;
        string++;
      }
    } else {
      buffer_head[size] = *string;
      string++;
    }

    size++;
    terminal->transmit_buffer_head++;

    if (terminal->transmit_buffer_head == terminal->transmit_buffer_size) {
      terminal->transmit_buffer_head = 0;

      terminal->callbacks->uart_transmit(buffer_head, size,
                                         terminal->transmit_buffer_head);

      buffer_head = terminal->transmit_buffer;
      size = 0;
    }
  }

  if (size)
    terminal->callbacks->uart_transmit(buffer_head, size,
                                       terminal->transmit_buffer_head);
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

void terminal_uart_xon_off(struct terminal *terminal, enum xon_off xon_off) {
  if (!terminal->lock_state.scroll && terminal->xon_off != xon_off) {
    terminal_uart_transmit_character(terminal,
                                     xon_off == XOFF ? CHAR_XOFF : CHAR_XON);
    terminal->xon_off = xon_off;
#ifdef DEBUG_LOG_XON_OFF
    printf(xon_off == XOFF ? "XOFF\r\n" : "XON\r\n");
#endif
  }
}

void terminal_uart_init(struct terminal *terminal) {
  if (terminal->charset == CHARSET_UTF8)
    terminal->receive_table = &utf8_prefix_receive_table;
  else
    terminal->receive_table = &ascii_receive_table;

  clear_esc_params(terminal);
  clear_utf8_buffer(terminal);
  clear_control_data(&terminal->dcs);
  clear_control_data(&terminal->osc);
  clear_control_data(&terminal->apc);
  clear_control_data(&terminal->pm);

  terminal->gset_received = GSET_UNDEFINED;
  terminal->xon_off = XON;

  terminal->vs.gset_gl = GSET_G0;
  memset(terminal->vs.gset_table, 0,
         GSET_MAX * sizeof(codepoint_transformation_table_t *));

#ifdef DEBUG
  memset(terminal->debug_buffer, 0, DEBUG_BUFFER_LENGTH);
  terminal->debug_buffer_length = 0;
  terminal->unhandled = false;
#endif
}
