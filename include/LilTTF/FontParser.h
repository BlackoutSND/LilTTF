#ifndef LILTTF_FONT_PARSER_H
#define LILTTF_FONT_PARSER_H

#include "Config.h"
#include "FileReader.h"
#include "Font.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TABLE_CMAP 0
#define TABLE_LOCA 1
#define TABLE_HEAD 2
#define TABLE_GLYF 3
#define TABLE_MAXP 4
#define TABLE_HHEA 5
#define TABLE_HMTX 6
#define NUM_TABLES 7

/**
 * @brief Parse the TrueType table directory.
 *
 * @param context Font reading context.
 * @return true if key tables were located, false otherwise.
 */
bool ParseTableDirectory(FontContext *context);

/**
 * @brief Map a Unicode character codepoint to its glyph index via 'cmap'.
 *
 * @param context   Font reading context.
 * @param charCode  Unicode codepoint (ASCII or decoded UTF-8/UTF-32).
 * @return Glyph index (0 if not found / .notdef glyph).
 */
uint32_t GetGlyphIndexForChar(FontContext *context, uint32_t charCode);

/**
 * @brief Parse outline points and contours for a glyph index (simple or composite).
 *
 * @param context     Font reading context.
 * @param glyphIndex  TrueType glyph index.
 * @return Decomposed GlyphComponent structure.
 */
GlyphComponent GetGlyphForIndex(FontContext *context, uint32_t glyphIndex);

/**
 * @brief Read horizontal metrics (advance width and left side bearing) from 'hmtx'.
 *
 * @param context       Font reading context.
 * @param glyphIndex    TrueType glyph index.
 * @param advanceWidth  [out] Advance width in font design units.
 * @param lsb           [out] Left side bearing in font design units.
 * @return true on success, false if tables are invalid.
 */
bool GetGlyphMetrics(FontContext *context, uint32_t glyphIndex,
                     uint16_t *advanceWidth, int16_t *lsb);

/**
 * @brief Reset scratchpad buffer allocators for the next glyph parse.
 */
void ResetAllocator(void);

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_FONT_PARSER_H */
