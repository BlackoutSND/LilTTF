#if __has_include("LilTTF/FontParser.h")
  #include "LilTTF/FontParser.h"
  #include "LilTTF/FixedPoint.h"
#else
  #include "FontParser.h"
  #include "FixedPoint.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Static Scratch Buffers */
static Point global_point_buffer[MAX_POINTS_PER_GLYPH];
#if ENABLE_BEZIER_CURVES
static uint8_t global_onCurve_buffer[MAX_POINTS_PER_GLYPH];
#endif
static uint16_t global_contour_buffer[MAX_CONTOURS_PER_GLYPH];
static uint32_t global_table_offsets[NUM_TABLES];
static uint8_t global_scratch_flags[MAX_POINTS_PER_GLYPH];

#define LOG_ERROR(...) RASTERIZER_LOG(__VA_ARGS__)

static int global_point_count = 0;
static int global_contour_count = 0;

void ResetAllocator(void) {
  global_point_count = 0;
  global_contour_count = 0;
}

static bool AllocVectors(int count, Point **pts, uint8_t **onCurve) {
  if (global_point_count + count > MAX_POINTS_PER_GLYPH) {
    LOG_ERROR("Error: Points buffer exceeded (%d / %d)\n",
              global_point_count + count, MAX_POINTS_PER_GLYPH);
    return false;
  }
  *pts = &global_point_buffer[global_point_count];
#if ENABLE_BEZIER_CURVES
  if (onCurve) {
    *onCurve = &global_onCurve_buffer[global_point_count];
  }
#endif
  global_point_count += count;
  return true;
}

static uint16_t *AllocateContours(int count) {
  if (global_contour_count + count > MAX_CONTOURS_PER_GLYPH) {
    LOG_ERROR("Error: Contours buffer exceeded (%d / %d)\n",
              global_contour_count + count, MAX_CONTOURS_PER_GLYPH);
    return NULL;
  }
  uint16_t *ptr = &global_contour_buffer[global_contour_count];
  global_contour_count += count;
  return ptr;
}

static uint32_t SelectCmapSubtable(FontContext *context,
                                   uint16_t numberSubtables) {
  uint32_t cmapSubtableOffset = 0;
  int selectedUnicodeVersionID = -1;

  for (int i = 0; i < numberSubtables; i++) {
    uint16_t platformID = ReadUInt16(context);
    uint16_t platformSpecificID = ReadUInt16(context);
    uint32_t offset = ReadUInt32(context);

    if (platformID == 0) {
      if ((platformSpecificID == 0 || platformSpecificID == 1 ||
           platformSpecificID == 3 || platformSpecificID == 4) &&
          platformSpecificID > selectedUnicodeVersionID) {
        cmapSubtableOffset = offset;
        selectedUnicodeVersionID = platformSpecificID;
      }
    } else if (platformID == 3 && selectedUnicodeVersionID == -1) {
      if (platformSpecificID == 1 || platformSpecificID == 10) {
        cmapSubtableOffset = offset;
      }
    }
  }
  return cmapSubtableOffset;
}

static uint32_t GetGlyphIndexFormat4(FontContext *context, uint32_t charCode) {
  ReadUInt16(context); /* length */
  ReadUInt16(context); /* language */
  uint16_t segCount2X = ReadUInt16(context);
  uint16_t segCount = segCount2X / 2;
  ShiftReadPointer(context, 6);

  uint32_t endCodesPos = GetCurrentOffset(context);
  uint32_t startCodesPos = endCodesPos + segCount * 2 + 2;
  uint32_t idDeltasPos = startCodesPos + segCount * 2;
  uint32_t idRangeOffsetsPos = idDeltasPos + segCount * 2;

  int segmentIndex = -1;

  GoToOffset(context, endCodesPos);
  for (int i = 0; i < segCount; i++) {
    uint16_t endCode = ReadUInt16(context);
    if (endCode >= charCode) {
      segmentIndex = i;
      break;
    }
  }

  if (segmentIndex == -1)
    return 0;

  GoToOffset(context, startCodesPos + segmentIndex * 2);
  uint16_t startCode = ReadUInt16(context);
  if (startCode > charCode)
    return 0;

  GoToOffset(context, idDeltasPos + segmentIndex * 2);
  int16_t idDelta = ReadInt16(context);

  GoToOffset(context, idRangeOffsetsPos + segmentIndex * 2);
  uint32_t idRangeOffsetPtr = GetCurrentOffset(context);
  uint16_t idRangeOffset = ReadUInt16(context);

  if (idRangeOffset == 0) {
    return (charCode + idDelta) & 0xFFFF;
  } else {
    uint32_t glyphIndexAddr =
        idRangeOffsetPtr + idRangeOffset + 2 * (charCode - startCode);
    GoToOffset(context, glyphIndexAddr);
    uint16_t glyphIndex = ReadUInt16(context);
    if (glyphIndex != 0) {
      glyphIndex = (glyphIndex + idDelta) & 0xFFFF;
    }
    return glyphIndex;
  }
}

static uint32_t GetGlyphIndexFormat12(FontContext *context, uint32_t charCode) {
  ShiftReadPointer(context, 2); /* reserved */
  ReadUInt32(context);          /* length */
  ReadUInt32(context);          /* language */
  uint32_t numGroups = ReadUInt32(context);

  for (uint32_t i = 0; i < numGroups; i++) {
    uint32_t startCharCode = ReadUInt32(context);
    uint32_t endCharCode = ReadUInt32(context);
    uint32_t startGlyphIndex = ReadUInt32(context);

    if (charCode >= startCharCode && charCode <= endCharCode) {
      return startGlyphIndex + (charCode - startCharCode);
    }
  }
  return 0;
}

static uint32_t GetGlyphIndex(FontContext *context, uint32_t charCode) {
  uint32_t cmapOffset = global_table_offsets[TABLE_CMAP];
  if (cmapOffset == 0) {
    LOG_ERROR("CMAP table not found\n");
    return 0;
  }

  GoToOffset(context, cmapOffset);
  ReadUInt16(context); /* version */
  uint16_t numberSubtables = ReadUInt16(context);
  uint32_t subtableOffset = SelectCmapSubtable(context, numberSubtables);
  if (subtableOffset == 0)
    return 0;

  GoToOffset(context, cmapOffset + subtableOffset);
  uint16_t format = ReadUInt16(context);

  if (format == 4) {
    return GetGlyphIndexFormat4(context, charCode);
  } else if (format == 12) {
    return GetGlyphIndexFormat12(context, charCode);
  }

  return 0;
}

static uint32_t GetGlyphOffset(FontContext *context, uint32_t glyphIndex,
                               uint32_t *length) {
  uint32_t locaOffset = global_table_offsets[TABLE_LOCA];
  uint32_t glyfOffset = global_table_offsets[TABLE_GLYF];
  uint32_t headOffset = global_table_offsets[TABLE_HEAD];

  if (locaOffset == 0 || glyfOffset == 0 || headOffset == 0) {
    return (uint32_t)-1;
  }

  GoToOffset(context, headOffset + 50);
  int16_t indexToLocFormat = ReadInt16(context);

  uint32_t offset = 0;
  uint32_t nextOffset = 0;

  if (indexToLocFormat == 0) {
    GoToOffset(context, locaOffset + glyphIndex * 2);
    offset = (uint32_t)ReadUInt16(context) * 2;
    nextOffset = (uint32_t)ReadUInt16(context) * 2;
  } else {
    GoToOffset(context, locaOffset + glyphIndex * 4);
    offset = ReadUInt32(context);
    nextOffset = ReadUInt32(context);
  }

  if (length) {
    *length = nextOffset - offset;
  }

  return glyfOffset + offset;
}

bool GetGlyphMetrics(FontContext *context, uint32_t glyphIndex,
                     uint16_t *advanceWidth, int16_t *lsb) {
  uint32_t hheaOffset = global_table_offsets[TABLE_HHEA];
  uint32_t hmtxOffset = global_table_offsets[TABLE_HMTX];

  if (hheaOffset == 0 || hmtxOffset == 0) {
    LOG_ERROR("Metrics tables missing (hhea/hmtx)\n");
    return false;
  }

  uint32_t savedPos = GetCurrentOffset(context);
  GoToOffset(context, hheaOffset + 34);
  uint16_t numberOfHMetrics = ReadUInt16(context);

  GoToOffset(context, hmtxOffset);

  if (glyphIndex < numberOfHMetrics) {
    GoToOffset(context, hmtxOffset + glyphIndex * 4);
    if (advanceWidth)
      *advanceWidth = ReadUInt16(context);
    if (lsb)
      *lsb = ReadInt16(context);
  } else {
    GoToOffset(context, hmtxOffset + (numberOfHMetrics - 1) * 4);
    uint16_t lastAdvance = ReadUInt16(context);
    if (advanceWidth)
      *advanceWidth = lastAdvance;

    uint32_t lsbOffset =
        hmtxOffset + numberOfHMetrics * 4 + (glyphIndex - numberOfHMetrics) * 2;
    GoToOffset(context, lsbOffset);
    if (lsb)
      *lsb = ReadInt16(context);
  }

  GoToOffset(context, savedPos);
  return true;
}

static inline bool CheckBit(uint8_t flag, int bitIndex) {
  return ((flag >> bitIndex) & 1) == 1;
}

static void FillCoordinates(FontContext *context, uint8_t *flags, Point *points,
                            uint8_t *onCurves, int numPoints, bool isX) {
  int indexOfSizeFlag = isX ? 1 : 2;
  int indexOfSignOrSkipFlag = isX ? 4 : 5;
  int currentValue = 0;

  for (int i = 0; i < numPoints; i++) {
    uint8_t flag = flags[i];
    if (isX) {
#if ENABLE_BEZIER_CURVES
      if (onCurves) {
        onCurves[i] = CheckBit(flag, 0);
      }
#endif
    }
    if (CheckBit(flag, indexOfSizeFlag)) {
      uint8_t offset = ReadUInt8(context);
      int sign = CheckBit(flag, indexOfSignOrSkipFlag) ? 1 : -1;
      currentValue += sign * offset;
    } else if (!CheckBit(flag, indexOfSignOrSkipFlag)) {
      currentValue += ReadInt16(context);
    }
    if (isX) {
      points[i].x = currentValue;
    } else {
      points[i].y = currentValue;
    }
  }
}

static bool IsComplexGlyph(FontContext *context, uint32_t glyphOffset) {
  GoToOffset(context, glyphOffset);
  int16_t numContours = ReadInt16(context);
  return (numContours < 0);
}

static GlyphComponent GetGlyphComponent(FontContext *context,
                                        uint32_t glyphIndex, int depth);
static GlyphComponent GetSimpleGlyph(FontContext *context,
                                     uint32_t glyphOffset);

static GlyphComponent GetComplexGlyph(FontContext *context,
                                      uint32_t glyphOffset, int depth) {
  GlyphComponent compositeGlyph = {0};

  if (depth > MAX_RECURSION_DEPTH) {
    LOG_ERROR("Error: Max recursion depth exceeded\n");
    compositeGlyph.numberOfContours = -1;
    return compositeGlyph;
  }

  int initial_point_count = global_point_count;
  int initial_contour_count = global_contour_count;

  compositeGlyph.points = &global_point_buffer[initial_point_count];
#if ENABLE_BEZIER_CURVES
  compositeGlyph.onCurve = &global_onCurve_buffer[initial_point_count];
#endif
  compositeGlyph.endPtsOfContours =
      &global_contour_buffer[initial_contour_count];

  GoToOffset(context, glyphOffset);
  ShiftReadPointer(context, 10);

  uint16_t flags;
  uint16_t glyphIndex;

  do {
    flags = ReadUInt16(context);
    glyphIndex = ReadUInt16(context);
    int32_t arg1, arg2;

    if (flags & 0x0001) {
      arg1 = ReadInt16(context);
      arg2 = ReadInt16(context);
    } else {
      arg1 = ReadInt8(context);
      arg2 = ReadInt8(context);
    }

    FixedPoint m00 = FX_ONE, m01 = 0, m10 = 0, m11 = FX_ONE;

    if (flags & 0x0008) {
      m00 = m11 = F2Dot14ToFixed(ReadInt16(context));
    } else if (flags & 0x0040) {
      m00 = F2Dot14ToFixed(ReadInt16(context));
      m11 = F2Dot14ToFixed(ReadInt16(context));
    } else if (flags & 0x0080) {
      m00 = F2Dot14ToFixed(ReadInt16(context));
      m01 = F2Dot14ToFixed(ReadInt16(context));
      m10 = F2Dot14ToFixed(ReadInt16(context));
      m11 = F2Dot14ToFixed(ReadInt16(context));
    }

    uint32_t currentPos = GetCurrentOffset(context);
    GlyphComponent subGlyph = GetGlyphComponent(context, glyphIndex, depth + 1);
    GoToOffset(context, currentPos);

    if (subGlyph.numberOfContours == -1) {
      compositeGlyph.numberOfContours = -1;
      return compositeGlyph;
    }

    int xOffset = 0;
    int yOffset = 0;
    if (flags & 0x0002) {
      xOffset = arg1;
      yOffset = arg2;
    }

    for (unsigned int i = 0; i < subGlyph.numberOfPoints; i++) {
      int originalX = subGlyph.points[i].x;
      int originalY = subGlyph.points[i].y;

      int transX = FX_TO_INT_ROUND(originalX * m00 + originalY * m10);
      int transY = FX_TO_INT_ROUND(originalX * m01 + originalY * m11);

      subGlyph.points[i].x = transX + xOffset;
      subGlyph.points[i].y = transY + yOffset;
    }

    int pointOffset = subGlyph.points - compositeGlyph.points;
    for (int i = 0; i < subGlyph.numberOfContours; i++) {
      subGlyph.endPtsOfContours[i] += pointOffset;
    }

  } while (flags & 0x0020);

  if (flags & 0x0100) {
    uint16_t numInstr = ReadUInt16(context);
    ShiftReadPointer(context, numInstr);
  }

  compositeGlyph.numberOfPoints = global_point_count - initial_point_count;
  compositeGlyph.numberOfContours =
      global_contour_count - initial_contour_count;

  return compositeGlyph;
}

static GlyphComponent GetGlyphComponent(FontContext *context,
                                        uint32_t glyphIndex, int depth) {
  uint32_t length = 0;
  uint32_t glyphOffset = GetGlyphOffset(context, glyphIndex, &length);

  if (length == 0 || glyphOffset == (uint32_t)-1) {
    return (GlyphComponent){.numberOfContours = 0, .points = NULL, .endPtsOfContours = NULL};
  }

  if (IsComplexGlyph(context, glyphOffset)) {
    return GetComplexGlyph(context, glyphOffset, depth);
  } else {
    return GetSimpleGlyph(context, glyphOffset);
  }
}

static GlyphComponent GetSimpleGlyph(FontContext *context,
                                     uint32_t glyphOffset) {
  GoToOffset(context, glyphOffset);
  int16_t numContours = ReadInt16(context);

  uint16_t *endPtsOfContours = AllocateContours(numContours);

  if (!endPtsOfContours) {
    return (GlyphComponent){.numberOfContours = -1};
  }

  ReadInt16(context); /* xMin */
  ReadInt16(context); /* yMin */
  ReadInt16(context); /* xMax */
  ReadInt16(context); /* yMax */

  for (int i = 0; i < numContours; i++) {
    endPtsOfContours[i] = ReadInt16(context);
  }

  int numPoints = (numContours > 0) ? (endPtsOfContours[numContours - 1] + 1) : 0;
  if (numPoints > MAX_POINTS_PER_GLYPH) {
    LOG_ERROR("Global flag scratch/point buffer exceeded limit\n");
    return (GlyphComponent){.numberOfContours = -1};
  }

  uint8_t *flags = global_scratch_flags;
  ShiftReadPointer(context, ReadInt16(context));

  for (int i = 0; i < numPoints; i++) {
    flags[i] = ReadUInt8(context);
    if (CheckBit(flags[i], 3)) {
      int repeatCount = ReadUInt8(context);
      for (int j = 0; j < repeatCount; j++) {
        i++;
        if (i >= numPoints)
          break;
        flags[i] = flags[i - 1];
      }
    }
  }

  Point *points;
  uint8_t *onCurves = NULL;
  if (!AllocVectors(numPoints, &points, &onCurves)) {
    return (GlyphComponent){.numberOfContours = -1};
  }

  FillCoordinates(context, flags, points, onCurves, numPoints, true);
  FillCoordinates(context, flags, points, NULL, numPoints, false);

  GlyphComponent glyphComponent = {.endPtsOfContours = endPtsOfContours,
                                   .numberOfContours = numContours,
                                   .points = points,
#if ENABLE_BEZIER_CURVES
                                   .onCurve = onCurves,
#else
                                   .onCurve = NULL,
#endif
                                   .numberOfPoints = numPoints};
  return glyphComponent;
}

bool ParseTableDirectory(FontContext *context) {
  GoToOffset(context, 4);
  if (context->size < 12)
    return false;

  uint16_t numTables = ReadUInt16(context);
  ShiftReadPointer(context, 6); /* searchRange, entrySelector, rangeShift */

  for (int i = 0; i < NUM_TABLES; i++) {
    global_table_offsets[i] = 0;
  }

  for (size_t i = 0; i < numTables; i++) {
    uint32_t tag = ReadUInt32(context);
    ReadUInt32(context); /* Skip checkSum */
    uint32_t offset = ReadUInt32(context);
    ReadUInt32(context); /* Skip length */

    /* CMAP: 0x636D6170 */
    if (tag == 0x636D6170) {
      global_table_offsets[TABLE_CMAP] = offset;
    }
    /* LOCA: 0x6C6F6361 */
    else if (tag == 0x6C6F6361) {
      global_table_offsets[TABLE_LOCA] = offset;
    }
    /* HEAD: 0x68656164 */
    else if (tag == 0x68656164) {
      global_table_offsets[TABLE_HEAD] = offset;

      uint32_t currentPos = GetCurrentOffset(context);
      GoToOffset(context, offset + 18);
      context->unitsPerEm = ReadUInt16(context);
      GoToOffset(context, currentPos);
    }
    /* GLYF: 0x676C7966 */
    else if (tag == 0x676C7966) {
      global_table_offsets[TABLE_GLYF] = offset;
    }
    /* MAXP: 0x6D617870 */
    else if (tag == 0x6D617870) {
      global_table_offsets[TABLE_MAXP] = offset;
    }
    /* HHEA: 0x68686561 */
    else if (tag == 0x68686561) {
      global_table_offsets[TABLE_HHEA] = offset;

      uint32_t currentPos = GetCurrentOffset(context);
      GoToOffset(context, offset + 4);
      context->ascender = ReadInt16(context);
      context->descender = ReadInt16(context);
      GoToOffset(context, currentPos);
    }
    /* HMTX: 0x686D7478 */
    else if (tag == 0x686D7478) {
      global_table_offsets[TABLE_HMTX] = offset;
    }
  }

  /* Making sure key tables were found */
  return (global_table_offsets[TABLE_CMAP] != 0 &&
          global_table_offsets[TABLE_LOCA] != 0 &&
          global_table_offsets[TABLE_HEAD] != 0 &&
          global_table_offsets[TABLE_GLYF] != 0);
}

uint32_t GetGlyphIndexForChar(FontContext *context, uint32_t charCode) {
  if (!context)
    return 0;
  return GetGlyphIndex(context, charCode);
}

GlyphComponent GetGlyphForIndex(FontContext *context, uint32_t glyphIndex) {
  GlyphComponent empty = {.numberOfContours = -1, .points = NULL, .endPtsOfContours = NULL};
  if (context == NULL)
    return empty;
  ResetAllocator();

  uint32_t length = 0;
  uint32_t glyphOffset = GetGlyphOffset(context, glyphIndex, &length);

  if (length == 0 || glyphOffset == (uint32_t)-1) {
    return (GlyphComponent){
        .numberOfContours = 0, .points = NULL, .endPtsOfContours = NULL};
  }

  if (IsComplexGlyph(context, glyphOffset)) {
    return GetComplexGlyph(context, glyphOffset, 0);
  } else {
    return GetSimpleGlyph(context, glyphOffset);
  }
}
