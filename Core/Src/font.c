#include "font.h"

#include <stdlib.h>

const uint8_t *find_glyph(const struct bitmap_font *font,
                          unsigned short codepoint) {
  size_t first = 0;
  size_t last = font->codepoints_length - 1;
  size_t middle = (first + last) / 2;

  while (first <= last) {
    if (font->codepoints[middle] < codepoint)
      first = middle + 1;
    else if (font->codepoints[middle] == codepoint)
      return font->data + (font->codepoints_map[middle] * font->height);
    else
      last = middle - 1;

    middle = (first + last) / 2;
  }

  return NULL;
}

#include "FontProblems/bold.h"
#include "FontProblems/normal.h"

const struct bitmap_font normal_font = {
    .height = normal_font_height,
    .width = 7,
    .data = normal_font_data,
    .codepoints_length = sizeof(normal_font_codepoints) / sizeof(int),
    .codepoints = normal_font_codepoints,
    .codepoints_map = normal_font_codepoints_map,
};

const struct bitmap_font bold_font = {
    .height = bold_font_height,
    .width = 7,
    .data = bold_font_data,
    .codepoints_length = sizeof(bold_font_codepoints) / sizeof(int),
    .codepoints = bold_font_codepoints,
    .codepoints_map = bold_font_codepoints_map,
};
