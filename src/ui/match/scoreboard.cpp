#include "ui/match/scoreboard.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/format.hpp"
#include "ui/widgets/team_badge.hpp"
#include "ui/widgets/tags.hpp"

#include <imgui_internal.h>

namespace t20::ui {

namespace {

void hline(ImDrawList* dl, float x0, float x1, float y) {
  dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), col(currentTheme().boardLine));
}

}  // namespace

void drawScoreboard(void* drawListArg, const ScoreboardViewData& data, float width) {
  const auto& th = currentTheme();
  const auto& met = metrics();
  ImDrawList* dl = drawListArg ? static_cast<ImDrawList*>(drawListArg) : ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float w = width;
  const float pad = 22.0f;

  // channel 0: board background (sized once we know the height); channel 1: content
  dl->ChannelsSplit(2);
  dl->ChannelsSetCurrent(1);

  // --- Row 1: team block · score · stats -------------------------------------------------
  const float row1H = 104.0f;
  const std::string teamColor = data.battingTeam.color.empty() ? "#3987e5" : data.battingTeam.color;
  // team-colour slanted block behind the badge
  {
    const float bw = 330.0f, skew = 26.0f;
    // rounded top-left so it stays inside the board's corner
    const float r = met.scoreboardRadius;
    const int start = dl->VtxBuffer.Size;
    dl->PathArcTo(ImVec2(o.x + r, o.y + r), r, IM_PI, IM_PI * 1.5f, 8);
    dl->PathLineTo(ImVec2(o.x + bw + skew, o.y));
    dl->PathLineTo(ImVec2(o.x + bw, o.y + row1H));
    dl->PathLineTo(ImVec2(o.x, o.y + row1H));
    dl->PathFillConvex(IM_COL32_WHITE);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, ImVec2(o.x, o.y), ImVec2(o.x + bw + skew, o.y),
                                                   hexCol(teamColor, 0.55f) | IM_COL32_A_MASK, hexCol(shade(teamColor, -0.6)));
    for (int i = start; i < dl->VtxBuffer.Size; ++i) {
      ImDrawVert& v = dl->VtxBuffer[i];
      const float t = (v.pos.x - o.x) / (bw + skew);
      const auto a = static_cast<ImU32>(((v.col >> IM_COL32_A_SHIFT) & 0xFF) * (0.55f - 0.45f * t));
      v.col = (v.col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
    }
  }
  drawTeamBadge(dl, data.battingTeam, o.x + pad, o.y + (row1H - 46 * 1.1f) * 0.5f, 46.0f);
  const float tx = o.x + pad + 46 + 16;
  drawText(dl, FontRole::BoardLabel, ImVec2(tx, o.y + 30), col(th.goldText), upper(data.inningsLabel));
  drawText(dl, FontRole::PanelTitle, ImVec2(tx, o.y + 46), col(th.boardInk), upper(data.battingTeam.name));

  // score
  const float sx = o.x + 380.0f;
  const std::string score = std::to_string(data.runs) + "/" + std::to_string(data.wickets);
  const ImVec2 scs = textSize(FontRole::ScoreboardScore, score);
  drawText(dl, FontRole::ScoreboardScore, ImVec2(sx, o.y + (row1H - scs.y) * 0.5f), col(th.boardInk), score);
  const std::string ov = oversText(data.balls) + " OV";
  const ImVec2 ovs = textSize(FontRole::H2, ov);
  drawText(dl, FontRole::H2, ImVec2(sx + scs.x + 12, o.y + (row1H + scs.y) * 0.5f - ovs.y - 8), col(th.boardMuted), ov);

  // stat columns
  float stx = sx + scs.x + 12 + ovs.x + 34;
  for (const auto& st : data.stats) {
    dl->AddLine(ImVec2(stx - 17, o.y + 26), ImVec2(stx - 17, o.y + row1H - 26), col(th.boardLine));
    drawText(dl, FontRole::BoardLabel, ImVec2(stx, o.y + 30), col(th.boardMuted), upper(st.label));
    drawText(dl, FontRole::BoardStat, ImVec2(stx, o.y + 46), st.gold ? col(th.goldText) : col(th.boardInk), st.value);
    stx += std::max(textSize(FontRole::BoardLabel, upper(st.label)).x, textSize(FontRole::BoardStat, st.value).x) + 34;
  }

  float y = o.y + row1H;
  hline(dl, o.x, o.x + w, y);

  // --- Row 2: headline · over pips · tags ------------------------------------------------
  const float row2H = 58.0f;
  {
    const float cy = y + row2H * 0.5f;
    const std::string head = upper(data.headline);
    const ImVec2 hs = textSize(FontRole::H2, head);
    drawText(dl, FontRole::H2, ImVec2(o.x + pad, cy - hs.y * 0.5f), col(th.goldText), head);
    float x = o.x + pad + std::max(hs.x + 30, 230.0f);
    if (!data.currentOverPips.empty()) {
      const std::string lbl = upper(data.overLabel);
      const ImVec2 ls = textSize(FontRole::BoardLabel, lbl);
      drawText(dl, FontRole::BoardLabel, ImVec2(x, cy - ls.y * 0.5f), col(th.boardMuted), lbl);
      x += ls.x + 12;
      for (const auto& pip : data.currentOverPips) {
        drawBallPip(dl, pip, x + 16, cy, 32.0f);
        x += 38;
      }
      const std::string rr = std::to_string(data.overRuns) + (data.overRuns == 1 ? " run" : " runs");
      const ImVec2 rs = textSize(FontRole::BodyStrong, rr);
      drawText(dl, FontRole::BodyStrong, ImVec2(x + 4, cy - rs.y * 0.5f), col(th.boardInk), rr);
      x += rs.x + 24;
    }
    // tags, right-aligned
    float rx = o.x + w - pad;
    std::vector<std::string> tags = data.tags;
    if (data.freeHit) tags.insert(tags.begin(), "Free hit");
    for (auto it = tags.rbegin(); it != tags.rend(); ++it) {
      const std::string t = upper(*it);
      const ImVec2 ts = textSize(FontRole::BoardLabel, t);
      const float pw = ts.x + 18;
      if (rx - pw < x) break;
      const bool hot = *it == "Free hit";
      pill(dl, ImVec2(rx - pw, cy - (ts.y + 8) * 0.5f), FontRole::BoardLabel, t, hot ? col(th.gold, 0.18f) : col(th.boardLine, 0.7f),
           hot ? col(th.goldText) : col(th.boardInk), hot ? col(th.gold, 0.6f) : 0, 9.0f, 4.0f);
      rx -= pw + 8;
    }
  }
  y += row2H;

  // --- Rows 3-4: win chance, fans --------------------------------------------------------
  if (data.battingWinPct || data.winPending || data.fansHome) {
    // win bar and fans meter lay out with the available width, so give each a borderless child
    hline(dl, o.x, o.x + w, y);
    y += 14;
    if (data.battingWinPct) {
      ImGui::SetCursorScreenPos(ImVec2(o.x + pad, y));
      ImGui::BeginChild("##winbar", ImVec2(w - pad * 2, 22), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
      winBar(*data.battingWinPct, data.battingTeam.id, data.bowlingTeam.id);
      ImGui::EndChild();
      y += 22 + 12;
    } else if (data.winPending) {
      // same height as the bar, so nothing below jumps when the estimate arrives
      drawText(dl, FontRole::BoardLabel, ImVec2(o.x + pad, y + 5), col(th.boardMuted), "WIN CHANCE");
      drawText(dl, FontRole::Hint, ImVec2(o.x + pad + textSize(FontRole::BoardLabel, "WIN CHANCE").x + 14, y + 3), col(th.boardMuted),
               "Working it out…");
      y += 22 + 12;
    }
    if (data.fansHome && data.fansAway) {
      ImGui::SetCursorScreenPos(ImVec2(o.x + pad, y));
      ImGui::BeginChild("##fans", ImVec2(w - pad * 2, 58), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
      fansMeter(*data.fansHome, *data.fansAway, data.groundCapacity);
      ImGui::EndChild();
      y += 58;
    }
    y += 4;
  }

  // --- Row 5: conditions strip ----------------------------------------------------------
  if (!data.conditions.empty()) {
    hline(dl, o.x, o.x + w, y);
    const float rowH = 38.0f;
    const ImVec2 cs = textSize(FontRole::Hint, data.conditions);
    drawText(dl, FontRole::Hint, ImVec2(o.x + pad, y + (rowH - cs.y) * 0.5f), col(th.boardMuted), data.conditions);
    y += rowH;
  }

  // --- background -----------------------------------------------------------------------
  const ImVec2 b(o.x + w, y);
  dl->ChannelsSetCurrent(0);
  drawPanelShadow(dl, o.x, o.y, b.x, b.y, met.scoreboardRadius, true);
  rectGradient(dl, o, b, col(th.boardBg2), col(th.boardBg), met.scoreboardRadius, true);
  dl->AddRect(o, b, col(th.boardLine), met.scoreboardRadius);
  dl->ChannelsMerge();

  ImGui::SetCursorScreenPos(o);
  ImGui::Dummy(ImVec2(w, y - o.y));
}

} // namespace t20::ui
