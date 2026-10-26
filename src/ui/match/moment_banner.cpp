#include "ui/match/moment_banner.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/anim.hpp"

#include <imgui_internal.h>

#include <cmath>

namespace t20::ui {

static MomentEvent s_activeMoment;

void triggerMomentBanner(const MomentEvent& evt) {
  s_activeMoment = evt;
  // 1.9s total animation duration
  startTween("moment_banner", 0.0f, 1.0f, 1.9, EaseType::Linear);
}

void drawMomentBanner(void* drawList, float posX, float posY) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  const float progress = sampleTween("moment_banner", 0.0f);
  if (progress <= 0.0f || progress >= 1.0f) return;

  // 0-0.12 slide in from +40px; hold; 0.80-1.0 slide out to -20px
  float dx = 0, alpha = 1;
  if (progress < 0.12f) {
    const float t = progress / 0.12f;
    dx = 40 * (1 - t);
    alpha = t;
  } else if (progress > 0.80f) {
    const float t = (progress - 0.80f) / 0.20f;
    dx = -20 * t;
    alpha = 1 - t;
  }

  const char* c0 = "#1f6fd1";
  const char* c1 = "#52abff";
  ImU32 ink = IM_COL32_WHITE;
  switch (s_activeMoment.kind) {
    case MomentKind::Six: c0 = "#f5b83d"; c1 = "#ffdf8a"; ink = hexCol("#1a1203"); break;
    case MomentKind::Wicket: c0 = "#b3242a"; c1 = "#ff5d62"; break;
    case MomentKind::Milestone: c0 = "#6a4bd4"; c1 = "#a88bfa"; break;
    case MomentKind::Result: c0 = "#0c9563"; c1 = "#37d39a"; break;
    default: break;
  }
  ink = (ink & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * 255) << IM_COL32_A_SHIFT);

  // Skewed -8 degree parallelogram, right-aligned at (posX, posY)
  const std::string title = upper(s_activeMoment.title);
  const ImVec2 ts = textSize(FontRole::Moment, title);
  const ImVec2 ss = textSize(FontRole::Eyebrow, s_activeMoment.subtitle);
  const float h = ts.y + (s_activeMoment.subtitle.empty() ? 12 : ss.y + 14);
  const float w = std::max(ts.x, ss.x) + 44;
  const float skew = std::tan(8.0f * IM_PI / 180.0f) * h;
  const float x1 = posX + dx, x0 = x1 - w;
  ImVec2 pts[] = {ImVec2(x0 + skew, posY), ImVec2(x1 + skew, posY), ImVec2(x1, posY + h), ImVec2(x0, posY + h)};
  // soft drop shadow: the same parallelogram, offset and translucent
  ImVec2 sh[] = {ImVec2(pts[0].x + 3, pts[0].y + 6), ImVec2(pts[1].x + 3, pts[1].y + 6), ImVec2(pts[2].x + 3, pts[2].y + 6),
                 ImVec2(pts[3].x + 3, pts[3].y + 6)};
  dl->AddConvexPolyFilled(sh, 4, IM_COL32(0, 0, 0, static_cast<int>(90 * alpha)));
  const int start = dl->VtxBuffer.Size;
  dl->AddConvexPolyFilled(pts, 4, IM_COL32_WHITE);
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, ImVec2(x0, posY), ImVec2(x1, posY + h),
                                                 hexCol(c0), hexCol(c1));
  for (int i = start; i < dl->VtxBuffer.Size; ++i) {
    ImDrawVert& v = dl->VtxBuffer[i];
    const auto a = static_cast<ImU32>(((v.col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
    v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
  }
  const float tx = x0 + (w - ts.x) * 0.5f + skew * 0.5f;
  drawText(dl, FontRole::Moment, ImVec2(tx, posY + 4), ink, title);
  if (!s_activeMoment.subtitle.empty())
    drawText(dl, FontRole::Eyebrow, ImVec2(x0 + (w - ss.x) * 0.5f + skew * 0.2f, posY + 4 + ts.y), ink, s_activeMoment.subtitle);
}

} // namespace t20::ui
