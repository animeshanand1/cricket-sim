#include "ui/match/result_panels.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdio>

#include "core/str.hpp"
#include "engine/achievements.hpp"
#include "engine/milestones.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

TeamMeta metaOf(const MatchTeam& t) {
  TeamMeta tm;
  tm.id = t.id;
  tm.name = t.name;
  tm.shortName = t.id;
  tm.color = t.color;
  return tm;
}

PlayerMeta playerMeta(const Player& p) {
  PlayerMeta pm;
  pm.id = p.id;
  pm.name = p.name;
  pm.role = p.role;
  return pm;
}

// "78* (49), 1/22 (2 ov)" across both innings (Super Overs left out)
std::string potmLine(const Match& m, const PlayerId& id) {
  std::string out;
  for (const Innings& inn : m.innings) {
    if (inn.superOver) continue;
    if (const BatterInnings* b = inn.batters.find(id)) {
      out += (out.empty() ? "" : ", ") + cat(b->runs, b->out ? "" : "*", " (", b->balls, ")");
    }
    if (const BowlingFigures* f = inn.bowling.find(id); f && f->balls > 0) {
      out += (out.empty() ? "" : ", ") + cat(f->wickets, "/", f->runs, " (", oversText(f->balls), " ov)");
    }
  }
  return out;
}

std::string signed1(double v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%+.1f", v);
  return buf;
}

}  // namespace

void drawResultBanner(const Match& m, const std::vector<MatchExit>& exits) {
  if (m.status != MatchStatus::complete || !m.result) return;
  const auto& th = currentTheme();
  const Result& r = *m.result;
  const int user = m.userTeam.value_or(0);
  const bool tie = !r.winner;
  const bool won = r.winner && *r.winner == user;
  const MatchTeam& accentTeam = m.teams[static_cast<size_t>(r.winner.value_or(user))];
  const std::string accent = accentTeam.color.empty() ? "#3987e5" : accentTeam.color;

  const float w = ImGui::GetContentRegionAvail().x, h = 132.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  // dark board with the winner's colour sweeping in from the left
  rectGradient(dl, p, ImVec2(p.x + w, p.y + h), col(th.boardBg2), col(th.boardBg), metrics().scoreboardRadius);
  rectGradient(dl, p, ImVec2(p.x + w * 0.55f, p.y + h), hexCol(accent, tie ? 0.18f : 0.45f), hexCol(accent, 0.0f), metrics().scoreboardRadius);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), hexCol(accent, 0.7f), metrics().scoreboardRadius, 0, 1.5f);
  const float badge = 84.0f;
  if (!tie) drawTeamBadge(dl, metaOf(accentTeam), p.x + 22, p.y + (h - badge * 1.1f) * 0.5f, badge);
  else drawIcon(dl, "swap", p.x + 40, p.y + h * 0.5f - 24, 48, th.boardMuted);
  const float tx = p.x + 22 + badge + 24;
  const char* eyebrow = r.noResult ? "NO RESULT" : tie ? "TIED" : won ? "VICTORY" : "DEFEAT";
  drawText(dl, FontRole::Eyebrow, ImVec2(tx, p.y + 22), won ? col(th.green) : tie || r.noResult ? col(th.goldText) : col(th.red), eyebrow);
  drawText(dl, FontRole::H1, ImVec2(tx, p.y + 38), col(th.boardInk), upper(r.text));
  if (m.potm) {
    const Player& potm = m.players.at(*m.potm);
    const std::string line = "Player of the match: " + potm.name + "  " + potmLine(m, *m.potm);
    drawText(dl, FontRole::Body, ImVec2(tx, p.y + 88), col(th.boardMuted), line);
  }
  // the way out, bottom right
  float bw = 0;
  for (const auto& e : exits) bw += buttonWidth(e.label.c_str(), ButtonKind::Default) + 8;
  ImGui::SetCursorScreenPos(ImVec2(p.x + w - bw - 14, p.y + h - 56));
  for (size_t i = 0; i < exits.size(); ++i) {
    if (i) ImGui::SameLine(0, 8);
    if (button(exits[i].label.c_str(), i == 0 ? ButtonKind::Primary : ButtonKind::Default) && exits[i].action) exits[i].action();
  }
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(w, h));
}

void drawMatchMilestones(const Match& m, const Career& career, const std::vector<SeasonArchive>& history,
                         const std::vector<BoardEntry>& board) {
  const auto& th = currentTheme();
  const bool hasMs = career.lastMilestones && career.lastMilestones->matchId == m.id && !career.lastMilestones->list.empty();
  const bool hasAch = career.lastUnlocked && career.lastUnlocked->matchId == m.id && !career.lastUnlocked->keys.empty();
  if (!hasMs && !hasAch) return;
  beginPanel("##milestones", "Milestones and achievements");
  if (hasAch) {
    const auto all = achievementList(career.manager, history, board);
    for (const auto& key : career.lastUnlocked->keys)
      for (const auto& a : all)
        if (a.key == key) {
          const ImVec2 p = ImGui::GetCursorScreenPos();
          drawIcon(ImGui::GetWindowDrawList(), "trophy", p.x, p.y + 2, 20, th.goldText);
          ImGui::SetCursorScreenPos(ImVec2(p.x + 30, p.y));
          ImGui::BeginGroup();
          {
            FontScope fs(FontRole::Eyebrow);
            ImGui::TextColored(vec(th.goldText), "ACHIEVEMENT UNLOCKED");
          }
          {
            FontScope fs(FontRole::BodyStrong);
            ImGui::TextUnformatted(std::string(a.title).c_str());
          }
          ImGui::SameLine(0, 8);
          ImGui::TextColored(vec(th.muted), "%s", std::string(a.text).c_str());
          ImGui::EndGroup();
          ImGui::Dummy(ImVec2(0, 4));
        }
  }
  if (hasMs) {
    int n = 0;
    for (const auto& hit : career.lastMilestones->list) {
      if (n++ >= 8) break;
      const Player* p = m.players.find(hit.id);
      const MatchTeam* t = nullptr;
      for (const auto& team : m.teams)
        if (team.id == hit.team) t = &team;
      if (t) {
        teamBadge(metaOf(*t), 18);
        ImGui::SameLine(0, 8);
      }
      ImGui::PushID(n);
      if (p) playerLink(p->id, p->name);
      ImGui::PopID();
      ImGui::SameLine(0, 8);
      ImGui::TextColored(vec(th.ink2), "%s", milestoneText(hit.type, hit.value).c_str());
    }
  }
  endPanel();
}

void drawDevelopmentReport(const Match& m, const Career& career, DevReportState& st) {
  if (!career.lastReport || career.lastReport->matchId != m.id) return;
  const auto& th = currentTheme();
  beginPanel("##devreport", "Player development");
  const int user = m.userTeam.value_or(0);
  const int side = st.side < 0 ? user : st.side;
  for (int s = 0; s < 2; ++s) {
    if (chip((m.teams[static_cast<size_t>(s)].id + "##dev").c_str(), side == s)) st.side = s;
    ImGui::SameLine(0, 6);
  }
  ImGui::NewLine();
  {
    FontScope fs(FontRole::Hint);
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(vec(th.muted), "Every performance is judged against what's normal for his batting position and rating. "
                                      "Skills move by up to half a point a match, within 8%% of where he started; captains "
                                      "learn a little extra.");
    ImGui::PopTextWrapPos();
  }
  ImGui::Dummy(ImVec2(0, 4));
  const MatchTeam& team = m.teams[static_cast<size_t>(side)];
  int shown = 0;
  auto inTeam = [&](const PlayerId& id) {
    for (const auto& x : team.players)
      if (x == id) return true;
    for (const auto& x : team.bench)
      if (x == id) return true;
    return false;
  };
  auto tone = [&](double v) { return v >= 0.2 ? th.green : v <= -0.2 ? th.red : th.ink2; };
  for (const DevReport& r : career.lastReport->reports) {
    if (!inTeam(r.id)) continue;
    const Player* p = m.players.find(r.id);
    if (!p) continue;
    ++shown;
    ImGui::PushID(r.id.c_str());
    avatar(playerMeta(*p), team.color, 34, true);
    ImGui::SameLine(0, 10);
    ImGui::BeginGroup();
    playerLink(p->id, p->name);
    if (r.captain) {
      ImGui::SameLine(0, 6);
      tag("Captain", TagTone::Hot);
    }
    if (p->training) {
      ImGui::SameLine(0, 6);
      styleChip("Trained: " + p->training->label);
    }
    // what he did
    std::string sep;
    if (r.bat) {
      ImGui::TextColored(vec(tone(r.bat->perf)), "Bat %s: %s", r.bat->line.c_str(), r.bat->label.c_str());
      sep = "x";
    }
    if (r.bowl) {
      if (!sep.empty()) ImGui::SameLine(0, 14);
      ImGui::TextColored(vec(tone(r.bowl->perf)), "Bowl %s: %s", r.bowl->line.c_str(), r.bowl->label.c_str());
      sep = "x";
    }
    if (r.field && (r.field->taken || r.field->dropped)) {
      if (!sep.empty()) ImGui::SameLine(0, 14);
      std::string f = std::to_string(r.field->taken) + " taken";
      if (r.field->dropped) f += ", " + std::to_string(r.field->dropped) + " dropped";
      ImGui::TextColored(vec(r.field->delta > 0 ? th.green : r.field->delta < 0 ? th.red : th.ink2), "%s", f.c_str());
    }
    // rating changes
    std::string changes;
    auto change = [&](const char* label, double before, double after) {
      const double d = after - before;
      if (std::fabs(d) < 0.05) return;
      ImGui::TextColored(vec(th.ink2), "%s %d", label, static_cast<int>(std::lround(after)));
      ImGui::SameLine(0, 4);
      ImGui::TextColored(vec(d > 0 ? th.green : th.red), "%s %s", d > 0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc", signed1(d).c_str());
      ImGui::SameLine(0, 14);
      changes = "x";
    };
    change("Batting", r.before.bat, r.after.bat);
    change("Bowling", r.before.bowl, r.after.bowl);
    change("Fielding", r.before.field, r.after.field);
    if (!changes.empty()) ImGui::NewLine();
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PopID();
  }
  if (shown == 0) ImGui::TextColored(vec(th.muted), "Nobody from %s did enough to be judged this match.", team.name.c_str());
  endPanel();
}

}  // namespace t20::ui
