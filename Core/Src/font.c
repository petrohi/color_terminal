#include "font.h"

#include <stdlib.h>

const unsigned char *find_glyph(const struct bitmap_font *font,
                                unsigned short codepoint) {
  size_t first = 0;
  size_t last = font->Chars - 1;
  size_t middle = (first + last) / 2;

  while (first <= last) {
    if (font->Index[middle] < codepoint)
      first = middle + 1;
    else if (font->Index[middle] == codepoint)
      return font->Bitmap + (middle * font->Height);
    else
      last = middle - 1;

    middle = (first + last) / 2;
  }

  return NULL;
}
