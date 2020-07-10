#include "terminal_config_ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#define PRINTF_BUFFER_SIZE 256

static size_t current_baud_rate(struct terminal_config_ui *terminal_config_ui) {
  return terminal_config_ui->terminal_config_copy.baud_rate;
}

static size_t
current_word_length(struct terminal_config_ui *terminal_config_ui) {
  return terminal_config_ui->terminal_config_copy.word_length;
}

static size_t current_stop_bits(struct terminal_config_ui *terminal_config_ui) {
  return terminal_config_ui->terminal_config_copy.stop_bits;
}

static size_t current_parity(struct terminal_config_ui *terminal_config_ui) {
  return terminal_config_ui->terminal_config_copy.parity;
}

static void change_baud_rate(struct terminal_config_ui *terminal_config_ui,
                             size_t baud_rate) {
  terminal_config_ui->terminal_config_copy.baud_rate = baud_rate;
}

static void change_word_length(struct terminal_config_ui *terminal_config_ui,
                               size_t word_length) {
  terminal_config_ui->terminal_config_copy.word_length = word_length;
}

static void change_stop_bits(struct terminal_config_ui *terminal_config_ui,
                             size_t stop_bits) {
  terminal_config_ui->terminal_config_copy.stop_bits = stop_bits;
}

static void change_parity(struct terminal_config_ui *terminal_config_ui,
                          size_t parity) {
  terminal_config_ui->terminal_config_copy.parity = parity;
}

static const struct terminal_ui_menu menus[] = {
    {"Serial",
     &(struct terminal_ui_option[]){
         {"Baud rate", current_baud_rate, change_baud_rate,
          &(const struct terminal_ui_choice[]){
              [BAUD_RATE_110] = {"110"},
              [BAUD_RATE_150] = {"150"},
              [BAUD_RATE_300] = {"300"},
              [BAUD_RATE_1200] = {"1200"},
              [BAUD_RATE_2400] = {"2400"},
              [BAUD_RATE_4800] = {"4800"},
              [BAUD_RATE_9600] = {"9600"},
              [BAUD_RATE_19200] = {"19200"},
              [BAUD_RATE_38400] = {"38400"},
              [BAUD_RATE_57600] = {"57600"},
              [BAUD_RATE_115200] = {"115200"},
              [BAUD_RATE_230400] = {"230400"},
              [BAUD_RATE_460800] = {"460800"},
              [BAUD_RATE_921600] = {"921600"},
              {NULL},
          }},
         {"Word length", current_word_length, change_word_length,
          &(const struct terminal_ui_choice[]){[WORD_LENGTH_8B] = {"8 bits"},
                                               [WORD_LENGTH_9B] = {"9 bits"},
                                               {NULL}}},
         {"Stop bits", current_stop_bits, change_stop_bits,
          &(const struct terminal_ui_choice[]){
              [STOP_BITS_1] = {"1 bit"}, [STOP_BITS_2] = {"2 bits"}, {NULL}}},
         {"Parity", current_parity, change_parity,
          &(const struct terminal_ui_choice[]){[PARITY_NONE] = {"none"},
                                               [PARITY_EVEN] = {"even"},
                                               [PARITY_ODD] = {"odd"},
                                               {NULL}}},
         {NULL}}},
    {"Terminal",
     &(struct terminal_ui_option[]){
         {"Newline mode", current_baud_rate, change_baud_rate,
          &(const struct terminal_ui_choice[]){
              {"off"},
              {"on"},
              {NULL},
          }},
         {"VT52 mode", current_baud_rate, change_baud_rate,
          &(const struct terminal_ui_choice[]){
              {"off"},
              {"on"},
              {NULL},
          }},
         {NULL}}},
    {NULL}};

void screen_printf(struct terminal_config_ui *terminal_config_ui,
                   const char *format, ...) {
  va_list args;
  va_start(args, format);

  char buffer[PRINTF_BUFFER_SIZE];
  vsnprintf(buffer, PRINTF_BUFFER_SIZE, format, args);
  terminal_uart_receive_string(terminal_config_ui->terminal, buffer);

  va_end(args);
}

void terminal_config_ui_init(struct terminal_config_ui *terminal_config_ui,
                             struct terminal *terminal,
                             struct terminal_config *terminal_config) {
  terminal_config_ui->terminal = terminal;
  terminal_config_ui->terminal_config = terminal_config;

  terminal_config_ui->activated = false;

  terminal_config_ui->current_menu = &menus[0];
  terminal_config_ui->current_option =
      &(*terminal_config_ui->current_menu->options)[0];
  terminal_config_ui->current_choice = NULL;
}

static void clear_screen(struct terminal_config_ui *terminal_config_ui) {
  screen_printf(terminal_config_ui, "\x1b[2J");
}

static void disable_cursor(struct terminal_config_ui *terminal_config_ui) {
  screen_printf(terminal_config_ui, "\x1b[?25l");
}

static void enable_cursor(struct terminal_config_ui *terminal_config_ui) {
  screen_printf(terminal_config_ui, "\x1b[?25h");
}

static void move_cursor(struct terminal_config_ui *terminal_config_ui,
                        size_t row, size_t col) {
  screen_printf(terminal_config_ui, "\x1b[%d;%dH", row, col);
}

static const char* choice_help[] = {
    "↑↓ - Change value",
    "<Enter> - OK",
    "<Esc> - Cancel",
    NULL,
};

static const char* default_help[] = {
    "←→ Select menu",
    "↑↓ - Select option",
    "<Enter> - Edit option",
    "<Esc> - Exit without saving",
    "<F12> - Save and restart",
    NULL,
};

#define TITLE_ROW 2
#define MENU_ROW 4
#define TOP_ROW 5
#define MAIN_ROW 8
#define BOTTOM_ROW 24

#define LEFT_COL 1
#define OPTIONS_COL 4
#define CHOICES_COL 36
#define DIVIDER_COL 50
#define HELP_COL 52
#define RIGHT_COL 80

static void clear_help(struct terminal_config_ui * terminal_config_ui) {
  const char** help = 
    terminal_config_ui->current_choice ? &choice_help[0] : &default_help[0];

  size_t i = 0;
  while (*help) {
    move_cursor(terminal_config_ui, i + MAIN_ROW, HELP_COL);
    screen_printf(terminal_config_ui, "\x1b[0K");
    help++;
    i++;
  }
}

static void render_help(struct terminal_config_ui * terminal_config_ui) {
  const char** help = 
    terminal_config_ui->current_choice ? &choice_help[0] : &default_help[0];

  size_t i = 0;
  while (*help) {
    move_cursor(terminal_config_ui, i + MAIN_ROW, HELP_COL);
    screen_printf(terminal_config_ui, *help);
    help++;
    i++;
  }
}

static void render_screen(struct terminal_config_ui *terminal_config_ui) {
  move_cursor(terminal_config_ui, TITLE_ROW, 30);
  screen_printf(terminal_config_ui, "\x1b[1mASCII TERMINAL SETUP\x1b[22m");

  const struct terminal_ui_menu *menu =
      &menus[0];

  size_t col = 1;
  while (menu->title) {
    bool current_menu = menu == terminal_config_ui->current_menu;
    move_cursor(terminal_config_ui, MENU_ROW, col);
    screen_printf(terminal_config_ui, "\x1b[%dm%s\x1b[27m",
                  current_menu ? 7 : 27, menu->title);

    menu++;
    col += (strlen(menu->title) + 1);
  }

  move_cursor(terminal_config_ui, TOP_ROW, LEFT_COL);
  screen_printf(terminal_config_ui, "╔");
  move_cursor(terminal_config_ui, TOP_ROW, RIGHT_COL);
  screen_printf(terminal_config_ui, "╗");

  for (size_t row = TOP_ROW + 1; row <= BOTTOM_ROW - 1; row++) {
    move_cursor(terminal_config_ui, row, LEFT_COL);
    screen_printf(terminal_config_ui, "║");
    move_cursor(terminal_config_ui, row, DIVIDER_COL);
    screen_printf(terminal_config_ui, "│");
    move_cursor(terminal_config_ui, row, RIGHT_COL);
    screen_printf(terminal_config_ui, "║");
  }

  move_cursor(terminal_config_ui, 24, LEFT_COL);
  screen_printf(terminal_config_ui, "╚");
  move_cursor(terminal_config_ui, 24, RIGHT_COL);
  screen_printf(terminal_config_ui, "╝");

  for (size_t col = LEFT_COL + 1; col <= RIGHT_COL - 1; col++) {
    move_cursor(terminal_config_ui, TOP_ROW, col);
    if (col == DIVIDER_COL)
      screen_printf(terminal_config_ui, "╤");
    else
      screen_printf(terminal_config_ui, "═");
    move_cursor(terminal_config_ui, BOTTOM_ROW, col);
    if (col == DIVIDER_COL)
      screen_printf(terminal_config_ui, "╧");
    else
      screen_printf(terminal_config_ui, "═");
  }

  size_t i = 0;
  const struct terminal_ui_option *option =
      &(*terminal_config_ui->current_menu->options)[0];

  while (option->title) {
    bool current_option = option == terminal_config_ui->current_option;

    move_cursor(terminal_config_ui, i + MAIN_ROW, OPTIONS_COL);
    screen_printf(terminal_config_ui, "\x1b[%dm%s:\x1b[27m",
                  !terminal_config_ui->current_choice && current_option ? 7 : 27,
                  option->title);

    move_cursor(terminal_config_ui, i + MAIN_ROW, CHOICES_COL);
    const struct terminal_ui_choice *choice =
        terminal_config_ui->current_choice && current_option
            ? terminal_config_ui->current_choice
            : &(*option->choices)[option->current(terminal_config_ui)];

    screen_printf(terminal_config_ui, "\x1b[%dm[%10s]\x1b[27m",
                  terminal_config_ui->current_choice && current_option ? 7 : 27,
                  choice->title);

    i++;
    option++;
  }

  render_help(terminal_config_ui);
}

static void prev_option(struct terminal_config_ui *terminal_config_ui) {
  if (terminal_config_ui->current_choice) {
    const struct terminal_ui_choice *first_choice =
        &(*terminal_config_ui->current_option->choices)[0];
    if (terminal_config_ui->current_choice != first_choice)
      terminal_config_ui->current_choice--;
    else {
      terminal_config_ui->current_choice = first_choice;
      while ((terminal_config_ui->current_choice + 1)->title)
        terminal_config_ui->current_choice++;
    }
  } else {
    const struct terminal_ui_option *first_option =
        &(*terminal_config_ui->current_menu->options)[0];
    if (terminal_config_ui->current_option != first_option)
      terminal_config_ui->current_option--;
    else {
      terminal_config_ui->current_option = first_option;
      while ((terminal_config_ui->current_option + 1)->title)
        terminal_config_ui->current_option++;
    }
  }

  render_screen(terminal_config_ui);
}

static void next_option(struct terminal_config_ui *terminal_config_ui) {
  if (terminal_config_ui->current_choice) {
    terminal_config_ui->current_choice++;

    if (!terminal_config_ui->current_choice->title)
      terminal_config_ui->current_choice =
          &(*terminal_config_ui->current_option->choices)[0];
  } else {
    terminal_config_ui->current_option++;

    if (!terminal_config_ui->current_option->title)
      terminal_config_ui->current_option =
          &(*terminal_config_ui->current_menu->options)[0];
  }

  render_screen(terminal_config_ui);
}

static void prev_menu(struct terminal_config_ui *terminal_config_ui) {
  if (!terminal_config_ui->current_choice) {
    const struct terminal_ui_menu *first_menu = &menus[0];
    if (terminal_config_ui->current_menu != first_menu)
      terminal_config_ui->current_menu--;
    else {
      terminal_config_ui->current_menu = first_menu;
      while ((terminal_config_ui->current_menu + 1)->title)
        terminal_config_ui->current_menu++;
    }

    terminal_config_ui->current_option =
        &(*terminal_config_ui->current_menu->options)[0];

    clear_screen(terminal_config_ui);
    render_screen(terminal_config_ui);
  }
}

static void next_menu(struct terminal_config_ui *terminal_config_ui) {
  if (!terminal_config_ui->current_choice) {
    terminal_config_ui->current_menu++;

    if (!terminal_config_ui->current_menu->title)
      terminal_config_ui->current_menu = &menus[0];

    terminal_config_ui->current_option =
        &(*terminal_config_ui->current_menu->options)[0];

    clear_screen(terminal_config_ui);
    render_screen(terminal_config_ui);
  }
}

void terminal_config_ui_enter(struct terminal_config_ui *terminal_config_ui) {
  memcpy(&terminal_config_ui->terminal_config_copy,
         terminal_config_ui->terminal_config, sizeof(struct terminal_config));
  terminal_uart_xon_off(terminal_config_ui->terminal, XOFF);
  terminal_config_ui->activated = true;

  disable_cursor(terminal_config_ui);
  screen_printf(terminal_config_ui, "\x1b[97;48;5;54m");
  clear_screen(terminal_config_ui);
  render_screen(terminal_config_ui);
}

static void enter(struct terminal_config_ui *terminal_config_ui) {
  clear_help(terminal_config_ui);

  if (terminal_config_ui->current_choice) {
    terminal_config_ui->current_option->change(
        terminal_config_ui,
        terminal_config_ui->current_choice -
            &(*terminal_config_ui->current_option->choices)[0]);
    terminal_config_ui->current_choice = NULL;
  } else
    terminal_config_ui->current_choice =
        &(*terminal_config_ui->current_option
               ->choices)[terminal_config_ui->current_option->current(
            terminal_config_ui)];

  render_screen(terminal_config_ui);
}

static void
write_terminal_config(struct terminal_config_ui *terminal_config_ui) {
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR);
  FLASH_Erase_Sector(FLASH_SECTOR_11, VOLTAGE_RANGE_3);

  for (size_t i = 0; i < sizeof(struct terminal_config); ++i)
    HAL_FLASH_Program(
        TYPEPROGRAM_BYTE,
        (uint32_t)((uint8_t *)terminal_config_ui->terminal_config + i),
        *(((uint8_t *)&terminal_config_ui->terminal_config_copy) + i));

  HAL_FLASH_Lock();
}

static void leave(struct terminal_config_ui *terminal_config_ui) {
  if (terminal_config_ui->current_choice) {
    clear_help(terminal_config_ui);

    terminal_config_ui->current_choice = NULL;

    render_screen(terminal_config_ui);
  } else {
    screen_printf(terminal_config_ui, "\x1b[0m");
    clear_screen(terminal_config_ui);
    move_cursor(terminal_config_ui, 1, 1);
    enable_cursor(terminal_config_ui);

    terminal_config_ui->activated = false;
    terminal_uart_xon_off(terminal_config_ui->terminal, XON);
  }
}

static void apply(struct terminal_config_ui *terminal_config_ui) {
  screen_printf(terminal_config_ui, "\x1b[0m");
  clear_screen(terminal_config_ui);
  move_cursor(terminal_config_ui, 1, 1);
  screen_printf(terminal_config_ui, "Writing config...");

  write_terminal_config(terminal_config_ui);
  terminal_uart_xon_off(terminal_config_ui->terminal, XON);
  screen_printf(terminal_config_ui, "\033c");
}

static bool match(character_t *characters, size_t size, const char *string) {
  size_t l = strlen(string);
  if (size == l && strncmp(string, (const char *)characters, size) == 0)
    return true;

  return false;
}

static const char *LEAVE = "\x1b";
static const char *ENTER = "\r";
static const char *UP = "\x1b[A";
static const char *DOWN = "\x1b[B";
static const char *LEFT = "\x1b[D";
static const char *RIGHT = "\x1b[C";
static const char *APPLY = "\x1b[24~";

void terminal_config_ui_receive_characters(
    struct terminal_config_ui *terminal_config_ui, character_t *characters,
    size_t size) {
  if (match(characters, size, ENTER))
    enter(terminal_config_ui);
  else if (match(characters, size, LEAVE))
    leave(terminal_config_ui);
  else if (match(characters, size, UP))
    prev_option(terminal_config_ui);
  else if (match(characters, size, DOWN))
    next_option(terminal_config_ui);
  else if (match(characters, size, LEFT))
    prev_menu(terminal_config_ui);
  else if (match(characters, size, RIGHT))
    next_menu(terminal_config_ui);
  else if (match(characters, size, APPLY))
    apply(terminal_config_ui);
}
