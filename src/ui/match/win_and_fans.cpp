#include "ui/match/win_and_fans.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/format.hpp"
#include "ui/widgets/team_badge.hpp"
#include <algorithm>
#include <cmath>

namespace t20::ui {

void winBar(float battingWinPct, const std::string& battingTeamCode, const std::string& bowlingTeamCode) {
  // WIN CHANCE  AAA 58% ██████████░░░░░░ 42% BBB   (drawn on the dark board)
  const auto& th = currentTheme();
  const float p = std::clamp(battingWinPct, 1.0f, 99.0f);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x;
  const float rowH = 22.0f;

  drawText(dl, FontRole::BoardLabel, ImVec2(o.x, o.y + 5), col(th.boardMuted), "WIN CHANCE");
  float x = o.x + textSize(FontRole::BoardLabel, "WIN CHANCE").x + 14;

  const std::string left = battingTeamCode + " " + std::to_string(static_cast<int>(std::lround(p))) + "%";
  const std::string right = std::to_string(static_cast<int>(std::lround(100 - p))) + "% " + bowlingTeamCode;
  const ImVec2 ls = textSize(FontRole::H3, left), rs = textSize(FontRole::H3, right);
  drawText(dl, FontRole::H3, ImVec2(x, o.y + (rowH - ls.y) * 0.5f), col(th.series1), left);
  x += ls.x + 10;
  const float barEnd = o.x + w - rs.x - 10;
  const float barY = o.y + rowH * 0.5f - 4;
  const float split = x + (barEnd - x) * p / 100.0f;
  dl->AddRectFilled(ImVec2(x, barY), ImVec2(barEnd, barY + 8), col(th.series2), 4.0f);
  dl->AddRectFilled(ImVec2(x, barY), ImVec2(split, barY + 8), col(th.series1), 4.0f, ImDrawFlags_RoundCornersLeft);
  dl->AddLine(ImVec2(split, barY - 2), ImVec2(split, barY + 10), col(th.boardBg), 2.0f);
  drawText(dl, FontRole::H3, ImVec2(barEnd + 10, o.y + (rowH - rs.y) * 0.5f), col(th.series2), right);
  ImGui::Dummy(ImVec2(w, rowH));
}

void fansMeter(const FansSideData& home, const FansSideData& away, int groundCapacity) {
  const auto& th = currentTheme();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x;

  // badge · count · trend triangle · "AAA fans" (the triangle is drawn: Barlow has no ▲▼ glyphs)
  auto side = [&](const FansSideData& s, float x, bool alignRight) {
    const std::string count = fmt_thousands(s.fansCount);
    const std::string label = s.team.id + " fans";
    const ImVec2 cs = textSize(FontRole::BodyStrong, count), ls = textSize(FontRole::Body, label);
    const float tri = s.trend != 0 ? 14.0f : 4.0f;
    const float total = 24 + cs.x + tri + 6 + ls.x;
    float bx = alignRight ? x - total : x;
    drawTeamBadge(dl, s.team, bx, o.y, 18.0f);
    bx += 24;
    drawText(dl, FontRole::BodyStrong, ImVec2(bx, o.y + 1), col(th.boardInk), count);
    bx += cs.x + 4;
    const float cy = o.y + 1 + cs.y * 0.5f;
    if (s.trend > 0)
      dl->AddTriangleFilled(ImVec2(bx + 4, cy - 4), ImVec2(bx, cy + 3), ImVec2(bx + 8, cy + 3), col(th.green));
    else if (s.trend < 0)
      dl->AddTriangleFilled(ImVec2(bx, cy - 3), ImVec2(bx + 8, cy - 3), ImVec2(bx + 4, cy + 4), col(th.red));
    bx += tri + 2;
    drawText(dl, FontRole::Body, ImVec2(bx, o.y + 1), col(th.boardMuted), label);
  };
  side(home, o.x, false);
  side(away, o.x + w, true);

  const float barY = o.y + 26;
  const float total = static_cast<float>(std::max(1, home.fansCount + away.fansCount));
  const float split = o.x + w * home.fansCount / total;
  dl->AddRectFilled(ImVec2(o.x, barY), ImVec2(o.x + w, barY + 6), hexCol(away.team.color, 0.85f), 3.0f);
  dl->AddRectFilled(ImVec2(o.x, barY), ImVec2(split, barY + 6), hexCol(home.team.color), 3.0f, ImDrawFlags_RoundCornersLeft);

  const int inGround = home.fansCount + away.fansCount;
  const int pct = groundCapacity > 0 ? static_cast<int>(std::lround(100.0 * inGround / groundCapacity)) : 0;
  const std::string line = fmt_thousands(inGround) + " in the ground · " + std::to_string(pct) + "% full";
  drawText(dl, FontRole::Hint, ImVec2(o.x, barY + 12), col(th.boardMuted), line);
  ImGui::Dummy(ImVec2(w, 26 + 12 + 18));
}

} // namespace t20::ui
