#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  FONT_NORMAL,
  FONT_BOLD,
  FONT_ITALIC,
} Font;

#define COLS 80
#define ROWS 24

#define CHAR_WIDTH 7
#define CHAR_HEIGHT 14

#define SCREEN_WIDTH (COLS * CHAR_WIDTH)
#define SCREEN_HEIGHT (ROWS * CHAR_HEIGHT)

typedef uint8_t Pixel;

Pixel screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

void DrawCharacter(Pixel *screen_buffer, size_t row, size_t col, uint8_t character, Font font, bool underlined);
void TestFonts(Pixel *screen_buffer);