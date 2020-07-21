#include <stdint.h>
#include <stdio.h>

#include "rgb.h"

#define RED_LUMINANCE 0.2126
#define GREEN_LUMINANCE 0.7152
#define BLUE_LUMINANCE 0.0722

int main() {
  printf("#define LUMNINANCE_TABLE_SIZE %d\r\n\r\n", RGB_TABLE_SIZE);
  printf("static const uint8_t luminance_table[LUMNINANCE_TABLE_SIZE] = {\r\n");
  for (size_t i = 0; i < RGB_TABLE_SIZE; ++i) {
    rgb_t rgb = rgb_table[i];
    printf("  %d,\r\n", (uint8_t)((float)(rgb & 0xff) * BLUE_LUMINANCE +
                                  (float)((rgb >> 8) & 0xff) * GREEN_LUMINANCE +
                                  (float)((rgb >> 16) & 0xff) * RED_LUMINANCE));
  }
  printf("};\r\n");
  return 0;
}
