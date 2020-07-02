#include <stdint.h>

#define FONT_WIDTH 7
#define FONT_HEIGHT 13

typedef uint8_t FontRow;

extern const FontRow normal_font_data[];
extern const FontRow italic_font_data[];
extern const FontRow bold_font_data[];
