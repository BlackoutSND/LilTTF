#ifndef LILTTF_RASTERIZER_H
#define LILTTF_RASTERIZER_H

#include "Config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the font rasterizer with TrueType font binary data.
 *
 * Locates required TrueType tables and sets up the internal context.
 * Can be called at runtime to switch between different typefaces.
 *
 * @param fontData Pointer to TTF binary in Flash or RAM.
 * @param size     Total size of the TTF binary in bytes.
 * @return true if required tables were found and validated, false otherwise.
 */
bool FontRasterizer_Init(const uint8_t *fontData, size_t size);

/**
 * @brief Rasterize a single character glyph into a destination pixel buffer.
 *
 * @param charCode     Unicode code point (ASCII or decoded UTF-8/UTF-32).
 * @param outputBitmap Destination pixel buffer.
 * @param startX       Horizontal pixel offset in the destination buffer.
 * @param startY       Vertical pixel offset in the destination buffer.
 * @param bufferWidth  Row width (in bytes for 4-bit, or pixels for 1-bit; 0 = auto in 1-bit).
 * @param bufferHeight Destination buffer height in pixels.
 * @param fontSize     Target glyph height in pixels.
 * @param outWidth     [out] Rendered glyph width in pixels (optional, can be NULL).
 * @param outAdvanceX  [out] Typographical advance cursor offset in pixels (optional, can be NULL).
 * @param is4Bit       true: packed 4-bit grayscale (2 px/byte); false: 1-bit monochrome (1 byte/px).
 * @param blend        true: OR-blend into existing buffer; false: clear buffer area first.
 * @return true on success, false on error or invalid parameters.
 */
bool GetBitmapForChar(uint32_t charCode, uint8_t *outputBitmap, int startX,
                      int startY, int bufferWidth, int bufferHeight,
                      int fontSize, int *outWidth, int *outAdvanceX,
                      bool is4Bit, bool blend);

/**
 * @brief Compute scaled vertical typographical metrics for a given font size.
 *
 * @param fontSize   Requested font size in pixels.
 * @param ascender   [out] Scaled distance from baseline to top of highest glyph in pixels (optional).
 * @param descender  [out] Scaled distance from baseline to lowest descent in pixels (optional).
 * @return Total recommended line height in pixels (including default safety padding).
 */
int GetFontVMetrics(int fontSize, int *ascender, int *descender);

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_RASTERIZER_H */
