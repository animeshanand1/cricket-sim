#pragma once
// JavaScript number semantics, so the C++ engine computes exactly what the JS engine computes.
// See docs/cpp-migration/05-engine-port-guide.md section 1.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace t20 {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Math.imul
inline int32_t js_imul(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
// 2654435761 as Math.imul sees it (ToInt32)
inline constexpr int32_t kGolden = static_cast<int32_t>(2654435761u);
// x >>> n for an int32 x
inline uint32_t js_ushr(int32_t x, int n) { return static_cast<uint32_t>(x) >> n; }
// (a + b) | 0 without signed overflow
inline int32_t js_add32(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
// x | 0 (ToInt32)
int32_t js_toint32(double x);

// Math.round: halves go up (towards +infinity), unlike std::round
inline double js_round(double x) {
  const double r = std::floor(x);
  return (x - r >= 0.5) ? r + 1.0 : r;
}

// Math.max(lo, Math.min(hi, v)); NaN propagates like JS for the engine's purposes
inline double clamp(double v, double lo, double hi) {
  if (std::isnan(v)) return v;
  return std::max(lo, std::min(hi, v));
}

// Math.exp as V8 computes it: fdlibm's __ieee754_exp, with exp(1) === Math.E.
double js_exp(double x);

// Number.prototype.toString()
std::string js_num(double x);
// Number.prototype.toFixed(digits): rounds the exact binary value, ties away from zero
std::string js_fixed(double x, int digits);
// toLocaleString('en-IN') for integers: 1,00,000 grouping
std::string fmt_indian(double n);
// toLocaleString('en-US') for integers: 100,000 grouping
std::string fmt_thousands(double n);

// "Ab" -> "ab" (ASCII only; every engine label is ASCII)
std::string lower_ascii(std::string s);
std::string lower_first(std::string s);

}  // namespace t20
