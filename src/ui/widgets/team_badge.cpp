#include "ui/widgets/team_badge.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include <imgui_internal.h>
#include <string>

namespace t20::ui {

void drawTeamBadge(void* drawList, const TeamMeta& team, float posX, float posY, float size) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  const float w = size;
  const float h = size * 1.1f;

  const std::string c = team.color.empty() ? "#3987e5" : team.color;
  const ImU32 cLight = hexCol(shade(c, 0.25));
  const ImU32 cDark = hexCol(shade(c, -0.45));
  const ImU32 textInk = hexCol(inkOn(c));

  // Shield on a 40x44 grid (the web SVG): flat top, straight sides, pointed bottom
  auto P = [&](float x, float y) { return ImVec2(posX + x / 40.0f * w, posY + y / 44.0f * h); };
  ImVec2 pts[] = {P(20, 1.5f), P(37, 6), P(37, 21), P(34.5f, 31), P(20, 42.5f), P(5.5f, 31), P(3, 21), P(3, 6)};
  const int start = dl->VtxBuffer.Size;
  dl->AddConvexPolyFilled(pts, IM_ARRAYSIZE(pts), IM_COL32_WHITE);
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, P(3, 1.5f), P(37, 42.5f), cLight, cDark);
  dl->AddPolyline(pts, IM_ARRAYSIZE(pts), IM_COL32(255, 255, 255, 90), ImDrawFlags_Closed, ImMax(1.0f, size / 28.0f));
  // highlight chevron
  ImVec2 chev[] = {P(8, 10.5f), P(20, 6.5f), P(32, 10.5f)};
  dl->AddPolyline(chev, 3, IM_COL32(255, 255, 255, 72), 0, ImMax(1.0f, size / 24.0f));

  if (size >= 22.0f) {
    const FontRole fr = size >= 50 ? FontRole::H2 : size >= 34 ? FontRole::H3 : FontRole::Eyebrow;
    const ImVec2 ts = textSize(fr, team.id);
    drawText(dl, fr, ImVec2(posX + (w - ts.x) * 0.5f, posY + h * 0.52f - ts.y * 0.5f), textInk, team.id);
  }
}

void teamBadge(const TeamMeta& team, float size) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(size, size * 1.1f));
  drawTeamBadge(ImGui::GetWindowDrawList(), team, p.x, p.y, size);
}

} // namespace t20::ui
