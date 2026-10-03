#if __has_include("LilTTF/Rasterizer.h")
  #include "LilTTF/Rasterizer.h"
  #include "LilTTF/FileReader.h"
  #include "LilTTF/FixedPoint.h"
  #include "LilTTF/Font.h"
  #include "LilTTF/FontParser.h"
#else
  #include "Rasterizer.h"
  #include "FileReader.h"
  #include "FixedPoint.h"
  #include "Font.h"
  #include "FontParser.h"
#endif

#include <string.h>

static FontContext _globalFontContext;

bool FontRasterizer_Init(const uint8_t *fontData, size_t size) {
  if (!fontData || size < 12)
    return false;
  _globalFontContext = InitFontContext(fontData, size);
  return ParseTableDirectory(&_globalFontContext);
}

#if USE_NON_ZERO_WINDING
typedef struct {
  int x;
  int dir;
} ScanlineIntersection;
#endif

#if ENABLE_BEZIER_CURVES
static Point flat_points[MAX_FLATTENED_POINTS];
static uint16_t flat_contour_endpoints[128];
static int num_flat_points = 0;

static void EmitPoint(int x, int y) {
  if (num_flat_points < MAX_FLATTENED_POINTS) {
    flat_points[num_flat_points].x = (int16_t)x;
    flat_points[num_flat_points].y = (int16_t)y;
    num_flat_points++;
  }
}

static void EmitBezier(int x0, int y0, int x1, int y1, int x2, int y2,
                       int depth) {
  int dx = x0 - 2 * x1 + x2;
  int dy = y0 - 2 * y1 + y2;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;

  if (depth >= BEZIER_MAX_DEPTH || (dx + dy) < BEZIER_FLATTEN_THRESHOLD) {
    EmitPoint(x2, y2);
    return;
  }

  int x01 = (x0 + x1) >> 1;
  int y01 = (y0 + y1) >> 1;
  int x12 = (x1 + x2) >> 1;
  int y12 = (y1 + y2) >> 1;
  int x012 = (x01 + x12) >> 1;
  int y012 = (y01 + y12) >> 1;

  EmitBezier(x0, y0, x01, y01, x012, y012, depth + 1);
  EmitBezier(x012, y012, x12, y12, x2, y2, depth + 1);
}
#endif

static void RasterizeGlyph(GlyphComponent *glyph, uint8_t *bitmap, int startX,
                           int startY, int bufferWidth, int bufferHeight,
                           FixedPoint invScale, FixedPoint fAscender,
                           bool is4Bit, bool blend) {
  if (!blend && bitmap && bufferWidth > 0 && bufferHeight > 0) {
    memset(bitmap, 0, (size_t)bufferWidth * (size_t)bufferHeight);
  }

  if (glyph->numberOfContours <= 0 || glyph->numberOfPoints == 0 ||
      glyph->points == NULL)
    return;

  if (invScale <= 0)
    invScale = FX_ONE;

  for (unsigned int i = 0; i < glyph->numberOfPoints; i++) {
    FixedPoint fx = INT_TO_FX(glyph->points[i].x);
    FixedPoint fy = INT_TO_FX(glyph->points[i].y);
    FixedPoint pX = FX_DIV(fx, invScale);
    FixedPoint pY = FX_DIV(fAscender - fy, invScale);
    glyph->points[i].x = (int16_t)(pX >> 8);
    glyph->points[i].y = (int16_t)(pY >> 8);
  }

  Point *rasterPoints = glyph->points;
  uint16_t *rasterEndPts = glyph->endPtsOfContours;
  int rasterContours = glyph->numberOfContours;
  unsigned int rasterPointsCount = glyph->numberOfPoints;

#if ENABLE_BEZIER_CURVES
  if (glyph->onCurve != NULL) {
    num_flat_points = 0;
    int num_flat_contours = 0;

    for (int c = 0; c < glyph->numberOfContours; c++) {
      int contourStart = (c == 0) ? 0 : glyph->endPtsOfContours[c - 1] + 1;
      int contourEnd = glyph->endPtsOfContours[c];
      int n = contourEnd - contourStart + 1;

      if (n <= 0)
        continue;

      int firstOnCurve = -1;
      for (int i = 0; i < n; i++) {
        if (glyph->onCurve[contourStart + i]) {
          firstOnCurve = i;
          break;
        }
      }
      if (firstOnCurve == -1)
        continue;

      int curr = firstOnCurve;
      int startPtX = glyph->points[contourStart + curr].x;
      int startPtY = glyph->points[contourStart + curr].y;
      EmitPoint(startPtX, startPtY);

      int lastX = startPtX;
      int lastY = startPtY;

      for (int i = 1; i <= n; i++) {
        int next = (firstOnCurve + i) % n;
        int ptX = glyph->points[contourStart + next].x;
        int ptY = glyph->points[contourStart + next].y;

        if (glyph->onCurve[contourStart + next]) {
          EmitPoint(ptX, ptY);
          lastX = ptX;
          lastY = ptY;
        } else {
          int next2 = (firstOnCurve + i + 1) % n;
          int pt2X = glyph->points[contourStart + next2].x;
          int pt2Y = glyph->points[contourStart + next2].y;

          if (glyph->onCurve[contourStart + next2]) {
            EmitBezier(lastX, lastY, ptX, ptY, pt2X, pt2Y, 0);
            lastX = pt2X;
            lastY = pt2Y;
            i++;
          } else {
            int impX = (ptX + pt2X) >> 1;
            int impY = (ptY + pt2Y) >> 1;
            EmitBezier(lastX, lastY, ptX, ptY, impX, impY, 0);
            lastX = impX;
            lastY = impY;
          }
        }
      }
      if (num_flat_contours < 128 && num_flat_points > 0) {
        flat_contour_endpoints[num_flat_contours++] = (uint16_t)(num_flat_points - 1);
      }
    }
    rasterPoints = flat_points;
    rasterEndPts = flat_contour_endpoints;
    rasterContours = num_flat_contours;
    rasterPointsCount = (unsigned int)num_flat_points;
  }
#endif

  int minX = 0x7FFFFFFF;
  int maxX = -0x7FFFFFFF - 1;
  int minY = 0x7FFFFFFF;
  int maxY = -0x7FFFFFFF - 1;

  for (unsigned int i = 0; i < rasterPointsCount; i++) {
    int px = rasterPoints[i].x >> 8;
    int py = rasterPoints[i].y >> 8;

    if (px < minX)
      minX = px;
    if (px > maxX)
      maxX = px;
    if (py < minY)
      minY = py;
    if (py > maxY)
      maxY = py;
  }

  minX -= 1;
  maxX += 1;
  minY -= 1;
  maxY += 1;

  if (minX < 0)
    minX = 0;
  if (minY < 0)
    minY = 0;

  for (int y = minY; y <= maxY; y++) {
    int py = (y * 256) + 128;

#if USE_NON_ZERO_WINDING
    ScanlineIntersection intersections[64];
#else
    int intersections[64];
#endif
    int numIntersections = 0;
    int contourStart = 0;

    for (int c = 0; c < rasterContours; c++) {
      int contourEnd = rasterEndPts[c];
      int numPointsInContour = contourEnd - contourStart + 1;

      for (int i = 0; i < numPointsInContour; i++) {
        int p1 = contourStart + i;
        int p2 = contourStart + ((i + 1) % numPointsInContour);

        int x1 = rasterPoints[p1].x;
        int y1 = rasterPoints[p1].y;
        int x2 = rasterPoints[p2].x;
        int y2 = rasterPoints[p2].y;

        if ((y1 > py) != (y2 > py)) {
          int dy = y2 - y1;
          if (dy != 0) {
            int intersectX = x1 + (py - y1) * (x2 - x1) / dy;
            if (numIntersections < 64) {
#if USE_NON_ZERO_WINDING
              intersections[numIntersections].x = intersectX;
              intersections[numIntersections].dir = (y2 > y1) ? 1 : -1;
              numIntersections++;
#else
              intersections[numIntersections++] = intersectX;
#endif
            }
          }
        }
      }
      contourStart = contourEnd + 1;
    }

    for (int i = 1; i < numIntersections; i++) {
#if USE_NON_ZERO_WINDING
      int keyX = intersections[i].x;
      int keyDir = intersections[i].dir;
      int j = i - 1;
      while (j >= 0 && intersections[j].x > keyX) {
        intersections[j + 1].x = intersections[j].x;
        intersections[j + 1].dir = intersections[j].dir;
        j = j - 1;
      }
      intersections[j + 1].x = keyX;
      intersections[j + 1].dir = keyDir;
#else
      int key = intersections[i];
      int j = i - 1;
      while (j >= 0 && intersections[j] > key) {
        intersections[j + 1] = intersections[j];
        j = j - 1;
      }
      intersections[j + 1] = key;
#endif
    }

    for (int x = minX; x <= maxX; x++) {
      int px = (x * 256) + 128;
#if USE_NON_ZERO_WINDING
      int winding = 0;

      for (int i = 0; i < numIntersections; i++) {
        if (px < intersections[i].x)
          break;
        winding += intersections[i].dir;
      }
      bool hit = (winding != 0);
#else
      int crossings = 0;

      for (int i = 0; i < numIntersections; i++) {
        if (px < intersections[i])
          break;
        crossings++;
      }
      bool hit = ((crossings % 2) == 1);
#endif

      if (hit) {
        int drawX = startX + x;
        int drawY = startY + y;

        if (drawY >= 0 && drawY < bufferHeight) {
          if (is4Bit) {
            if (drawX >= 0 && drawX < bufferWidth * 2) {
              int byteIdx = (drawY * bufferWidth) + (drawX / 2);
              if (drawX % 2 == 0) {
                bitmap[byteIdx] |= 0xF0;
              } else {
                bitmap[byteIdx] |= 0x0F;
              }
            }
          } else {
            if (drawX >= 0 && drawX < bufferWidth) {
              bitmap[drawY * bufferWidth + drawX] = 1;
            }
          }
        }
      }
    }
  }
}

bool GetBitmapForChar(uint32_t charCode, uint8_t *outputBitmap, int startX,
                      int startY, int bufferWidth, int bufferHeight,
                      int fontSize, int *outWidth, int *outAdvanceX,
                      bool is4Bit, bool blend) {
  /* Validate context and destination buffer */
  if (_globalFontContext.data == NULL || !outputBitmap)
    return false;

  /* Basic dimension and bounds checks */
  if (fontSize <= 0 || fontSize > 180 || bufferHeight <= 0 ||
      bufferHeight > 180)
    return false;

  if (bufferWidth < 0 || (is4Bit && bufferWidth == 0) || bufferWidth > 180)
    return false;

  FontContext *context = &_globalFontContext;

  uint32_t glyphIndex = GetGlyphIndexForChar(context, charCode);
  GlyphComponent glyph = GetGlyphForIndex(context, glyphIndex);

  uint16_t advanceWidth = 0;
  int16_t lsb = 0;
  GetGlyphMetrics(context, glyphIndex, &advanceWidth, &lsb);

  uint16_t upem = context->unitsPerEm;
  if (upem == 0)
    upem = 1000;

  int bitmapMaxWidth = advanceWidth;

  if (glyph.numberOfPoints > 0) {
    int maxX = 0;
    for (unsigned int i = 0; i < glyph.numberOfPoints; i++) {
      if (glyph.points[i].x > maxX) {
        maxX = glyph.points[i].x;
      }
    }

    if (glyphIndex == 0) {
      uint16_t padding = upem / 10;
      advanceWidth = (uint16_t)(maxX + padding);
      bitmapMaxWidth = advanceWidth;
    } else if (maxX > advanceWidth) {
      bitmapMaxWidth = maxX;
    }
  }

  FixedPoint invScale = FX_DIV(INT_TO_FX(upem), INT_TO_FX(fontSize));

  FixedPoint fAdvanceX = FX_DIV(INT_TO_FX(advanceWidth), invScale);
  int advanceX = FX_TO_INT(fAdvanceX) + LETTER_SPACING;

  FixedPoint fPixelWidth = FX_DIV(INT_TO_FX(bitmapMaxWidth), invScale);
  int pixelWidth = FX_TO_INT(fPixelWidth) + LETTER_SPACING;

  if (!is4Bit && bufferWidth == 0) {
    bufferWidth = pixelWidth;
    if (bufferWidth < 1)
      bufferWidth = 1;
  }

  if (outWidth) {
    int maxW = is4Bit ? (bufferWidth * 2) : bufferWidth;
    if (bufferWidth > 0 && pixelWidth > maxW) {
      *outWidth = maxW;
    } else {
      *outWidth = pixelWidth;
    }
  }

  if (outAdvanceX)
    *outAdvanceX = advanceX;

  FixedPoint fAscender = INT_TO_FX(context->ascender);

  RasterizeGlyph(&glyph, outputBitmap, startX, startY, bufferWidth,
                 bufferHeight, invScale, fAscender, is4Bit, blend);

  return true;
}

int GetFontVMetrics(int fontSize, int *ascender, int *descender) {
  FontContext *ctx = &_globalFontContext;
  if (fontSize <= 0 || ctx->data == NULL)
    return 0;

  if (ctx->unitsPerEm == 0)
    return fontSize;

  if (ascender)
    *ascender = (int)((int64_t)ctx->ascender * fontSize / ctx->unitsPerEm);
  if (descender)
    *descender = (int)((int64_t)ctx->descender * fontSize / ctx->unitsPerEm);

  int totalUnits = ctx->ascender - ctx->descender;
  int height = (int)((int64_t)totalUnits * fontSize / ctx->unitsPerEm);
  return height + 2; /* padding */
}
