#ifndef LILTTF_CONFIG_H
#define LILTTF_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*                          FEATURE TOGGLES (0 or 1)                         */
/* ========================================================================= */

/**
 * @brief Enable quadratic Bézier curve decomposition.
 * 1: Smooth curves via recursive midpoint subdivision.
 * 0: Direct linear segments between on-curve control points (reduces memory usage).
 */
#ifndef ENABLE_BEZIER_CURVES
#define ENABLE_BEZIER_CURVES 1
#endif

/**
 * @brief Fill rule selection.
 * 1: Non-Zero Winding rule (proper TrueType specification compliance).
 * 0: Even-Odd fill rule (faster, lower memory overhead).
 */
#ifndef USE_NON_ZERO_WINDING
#define USE_NON_ZERO_WINDING 1
#endif

/**
 * @brief Automatic optimal buffer sizing toggle.
 * 1: Derive MAX_POINTS_PER_GLYPH and MAX_CONTOURS_PER_GLYPH directly from FontData.h.
 * 0: Use manual fallback buffer limits defined below.
 */
#ifndef USE_OPTIMAL_SIZES
#define USE_OPTIMAL_SIZES 0
#endif

/* ========================================================================= */
/*                         RASTERIZATION PARAMETERS                          */
/* ========================================================================= */

/**
 * @brief Additional typographical letter spacing in pixels.
 */
#ifndef LETTER_SPACING
#define LETTER_SPACING 1
#endif

/**
 * @brief Maximum recursion depth for composite/compound TrueType glyphs.
 */
#ifndef MAX_RECURSION_DEPTH
#define MAX_RECURSION_DEPTH 8
#endif

#if ENABLE_BEZIER_CURVES
/**
 * @brief Curve flatness threshold in Q24.8 fixed-point format (x * 256).
 * 64 = 0.25 px error threshold, 128 = 0.50 px error threshold.
 */
#ifndef BEZIER_FLATTEN_THRESHOLD
#define BEZIER_FLATTEN_THRESHOLD 64
#endif

/**
 * @brief Maximum recursive subdivision depth for quadratic Bézier curves.
 */
#ifndef BEZIER_MAX_DEPTH
#define BEZIER_MAX_DEPTH 3
#endif
#endif /* ENABLE_BEZIER_CURVES */

/* ========================================================================= */
/*                          STATIC BUFFER LIMITS                             */
/* ========================================================================= */

#if USE_OPTIMAL_SIZES
  /* In optimal mode, FontData.h provides MAX_POINTS_PER_GLYPH and MAX_CONTOURS_PER_GLYPH */
  #if __has_include("FontData.h")
    #include "FontData.h"
  #endif
#endif

#ifndef MAX_POINTS_PER_GLYPH
#define MAX_POINTS_PER_GLYPH 256
#endif

#ifndef MAX_CONTOURS_PER_GLYPH
#define MAX_CONTOURS_PER_GLYPH 16
#endif

#if ENABLE_BEZIER_CURVES
/**
 * @brief Maximum flattened vertices generated per glyph during Bézier decomposition.
 */
#ifndef MAX_FLATTENED_POINTS
#define MAX_FLATTENED_POINTS 512
#endif
#endif /* ENABLE_BEZIER_CURVES */

/* ========================================================================= */
/*                              LOGGING HOOKS                                */
/* ========================================================================= */

#if defined(RASTERIZER_ENABLE_LOGGING)
  #include <stdio.h>
  #define RASTERIZER_LOG(...) printf(__VA_ARGS__)
#else
  #define RASTERIZER_LOG(...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_CONFIG_H */
