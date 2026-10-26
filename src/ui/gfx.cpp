#include "ui/gfx.hpp"

#include <imgui_internal.h>

#include <cctype>
#include <cmath>

namespace t20::ui {

void drawText(ImDrawList* dl, FontRole role, ImVec2 pos, ImU32 color, std::string_view text) {
  ImFont* f = font(role);
  if (!f) f = ImGui::GetFont();
  dl->AddText(f, f->FontSize / getFontAtlas().dpiScale, pos, color, text.data(), text.data() + text.size());
}

ImVec2 textSize(FontRole role, std::string_view text) {
  ImFont* f = font(role);
  if (!f) f = ImGui::GetFont();
  return f->CalcTextSizeA(f->FontSize / getFontAtlas().dpiScale, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
}

void rectGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c0, ImU32 c1, float rounding, bool vertical) {
  const int start = dl->VtxBuffer.Size;
  dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding);
  const int end = dl->VtxBuffer.Size;
  const ImVec2 p1 = vertical ? ImVec2(a.x, b.y) : ImVec2(b.x, b.y);
  // ShadeVerts keeps the vertex alpha, so blend the alpha by hand afterwards
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, end, a, p1, c0 | IM_COL32_A_MASK, c1 | IM_COL32_A_MASK);
  const float a0 = ((c0 >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f, a1 = ((c1 >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
  const ImVec2 d = ImVec2(p1.x - a.x, p1.y - a.y);
  const float len2 = ImMax(d.x * d.x + d.y * d.y, 1e-4f);
  for (int i = start; i < end; ++i) {
    ImDrawVert& v = dl->VtxBuffer[i];
    const float t = ImClamp(((v.pos.x - a.x) * d.x + (v.pos.y - a.y) * d.y) / len2, 0.0f, 1.0f);
    // multiply, so the anti-aliased fringe (vertex alpha 0) stays soft
    const float vertexAlpha = ((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
    const auto alpha = static_cast<ImU32>(vertexAlpha * (a0 + (a1 - a0) * t) * 255.0f + 0.5f);
    v.col = (v.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
  }
}

void radialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 inner, int segments) {
  const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
  const ImU32 outer = inner & ~IM_COL32_A_MASK;
  // Two rings so the falloff is closer to the CSS radial-gradient curve
  const ImU32 mid = (inner & ~IM_COL32_A_MASK) | ((((inner >> IM_COL32_A_SHIFT) & 0xFF) * 2 / 5) << IM_COL32_A_SHIFT);
  const float rMid = radius * 0.45f;
  dl->PrimReserve(segments * 9, 1 + segments * 2);
  const auto base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
  dl->PrimWriteVtx(center, uv, inner);
  for (int i = 0; i < segments; ++i) {
    const float a = (static_cast<float>(i) / segments) * IM_PI * 2.0f;
    const ImVec2 dir(std::cos(a), std::sin(a));
    dl->PrimWriteVtx(ImVec2(center.x + dir.x * rMid, center.y + dir.y * rMid), uv, mid);
    dl->PrimWriteVtx(ImVec2(center.x + dir.x * radius, center.y + dir.y * radius), uv, outer);
  }
  for (int i = 0; i < segments; ++i) {
    const int j = (i + 1) % segments;
    const auto m0 = static_cast<ImDrawIdx>(base + 1 + i * 2), o0 = static_cast<ImDrawIdx>(m0 + 1);
    const auto m1 = static_cast<ImDrawIdx>(base + 1 + j * 2), o1 = static_cast<ImDrawIdx>(m1 + 1);
    dl->PrimWriteIdx(base);
    dl->PrimWriteIdx(m0);
    dl->PrimWriteIdx(m1);
    dl->PrimWriteIdx(m0);
    dl->PrimWriteIdx(o0);
    dl->PrimWriteIdx(o1);
    dl->PrimWriteIdx(m0);
    dl->PrimWriteIdx(o1);
    dl->PrimWriteIdx(m1);
  }
}

ImVec2 pill(ImDrawList* dl, ImVec2 pos, FontRole role, std::string_view text, ImU32 bg, ImU32 fg, ImU32 border,
            float padX, float padY) {
  const ImVec2 ts = textSize(role, text);
  const ImVec2 size(ts.x + padX * 2, ts.y + padY * 2);
  const ImVec2 b(pos.x + size.x, pos.y + size.y);
  if (bg) dl->AddRectFilled(pos, b, bg, size.y * 0.5f);
  if (border) dl->AddRect(pos, b, border, size.y * 0.5f, 0, 1.0f);
  drawText(dl, role, ImVec2(pos.x + padX, pos.y + padY), fg, text);
  return size;
}

std::string upper(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}

}  // namespace t20::ui
