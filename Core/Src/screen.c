#include "screen.h"

#include "font.h"

void DrawCharacter(Pixel *screen_buffer, size_t row, size_t col, uint8_t character, Font font, bool underlined) {
  if (character >= 0x20) {
    character -= 0x1f;
  }
  else {
    character = 0;
  }

  size_t base = ((row * COLS * CHAR_HEIGHT) + col) * CHAR_WIDTH;
  const FontRow *font_data = (font == FONT_BOLD ? FontBoldData : (font == FONT_ITALIC ? FontItalicData : FontData));

  for (size_t char_y = 0; char_y < CHAR_HEIGHT; char_y++) {
    for (size_t char_x = 0; char_x < CHAR_WIDTH; char_x++) {

      size_t i = base + COLS * CHAR_WIDTH * char_y + char_x;

      Pixel pixel = 0;

      if (
        (underlined && char_y == CHAR_HEIGHT - 1) || 
        (char_x < FONT_WIDTH && font_data[character * FONT_HEIGHT + char_y] & (1 << char_x))) {
        pixel = 0xff;
      }

      screen_buffer[i] = pixel;
    }
  }
}

void TestFonts(Pixel *screen_buffer) {
  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row, col, character, FONT_NORMAL, false);
    }
  }

  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row, col + 16, character, FONT_BOLD, false);
    }
  }

  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row, col + 32, character, FONT_ITALIC, false);
    }
  }

  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row + 8, col, character, FONT_NORMAL, true);
    }
  }

  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row + 8, col + 16, character, FONT_BOLD, true);
    }
  }

  for (size_t row = 0; row < 8; row++) {
    for (size_t col = 0; col < 16; col++) {
      uint8_t character = ((row * 16) + col);

      DrawCharacter(screen_buffer, row + 8, col + 32, character, FONT_ITALIC, true);
    }
  }
}
