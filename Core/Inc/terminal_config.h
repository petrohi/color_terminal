#pragma once

enum baud_rate {
  BAUD_RATE_110 = 0,
  BAUD_RATE_150 = 1,
  BAUD_RATE_300 = 2,
  BAUD_RATE_1200 = 3,
  BAUD_RATE_2400 = 4,
  BAUD_RATE_4800 = 5,
  BAUD_RATE_9600 = 6,
  BAUD_RATE_19200 = 7,
  BAUD_RATE_38400 = 8,
  BAUD_RATE_57600 = 9,
  BAUD_RATE_115200 = 10,
  BAUD_RATE_230400 = 11,
  BAUD_RATE_460800 = 12,
  BAUD_RATE_921600 = 13,
};

enum word_length {
  WORD_LENGTH_8B = 0,
  WORD_LENGTH_9B = 1,
};

enum stop_bits {
  STOP_BITS_1 = 0,
  STOP_BITS_2 = 1,
};

enum parity {
  PARITY_NONE = 0,
  PARITY_EVEN = 1,
  PARITY_ODD = 2,
};

struct terminal_config {
  enum baud_rate baud_rate;
  enum word_length word_length;
  enum stop_bits stop_bits;
  enum parity parity;
};

