#ifndef LILTTF_FONT_H
#define LILTTF_FONT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 2D point coordinates.
 */
typedef struct Point {
  int16_t x;
  int16_t y;
} Point;

/**
 * @brief Decomposed TrueType glyph vector outline.
 */
typedef struct GlyphComponent {
  Point *points;               /**< Pointer to vertex coordinate array */
  uint8_t *onCurve;            /**< Flag array indicating on-curve vs off-curve control points */
  unsigned int numberOfPoints; /**< Total number of vertices */
  int numberOfContours;        /**< Total count of closed outline contours */
  uint16_t *endPtsOfContours;  /**< Endpoint indices for each contour */
} GlyphComponent;

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_FONT_H */
