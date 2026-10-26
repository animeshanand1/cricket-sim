#pragma once

#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace t20 {

struct ColorRGBA {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;

  constexpr ColorRGBA() = default;
  constexpr ColorRGBA(float r_, float g_, float b_, float a_ = 1.0f)
    : r(r_), g(g_), b(b_), a(a_) {}

  // uint32 ABGR (standard ImGui ImU32 / OpenGL Little-Endian color format)
  uint32_t toU32() const {
    auto u8 = [](float v) -> uint32_t {
      return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (u8(a) << 24) | (u8(b) << 16) | (u8(g) << 8) | u8(r);
  }

  static ColorRGBA fromU32(uint32_t u) {
    float r = (u & 0xFF) / 255.0f;
    float g = ((u >> 8) & 0xFF) / 255.0f;
    float b = ((u >> 16) & 0xFF) / 255.0f;
    float a = ((u >> 24) & 0xFF) / 255.0f;
    return {r, g, b, a};
  }

  static ColorRGBA fromHex(const std::string& hex, float alpha = 1.0f) {
    if (hex.empty()) return {1.0f, 1.0f, 1.0f, alpha};
    size_t start = (hex[0] == '#') ? 1 : 0;
    if (hex.size() < start + 6) return {1.0f, 1.0f, 1.0f, alpha};

    unsigned int rgb = 0;
    std::sscanf(hex.c_str() + start, "%06x", &rgb);
    float r = ((rgb >> 16) & 0xFF) / 255.0f;
    float g = ((rgb >> 8) & 0xFF) / 255.0f;
    float b = (rgb & 0xFF) / 255.0f;
    return {r, g, b, alpha};
  }

  std::string toHex() const {
    char buf[10];
    int ir = std::clamp(static_cast<int>(r * 255.0f + 0.5f), 0, 255);
    int ig = std::clamp(static_cast<int>(g * 255.0f + 0.5f), 0, 255);
    int ib = std::clamp(static_cast<int>(b * 255.0f + 0.5f), 0, 255);
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", ir, ig, ib);
    return std::string(buf);
  }
};

// shade(h, k): lighten if k > 0, darken if k < 0 (port of ui.jsx shade)
inline std::string shade(const std::string& hex, double k) {
  ColorRGBA c = ColorRGBA::fromHex(hex);
  double r = c.r * 255.0;
  double g = c.g * 255.0;
  double b = c.b * 255.0;

  auto adjust = [k](double v) {
    return k < 0.0 ? v * (1.0 + k) : v + (255.0 - v) * k;
  };

  r = std::clamp(adjust(r), 0.0, 255.0);
  g = std::clamp(adjust(g), 0.0, 255.0);
  b = std::clamp(adjust(b), 0.0, 255.0);

  char buf[10];
  std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                static_cast<int>(std::round(r)),
                static_cast<int>(std::round(g)),
                static_cast<int>(std::round(b)));
  return std::string(buf);
}

// inkOn(h): Pick "#0b1118" or "#ffffff" by luminance (port of ui.jsx inkOn)
inline std::string inkOn(const std::string& hex) {
  ColorRGBA c = ColorRGBA::fromHex(hex);
  auto linearize = [](float val) -> double {
    double v = val;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
  };

  double r = linearize(c.r);
  double g = linearize(c.g);
  double b = linearize(c.b);

  double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  return lum > 0.22 ? "#0b1118" : "#ffffff";
}

inline ColorRGBA inkOnColor(const std::string& hex) {
  return ColorRGBA::fromHex(inkOn(hex));
}

} // namespace t20
