#include "ui/widgets/nav_bar.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include "ui/format.hpp"
#include "ui/anim.hpp"

namespace t20::ui {

std::optional<View> navBar(const NavState& state, float screenWidth) {
  std::optional<View> requestedView;
  const auto& th = currentTheme();
  const auto& met = metrics();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float h = met.navBarHeight;

  // Sticky bar: navBg with a 1px bottom line
  dl->AddRectFilled(o, ImVec2(o.x + screenWidth, o.y + h), col(th.navBg));
  dl->AddLine(ImVec2(o.x, o.y + h - 0.5f), ImVec2(o.x + screenWidth, o.y + h - 0.5f), col(th.line));

  const float contentW = std::min(screenWidth - met.desktopSidePadding * 2, met.maxContentWidth);
  const float left = o.x + (screenWidth - contentW) * 0.5f;
  const float right = left + contentW;

  // Brand: ball mark + "T20 Coach" (Coach in gold)
  const float cy = o.y + h * 0.5f;
  dl->AddCircleFilled(ImVec2(left + 13, cy), 12, hexCol("#c0262d"));
  dl->AddBezierCubic(ImVec2(left + 6, cy - 9), ImVec2(left + 11, cy - 3), ImVec2(left + 11, cy + 3), ImVec2(left + 6, cy + 9),
                     IM_COL32(255, 255, 255, 170), 1.3f);
  dl->AddBezierCubic(ImVec2(left + 20, cy - 9), ImVec2(left + 15, cy - 3), ImVec2(left + 15, cy + 3), ImVec2(left + 20, cy + 9),
                     IM_COL32(255, 255, 255, 170), 1.3f);
  const ImVec2 t1 = textSize(FontRole::H2, "T20 ");
  drawText(dl, FontRole::H2, ImVec2(left + 34, cy - t1.y * 0.5f), col(th.ink), "T20 ");
  drawText(dl, FontRole::H2, ImVec2(left + 34 + t1.x, cy - t1.y * 0.5f), col(th.goldText), "Coach");
  ImGui::SetCursorScreenPos(ImVec2(left, o.y));
  if (ImGui::InvisibleButton("##brand", ImVec2(34 + t1.x + textSize(FontRole::H2, "Coach").x, h))) requestedView = View::home;

  if (state.seasonInfo) {
    const std::string s = "Season " + std::to_string(state.seasonInfo->seasonNumber) + " · " +
                          ordinal(state.seasonInfo->tablePosition) + " · " + std::to_string(state.seasonInfo->points) + " pts";
    const ImVec2 ts = textSize(FontRole::Hint, s);
    drawText(dl, FontRole::Hint, ImVec2(left + 190, cy - ts.y * 0.5f), col(th.muted), s);
  }

  struct Link {
    View view;
    const char* label;
    bool current;
    bool enabled;
  };
  const View v = state.currentView;
  const Link links[] = {
      {View::home, "Home", v == View::home || v == View::xi || v == View::training, true},
      {View::season, "Season", v == View::season || v == View::offseason || v == View::sacked, true},
      {View::club, "Club", v == View::club, true},
      {View::match, "Match", v == View::match || v == View::toss, state.hasActiveMatch},
      {View::players, "Players", v == View::players, true},
  };
  float x = right;
  for (int i = IM_ARRAYSIZE(links) - 1; i >= 0; --i) {
    const Link& l = links[i];
    const ImVec2 ts = textSize(FontRole::BodyStrong, l.label);
    const float extra = (l.view == View::match && state.isMatchLive) ? 14.0f : 0.0f;
    const float w = ts.x + 28 + extra;
    x -= w + 4;
    const ImVec2 a(x, o.y + 9), b(x + w, o.y + h - 9);
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(i);
    const bool clicked = ImGui::InvisibleButton("##nav", ImVec2(w, b.y - a.y));
    ImGui::PopID();
    const bool hovered = l.enabled && ImGui::IsItemHovered();
    if (l.current) {
      dl->AddRectFilled(a, b, col(th.surface3), 8.0f);
      dl->AddRectFilled(ImVec2(a.x + 10, b.y - 2), ImVec2(b.x - 10, b.y), col(th.gold), 1.0f);
    } else if (hovered) {
      dl->AddRectFilled(a, b, col(th.surface2), 8.0f);
    }
    const ImU32 fg = !l.enabled ? col(th.muted, 0.5f) : l.current ? col(th.ink) : col(th.ink2);
    drawText(dl, FontRole::BodyStrong, ImVec2(a.x + 14, cy - ts.y * 0.5f), fg, l.label);
    if (extra > 0) {
      const float pulse = AnimationManager::instance().getPulse(1.2f);
      dl->AddCircleFilled(ImVec2(a.x + 14 + ts.x + 9, cy), 4.0f, col(th.red, 0.45f + 0.55f * pulse));
    }
    if (clicked && l.enabled) requestedView = l.view;
  }

  ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + h));
  return requestedView;
}

} // namespace t20::ui
