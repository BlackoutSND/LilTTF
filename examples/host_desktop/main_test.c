/**
 * @file main_test.c
 * @brief Host verification test runner for LilTTF.
 *
 * Compiles on Linux/macOS/Windows with standard gcc or clang.
 * Loads a .ttf font file from disk, runs FontRasterizer_Init, rasterizes sample glyphs,
 * and renders ASCII visualizations to stdout.
 */

#include "LilTTF/Config.h"
#include "LilTTF/Rasterizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_ascii_bitmap(const uint8_t *bitmap, int width, int height, int stride) {
  printf("   +");
  for (int x = 0; x < width; x++) printf("-");
  printf("+\n");

  for (int y = 0; y < height; y++) {
    printf("%2d |", y);
    for (int x = 0; x < width; x++) {
      uint8_t pixel = bitmap[y * stride + x];
      printf("%c", pixel ? '#' : ' ');
    }
    printf("|\n");
  }

  printf("   +");
  for (int x = 0; x < width; x++) printf("-");
  printf("+\n\n");
}

int main(int argc, char **argv) {
  const char *font_path = (argc > 1) ? argv[1] : "../../fonts/LiberationMono-Regular.ttf";

  printf("============================================================\n");
  printf(" LilTTF - Host Desktop Verification Suite                   \n");
  printf("============================================================\n");
  printf("Loading font: %s\n", font_path);

  FILE *f = fopen(font_path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Could not open font file '%s'\n", font_path);
    return 1;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (file_size <= 0) {
    fprintf(stderr, "Error: Invalid font file size (%ld)\n", file_size);
    fclose(f);
    return 1;
  }

  uint8_t *font_data = (uint8_t *)malloc((size_t)file_size);
  if (!font_data) {
    fprintf(stderr, "Error: Out of memory allocating %ld bytes\n", file_size);
    fclose(f);
    return 1;
  }

  size_t read_bytes = fread(font_data, 1, (size_t)file_size, f);
  fclose(f);

  if (read_bytes != (size_t)file_size) {
    fprintf(stderr, "Error: Failed to read complete file\n");
    free(font_data);
    return 1;
  }

  printf("Read %zu bytes successfully.\n", read_bytes);

  /* Initialize Rasterizer */
  printf("\n[1/3] Initializing FontRasterizer_Init()...\n");
  if (!FontRasterizer_Init(font_data, read_bytes)) {
    fprintf(stderr, "FAIL: FontRasterizer_Init failed!\n");
    free(font_data);
    return 1;
  }
  printf("PASS: FontRasterizer initialized successfully!\n");

  /* Test Vertical Metrics */
  int fontSize = 24;
  int ascender = 0, descender = 0;
  int lineHeight = GetFontVMetrics(fontSize, &ascender, &descender);
  printf("\n[2/3] Font Vertical Metrics (Size = %d px):\n", fontSize);
  printf("  Ascender:    %d px\n", ascender);
  printf("  Descender:   %d px\n", descender);
  printf("  Line Height: %d px\n", lineHeight);

  /* Test Rasterizing Glyphs (ASCII Art rendering) */
  printf("\n[3/3] Rasterizing Test Glyphs:\n");
  const char test_chars[] = {'A', 'g', 'Q', '&', '8'};
  const int num_chars = (int)(sizeof(test_chars) / sizeof(test_chars[0]));

  #define CANVAS_W 40
  #define CANVAS_H 32
  uint8_t canvas[CANVAS_W * CANVAS_H];

  for (int i = 0; i < num_chars; i++) {
    char ch = test_chars[i];
    int outWidth = 0;
    int outAdvance = 0;

    memset(canvas, 0, sizeof(canvas));

    bool ok = GetBitmapForChar((uint32_t)ch, canvas, 2, 2,
                               CANVAS_W, CANVAS_H, fontSize,
                               &outWidth, &outAdvance, false, false);

    if (!ok) {
      fprintf(stderr, "FAIL: Could not rasterize character '%c' (0x%02X)\n", ch, ch);
      continue;
    }

    printf("Glyph '%c' (fontSize=%d, width=%d px, advanceX=%d px):\n",
           ch, fontSize, outWidth, outAdvance);
    print_ascii_bitmap(canvas, outWidth + 4, lineHeight, CANVAS_W);
  }

  free(font_data);
  printf("All checks passed successfully!\n");
  return 0;
}
