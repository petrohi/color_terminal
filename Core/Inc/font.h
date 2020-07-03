#include <stdint.h>

#include "Bdf2C/font.h"

extern const struct bitmap_font normal_font;
extern const struct bitmap_font bold_font;

const unsigned char *find_glyph(const struct bitmap_font *font,
                                unsigned short codepoint);
