#pragma once
// Dear ImGui drawing helpers shared by the widgets: theme colours as ImU32, fonts by role,
// gradients on rounded shapes and soft radial glows (the CSS the web game leans on).

#include <imgui.h>

#include <string>
#include <string_view>

#include "core/color.hpp"
#include "ui/theme.hpp"

namespace t20::ui {

inline ImU32 col(const ColorRGBA& c, float alphaMul = 1.0f) {
  return ColorRGBA(c.r, c.g, c.b, c.a * alphaMul).toU32();
}
inline ImU32 hexCol(const std::string& hex, float alpha = 1.0f) { return ColorRGBA::fromHex(hex, alpha).toU32(); }
inline ImVec4 vec(const ColorRGBA& c) { return ImVec4(c.r, c.g, c.b, c.a); }

inline ImFont* font(FontRole role) { return static_cast<ImFont*>(getFont(role)); }

// RAII font push (falls back to the default font when the atlas has no such role)
class FontScope {
 public:
  explicit FontScope(FontRole role) : pushed_(font(role) != nullptr) {
    if (pushed_) ImGui::PushFont(font(role));
  }
  ~FontScope() {
    if (pushed_) ImGui::PopFont();
  }
  FontScope(const FontScope&) = delete;
  FontScope& operator=(const FontScope&) = delete;

 private:
  bool pushed_;
};

// Text drawn straight onto a draw list in a given font role
void drawText(ImDrawList* dl, FontRole role, ImVec2 pos, ImU32 color, std::string_view text);
ImVec2 textSize(FontRole role, std::string_view text);

// Rounded rect filled with a linear gradient from c0 (at p0) to c1 (at p1)
void rectGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c0, ImU32 c1, float rounding, bool vertical = false);

// Soft radial glow: `inner` at the centre fading to transparent at `radius`
void radialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 inner, int segments = 64);

// A pill (fully rounded rect) with centred text; returns its size
ImVec2 pill(ImDrawList* dl, ImVec2 pos, FontRole role, std::string_view text, ImU32 bg, ImU32 fg, ImU32 border = 0,
            float padX = 9.0f, float padY = 3.0f);

// Uppercase copy (eyebrows, team names on the board)
std::string upper(std::string_view s);

}  // namespace t20::ui
