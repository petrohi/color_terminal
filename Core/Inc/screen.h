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

void ClearScreen(Pixel *screen_buffer, Pixel inactive);

void DrawCharacter(Pixel *screen_buffer, size_t row, size_t col,
                   uint8_t character, Font font, bool underlined, Pixel active,
                   Pixel inactive);

void TestFonts(Pixel *screen_buffer);

void TestMandelbrot(Pixel *screen_buffer, float window_x, float window_y,
                    float window_r, bool (*cancel)());

void TestColors(Pixel *screen_buffer);
