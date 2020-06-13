#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct TerminalCallbacks {
  void (*keyboard_set_leds)(bool caps, bool scroll, bool num);
  void (*uart_transmit)(uint8_t *data, uint16_t size);
} TerminalCallbacks_t;

typedef struct LockState {
  uint8_t caps : 1;
  uint8_t scroll : 1;
  uint8_t num : 1;
} LockState_t;

#define TRANSMIT_BUFFER_SIZE 256

typedef struct Terminal {
  const TerminalCallbacks_t *callbacks;
  uint8_t pressed_key_code;
  LockState_t lock_state;
  uint8_t shift_state : 1;
  uint8_t alt_state : 1;
  uint8_t ctrl_state : 1;
  uint8_t transmit_buffer[TRANSMIT_BUFFER_SIZE];
} Terminal_t;

void Terminal_Init(Terminal_t *terminal, const TerminalCallbacks_t *callbacks);
void Terminal_HandleKey(Terminal_t *terminal, uint8_t key);
void Terminal_HandleShift(Terminal_t *terminal, bool shift);
void Terminal_HandleAlt(Terminal_t *terminal, bool alt);
void Terminal_HandleCtrl(Terminal_t *terminal, bool ctrl);
