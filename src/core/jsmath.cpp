#include "core/jsmath.hpp"

#include <charconv>
#include <cstring>
#include <system_error>

namespace t20 {

int32_t js_toint32(double x) {
  if (!std::isfinite(x)) return 0;
  double t = std::trunc(x);
  double m = std::fmod(t, 4294967296.0);
  if (m < 0) m += 4294967296.0;
  return static_cast<int32_t>(static_cast<uint32_t>(m));
}

// ---------------------------------------------------------------------------
// fdlibm e_exp.c, as ported in V8 (src/base/ieee754.cc). Verified against node 22's Math.exp
// on 2.7 million inputs: identical except exp(1), which V8 returns as Math.E.
// ---------------------------------------------------------------------------
namespace {

inline uint32_t hiWord(double x) {
  uint64_t b;
  std::memcpy(&b, &x, sizeof b);
  return static_cast<uint32_t>(b >> 32);
}
inline uint32_t loWord(double x) {
  uint64_t b;
  std::memcpy(&b, &x, sizeof b);
  return static_cast<uint32_t>(b);
}
inline double withHiWord(double x, uint32_t hi) {
  uint64_t b;
  std::memcpy(&b, &x, sizeof b);
  b = (static_cast<uint64_t>(hi) << 32) | (b & 0xffffffffull);
  std::memcpy(&x, &b, sizeof b);
  return x;
}

constexpr double kOne = 1.0;
constexpr double kHalF[2] = {0.5, -0.5};
constexpr double kHuge = 1.0e+300;
constexpr double kTwom1000 = 9.33263618503218878990e-302;
constexpr double kOThreshold = 7.09782712893383973096e+02;
constexpr double kUThreshold = -7.45133219101941108420e+02;
constexpr double kLn2HI[2] = {6.93147180369123816490e-01, -6.93147180369123816490e-01};
constexpr double kLn2LO[2] = {1.90821492927058770002e-10, -1.90821492927058770002e-10};
constexpr double kInvLn2 = 1.44269504088896338700e+00;
constexpr double kP1 = 1.66666666666666019037e-01;
constexpr double kP2 = -2.77777777770155933842e-03;
constexpr double kP3 = 6.61375632143793436117e-05;
constexpr double kP4 = -1.65339022054652515390e-06;
constexpr double kP5 = 4.13813679705723846039e-08;
constexpr double kE = 2.718281828459045;

}  // namespace

double js_exp(double x) {
  double y, hi = 0.0, lo = 0.0, c, t;
  int32_t k = 0;
  uint32_t hx = hiWord(x);
  const int xsb = static_cast<int>((hx >> 31) & 1);
  hx &= 0x7fffffffu;

  if (hx >= 0x40862E42u) {  // |x| >= 709.78...
    if (hx >= 0x7ff00000u) {
      if (((hx & 0xfffffu) | loWord(x)) != 0) return x + x;  // NaN
      return (xsb == 0) ? x : 0.0;                            // exp(+-inf) = {inf, 0}
    }
    if (x > kOThreshold) return kHuge * kHuge;          // overflow
    if (x < kUThreshold) return kTwom1000 * kTwom1000;  // underflow
  }

  if (hx > 0x3fd62e42u) {     // |x| > 0.5 ln2
    if (hx < 0x3FF0A2B2u) {   // and |x| < 1.5 ln2
      if (x == 1.0) return kE;
      hi = x - kLn2HI[xsb];
      lo = kLn2LO[xsb];
      k = 1 - xsb - xsb;
    } else {
      k = static_cast<int32_t>(kInvLn2 * x + kHalF[xsb]);
      t = k;
      hi = x - t * kLn2HI[0];  // t*ln2HI is exact here
      lo = t * kLn2LO[0];
    }
    x = hi - lo;
  } else if (hx < 0x3e300000u) {  // |x| < 2**-28
    if (kHuge + x > kOne) return kOne + x;
  } else {
    k = 0;
  }

  t = x * x;
  c = x - t * (kP1 + t * (kP2 + t * (kP3 + t * (kP4 + t * kP5))));
  if (k == 0) return kOne - ((x * c) / (c - 2.0) - x);
  y = kOne - ((lo - (x * c) / (2.0 - c)) - hi);
  if (k >= -1021) return withHiWord(y, hiWord(y) + (static_cast<uint32_t>(k) << 20));
  return withHiWord(y, hiWord(y) + (static_cast<uint32_t>(k + 1000) << 20)) * kTwom1000;
}

// ---------------------------------------------------------------------------
// Number formatting
// ---------------------------------------------------------------------------

std::string js_num(double x) {
  if (std::isnan(x)) return "NaN";
  if (x == 0) return "0";  // also -0
  if (std::isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
  std::string sign;
  if (x < 0) {
    sign = "-";
    x = -x;
  }
  // Shortest round-trip digits and exponent, then ECMAScript Number::toString layout.
  char buf[64];
  auto res = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
  std::string sci(buf, res.ptr);
  const auto epos = sci.find('e');
  std::string mant = sci.substr(0, epos);
  const int exp10 = std::stoi(sci.substr(epos + 1));
  std::string digits;
  for (char ch : mant)
    if (ch != '.') digits += ch;
  const int k = static_cast<int>(digits.size());
  const int n = exp10 + 1;  // position of the decimal point
  std::string out;
  if (k <= n && n <= 21) {
    out = digits + std::string(static_cast<size_t>(n - k), '0');
  } else if (0 < n && n <= 21) {
    out = digits.substr(0, static_cast<size_t>(n)) + "." + digits.substr(static_cast<size_t>(n));
  } else if (-6 < n && n <= 0) {
    out = "0." + std::string(static_cast<size_t>(-n), '0') + digits;
  } else {
    const int e = n - 1;
    out = digits.substr(0, 1);
    if (k > 1) out += "." + digits.substr(1);
    out += "e";
    out += (e >= 0 ? "+" : "-");
    out += std::to_string(e >= 0 ? e : -e);
  }
  return sign + out;
}

std::string js_fixed(double x, int digits) {
  if (std::isnan(x)) return "NaN";
  if (std::abs(x) >= 1e21) return js_num(x);
  std::string sign;
  if (x < 0) {  // as in JS, (-0.04).toFixed(1) is "-0.0"
    x = -x;
    sign = "-";
  }
  // Exact decimal expansion of the double (enough digits for any value), then round half up.
  static thread_local char buf[1600];
  auto res = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::fixed, 1100);
  std::string s(buf, res.ptr);
  const auto dot = s.find('.');
  std::string intPart = s.substr(0, dot);
  std::string frac = s.substr(dot + 1);
  const bool up = frac[static_cast<size_t>(digits)] >= '5';
  std::string kept = intPart + frac.substr(0, static_cast<size_t>(digits));
  if (up) {
    int i = static_cast<int>(kept.size()) - 1;
    while (i >= 0) {
      if (kept[static_cast<size_t>(i)] == '9') {
        kept[static_cast<size_t>(i)] = '0';
        --i;
      } else {
        kept[static_cast<size_t>(i)] += 1;
        break;
      }
    }
    if (i < 0) kept.insert(kept.begin(), '1');
  }
  const size_t intLen = kept.size() - static_cast<size_t>(digits);
  std::string out = kept.substr(0, intLen);
  if (digits > 0) out += "." + kept.substr(intLen);
  return sign + out;
}

namespace {
std::string groupDigits(double n, bool indian) {
  const long long v = static_cast<long long>(js_round(n));
  std::string d = std::to_string(v < 0 ? -v : v);
  std::string out;
  const int len = static_cast<int>(d.size());
  if (len <= 3) {
    out = d;
  } else {
    std::string last3 = d.substr(static_cast<size_t>(len - 3));
    std::string rest = d.substr(0, static_cast<size_t>(len - 3));
    const int group = indian ? 2 : 3;
    std::string grouped;
    int count = 0;
    for (int i = static_cast<int>(rest.size()) - 1; i >= 0; --i) {
      grouped.insert(grouped.begin(), rest[static_cast<size_t>(i)]);
      if (++count % group == 0 && i > 0) grouped.insert(grouped.begin(), ',');
    }
    out = grouped + "," + last3;
  }
  return (v < 0 ? "-" : "") + out;
}
}  // namespace

std::string fmt_indian(double n) { return groupDigits(n, true); }
std::string fmt_thousands(double n) { return groupDigits(n, false); }

std::string lower_ascii(std::string s) {
  for (char& ch : s)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return s;
}

std::string lower_first(std::string s) {
  if (!s.empty() && s[0] >= 'A' && s[0] <= 'Z') s[0] = static_cast<char>(s[0] - 'A' + 'a');
  return s;
}

}  // namespace t20
