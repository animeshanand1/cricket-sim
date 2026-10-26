#include "ui/widgets/avatar.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include <imgui_internal.h>
#include <sstream>
#include <vector>

namespace t20::ui {

void drawAvatar(void* drawList, const PlayerMeta& player, const std::string& teamColor, float posX, float posY, float size, bool showRole) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  // Initials: first letter of the first and last name
  std::string initials = "??";
  if (!player.name.empty()) {
    std::stringstream ss(player.name);
    std::string word;
    std::vector<std::string> parts;
    while (ss >> word) {
      parts.push_back(word);
    }
    if (!parts.empty()) {
      char first = parts.front().empty() ? '?' : parts.front()[0];
      char last = parts.back().empty() ? '?' : parts.back()[0];
      initials = {first, last};
    }
  }

  const std::string c = teamColor.empty() ? "#4a6a8a" : teamColor;
  const float r = size * 0.5f;
  const ImVec2 center(posX + r, posY + r);
  // linear-gradient(145deg, shade(c, .15), shade(c, -.55)) clipped to a circle
  const int start = dl->VtxBuffer.Size;
  dl->AddCircleFilled(center, r, IM_COL32_WHITE, 32);
  ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, ImVec2(posX, posY), ImVec2(posX + size, posY + size),
                                                 hexCol(shade(c, 0.15)), hexCol(shade(c, -0.55)));
  const FontRole fr = size >= 40 ? FontRole::H3 : FontRole::Eyebrow;
  const ImVec2 ts = textSize(fr, initials);
  drawText(dl, fr, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), hexCol(inkOn(shade(c, -0.2))), initials);

  // Role disc, bottom right (WK / BAT / AR / BOWL as a letter until the icon atlas exists)
  if (showRole && size >= 30) {
    const auto& th = currentTheme();
    const float rr = size * 0.21f;
    const ImVec2 rc(posX + size - rr * 0.8f, posY + size - rr * 0.8f);
    dl->AddCircleFilled(rc, rr + 1.5f, col(th.surface));
    dl->AddCircleFilled(rc, rr, col(th.surface3));
    const char* letter = player.role == Role::WK ? "K" : player.role == Role::BAT ? "B" : player.role == Role::AR ? "A" : "O";
    const ImVec2 ls = textSize(FontRole::BoardLabel, letter);
    drawText(dl, FontRole::BoardLabel, ImVec2(rc.x - ls.x * 0.5f, rc.y - ls.y * 0.5f), col(th.goldText), letter);
  }
}

void avatar(const PlayerMeta& player, const std::string& teamColor, float size, bool showRole) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(size, size));
  drawAvatar(ImGui::GetWindowDrawList(), player, teamColor, p.x, p.y, size, showRole);
}

} // namespace t20::ui
