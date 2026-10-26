#include "ui/widgets/meter_and_rating.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/anim.hpp"
#include <algorithm>

namespace t20::ui {

bool playerLink(const std::string& playerId, const std::string& displayName) {
  // Text with an underline on hover; the caller opens the player card on click
  const auto& th = currentTheme();
  ImGui::PushID(playerId.c_str());
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImVec2 ts = textSize(FontRole::BodyStrong, displayName);
  const bool clicked = ImGui::InvisibleButton("##link", ts);
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  drawText(dl, FontRole::BodyStrong, p, col(hovered ? th.goldText : th.ink), displayName);
  if (hovered) {
    dl->AddLine(ImVec2(p.x, p.y + ts.y), ImVec2(p.x + ts.x, p.y + ts.y), col(th.goldText));
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  return clicked;
}

void rating(int value, const char* tooltip) {
  const auto& th = currentTheme();
  ColorRGBA c = th.muted;
  if (value >= 80) {
    c = th.green;
  } else if (value >= 60) {
    c = th.ink;
  }
  FontScope fs(FontRole::BodyStrong);
  ImGui::TextColored(vec(c), "%d", value);
  if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
}

void meter(float value, const char* label, bool compact, const char* toneOverride) {
  const auto& th = currentTheme();
  float clamped = std::clamp(value, 2.0f, 100.0f);

  ColorRGBA fillColor;
  if (toneOverride) {
    std::string to(toneOverride);
    if (to == "good") fillColor = th.green;
    else if (to == "mid") fillColor = th.gold;
    else fillColor = th.red;
  } else {
    if (clamped >= 70.0f) fillColor = th.green;
    else if (clamped >= 45.0f) fillColor = th.gold;
    else fillColor = th.red;
  }

  // 7px track (surface-3), fill gradient, label beside
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float avail = ImGui::GetContentRegionAvail().x;
  const float labelW = label ? std::min(textSize(FontRole::Hint, label).x + 10, avail * 0.5f) : 0.0f;
  const float w = compact ? std::min(120.0f, avail - labelW) : avail - labelW;
  const float h = 7.0f, rowH = 16.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float y = p.y + (rowH - h) * 0.5f;
  dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), col(th.surface3), 4.0f);
  rectGradient(dl, ImVec2(p.x, y), ImVec2(p.x + w * clamped / 100.0f, y + h), col(fillColor, 0.7f), col(fillColor), 4.0f);
  if (label) drawText(dl, FontRole::Hint, ImVec2(p.x + w + 10, p.y), col(th.muted), label);
  ImGui::Dummy(ImVec2(w + labelW, rowH));
}

} // namespace t20::ui
