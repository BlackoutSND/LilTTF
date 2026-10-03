#ifndef LILTTF_FILE_READER_H
#define LILTTF_FILE_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Font parser reading context wrapping a raw TrueType binary in memory.
 */
typedef struct {
  const uint8_t *data; /**< Pointer to base of TrueType font data */
  size_t size;         /**< Total byte length of font buffer */
  size_t offset;       /**< Current sequential byte cursor offset */
  uint16_t unitsPerEm; /**< TrueType grid resolution (from 'head' table) */
  int16_t ascender;    /**< Scaled typographic ascender (from 'hhea' table) */
  int16_t descender;   /**< Scaled typographic descender (from 'hhea' table) */
} FontContext;

/**
 * @brief Initialize a reading context for a font buffer.
 */
FontContext InitFontContext(const uint8_t *data, size_t size);

/* Stream reading functions with bounds checking */
uint8_t  ReadUInt8(FontContext *context);
uint16_t ReadUInt16(FontContext *context);
int16_t  ReadInt16(FontContext *context);
int8_t   ReadInt8(FontContext *context);
uint32_t ReadUInt32(FontContext *context);

/* Cursor navigation */
void ShiftReadPointer(FontContext *context, int offset);
void GoToOffset(FontContext *context, int offset);
uint32_t GetCurrentOffset(FontContext *context);

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_FILE_READER_H */
