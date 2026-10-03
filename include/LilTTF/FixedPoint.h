#ifndef LILTTF_FIXED_POINT_H
#define LILTTF_FIXED_POINT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 32-bit Q16.16 Signed Fixed-Point Representation
 * Dynamic range: [-32768.0, +32767.99998] with 1/65536 precision.
 */
typedef int32_t FixedPoint;

#define FX_SHIFT 16
#define FX_ONE   ((FixedPoint)(1 << FX_SHIFT))
#define FX_HALF  ((FixedPoint)(1 << (FX_SHIFT - 1)))

/* Conversion macros */
#define INT_TO_FX(x)       ((FixedPoint)((int32_t)(x) << FX_SHIFT))
#define FX_TO_INT(x)       ((int)((x) >> FX_SHIFT))
#define FX_TO_INT_ROUND(x) ((int)(((x) + FX_HALF) >> FX_SHIFT))

/**
 * @brief Convert TrueType F2Dot14 (2.14 signed fixed-point) to Q16.16 FixedPoint.
 */
static inline FixedPoint F2Dot14ToFixed(int16_t value) {
  return (FixedPoint)((int32_t)value << 2);
}

/**
 * @brief Signed Q16.16 division using 64-bit intermediate numerator.
 */
static inline FixedPoint FX_DIV(FixedPoint a, FixedPoint b) {
  if (b == 0) {
    return (a >= 0) ? (FixedPoint)0x7FFFFFFF : (FixedPoint)(-0x7FFFFFFF - 1);
  }
  return (FixedPoint)(((int64_t)a << FX_SHIFT) / b);
}

#ifdef __cplusplus
}
#endif

#endif /* LILTTF_FIXED_POINT_H */
