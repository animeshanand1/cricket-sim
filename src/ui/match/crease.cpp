#include "ui/match/crease.hpp"

#include <cmath>
#include <string>

#include "core/str.hpp"
#include "engine/league.hpp"
#include "engine/rules.hpp"
#include "engine/skills.hpp"
#include "engine/special.hpp"
#include "engine/tactics.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

const Player& P(const Match& m, const PlayerId& id) { return m.players.at(id); }

PlayerMeta metaOf(const Player& p) {
  PlayerMeta pm;
  pm.id = p.id;
  pm.name = p.name;
  pm.role = p.role;
  return pm;
}

std::string bowlShort(const Player& p) { return p.bowlType ? std::string(bowlTypeDef(*p.bowlType).shortName) : ""; }

std::string sr(int runs, int balls) { return balls ? js_fixed(runs * 100.0 / balls, 1) : "–"; }

// A card: a bordered child window that sizes to its content (gold border on the striker)
bool beginCard(const char* id, float width, bool highlight) {
  const auto& th = currentTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(th.surface2));
  ImGui::PushStyleColor(ImGuiCol_Border, vec(highlight ? th.gold : th.line));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, metrics().smallRadius + 2);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
  return ImGui::BeginChild(id, ImVec2(width, 0),
                           ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                           ImGuiWindowFlags_NoScrollbar);
}

void endCard() {
  ImGui::EndChild();
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

void muted(FontRole role, const std::string& text) {
  FontScope fs(role);
  ImGui::TextColored(vec(currentTheme().muted), "%s", text.c_str());
}

void batCard(const Match& m, const Innings& inn, const PlayerId& id, bool striker, const std::string& color, float width) {
  const auto& th = currentTheme();
  const BatterInnings& b = inn.batters.at(id);
  const Player& p = P(m, id);
  beginCard(id.c_str(), width, striker);
  // head: avatar · name / hand and style; the strike mark on the right
  avatar(metaOf(p), color, 38.0f);
  ImGui::SameLine(0, 10);
  ImGui::BeginGroup();
  playerLink(p.id, p.name);
  muted(FontRole::Hint, cat(p.hand == Hand::L ? "Left" : "Right", "-hand · ", p.batStyle));
  ImGui::EndGroup();
  // score: big runs + (balls); the strike mark sits at the right of this row
  {
    FontScope fs(FontRole::BoardStat);
    ImGui::Text("%d", b.runs);
  }
  if (striker) {
    const ImVec2 pill_w = textSize(FontRole::BoardLabel, "ON STRIKE");
    const float y = ImGui::GetItemRectMin().y + 6;
    pill(ImGui::GetWindowDrawList(), ImVec2(ImGui::GetWindowPos().x + width - 14 - pill_w.x - 16, y), FontRole::BoardLabel,
         "ON STRIKE", col(th.goldSoft), col(th.goldText), col(th.gold, 0.6f), 8.0f, 3.0f);
  }
  ImGui::SameLine(0, 6);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
  muted(FontRole::Body, cat("(", b.balls, ")"));
  muted(FontRole::Hint, cat("4s ", b.fours, "    6s ", b.sixes, "    SR ", sr(b.runs, b.balls)));
  meter(static_cast<float>(b.conf), confidenceLabel(b.conf).c_str(), true);
  // ChaseList: career milestones arrive with A4 (see crease.hpp)
  endCard();
}

void bowlCard(const Match& m, const Innings& inn, const PlayerId& id, const std::string& color) {
  const auto& th = currentTheme();
  const Player& bowler = P(m, id);
  const BowlingFigures* fig = inn.bowling.find(id);
  if (!fig) return;
  const float width = ImGui::GetContentRegionAvail().x;
  beginCard("##bowlcard", width, false);
  avatar(metaOf(bowler), color, 38.0f);
  ImGui::SameLine(0, 10);
  ImGui::BeginGroup();
  {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.muted), "%s", inn.currentBowler ? "BOWLING" : "BOWLED LAST OVER");
  }
  playerLink(bowler.id, bowler.name);
  std::string meta = bowlShort(bowler);
  const auto kph = speedKph(bowler);
  if (kph && bowler.bowl >= 35) meta += cat(" · ", js_num(*kph), " kph");
  if (bowler.bowlStyle) meta += " · " + *bowler.bowlStyle;
  const bool lastIsHis = inn.lastPlan && inn.lastPlan->bowler == id;
  if (lastIsHis && inn.lastPlan->plan != Plan::stock) meta += cat(" · Plan: ", planByKey(bowler, inn.lastPlan->plan).label);
  muted(FontRole::Hint, meta);
  if (lastIsHis && inn.lastPlan->read > 0) {
    ImGui::SameLine(0, 6);
    tag("Batter reading it", TagTone::Strong);
  }
  ImGui::EndGroup();

  // figures, top right: W-R big, then overs and economy
  {
    const ImVec2 wp = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string wr = cat(fig->wickets, "-", fig->runs);
    const std::string sub =
        cat(oversText(fig->balls), " OV · ECON ", fig->balls ? js_fixed(fig->runs * 6.0 / fig->balls, 2) : std::string("–"));
    const ImVec2 ws = textSize(FontRole::BoardStat, wr), ss = textSize(FontRole::BoardLabel, sub);
    drawText(dl, FontRole::BoardStat, ImVec2(wp.x + width - 14 - ws.x, wp.y + 10), col(th.ink), wr);
    drawText(dl, FontRole::BoardLabel, ImVec2(wp.x + width - 14 - ss.x, wp.y + 12 + ws.y), col(th.muted), sub);
  }

  const double rhythm = fig->rhythm;
  meter(static_cast<float>(rhythm), cat("Rhythm: ", rhythmLabel(rhythm)).c_str(), true);

  // Special delivery: ready, resting, or still building rhythm
  if (const auto special = specialLabel(bowler)) {
    const bool ready = specialReady(fig);
    const std::string state = ready ? "ready: he can produce it any ball"
                              : rhythm >= SPECIAL_RHYTHM
                                  ? "just bowled, needs an over to set it up again"
                                  : cat("needs rhythm ", js_num(SPECIAL_RHYTHM), " (now ", std::lround(rhythm), ")");
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(ready ? th.goldText : th.ink2), "%s", std::string(*special).c_str());
    ImGui::SameLine(0, 5);
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(vec(ready ? th.goldText : th.muted), "%s",
                       (state + (fig->specials ? cat(" · bowled ", fig->specials) : std::string())).c_str());
    ImGui::PopTextWrapPos();
  }
  endCard();
}

}  // namespace

void drawCrease(const Match& m) {
  const auto& th = currentTheme();
  const Innings& inn = inningsOf(m);
  beginPanel("##crease", "At the crease");
  {
    // panel-head: partnership on the right of the title row
    const std::string part = cat("Partnership ", inn.partnership.runs, " (", inn.partnership.balls, ")");
    const ImVec2 ts = textSize(FontRole::Hint, part);
    const ImVec2 wp = ImGui::GetWindowPos();
    drawText(ImGui::GetWindowDrawList(), FontRole::Hint, ImVec2(wp.x + ImGui::GetWindowWidth() - 18 - ts.x, wp.y + 22),
             col(th.muted), part);
  }

  // Two batter cards side by side
  const std::string batColor = m.teams[static_cast<size_t>(inn.batting)].color;
  const std::string bowlColor = m.teams[static_cast<size_t>(inn.bowlingSide)].color;
  const float gap = 10.0f;
  const float cardW = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
  bool first = true;
  for (const auto& id : {inn.striker, inn.nonStriker}) {
    if (!id || !inn.batters.find(*id)) continue;
    if (!first) ImGui::SameLine(0, gap);
    first = false;
    batCard(m, inn, *id, inn.striker && *id == *inn.striker, batColor, cardW);
  }

  // Mind line: the striker's own reading of the game
  if (inn.mindset && inn.striker && inn.mindset->batter == *inn.striker) {
    const std::string now = lower_ascii(tempoName(inn.mindset->tempo));
    const std::string told = lower_ascii(tempoName(inn.mindset->instructed));
    {
      FontScope fs(FontRole::BodyStrong);
      ImGui::TextColored(vec(th.goldText), "%s", inn.mindset->label.c_str());
    }
    ImGui::SameLine(0, 8);
    muted(FontRole::Body, cat("playing ", now, now != told ? cat(" (told: ", told, ")") : std::string()));
  }

  const std::optional<PlayerId>& bowlerId = inn.currentBowler ? inn.currentBowler : inn.lastOverBowler;
  if (bowlerId) bowlCard(m, inn, *bowlerId, bowlColor);

  if (!inn.fow.empty()) {
    const FallOfWicket& w = inn.fow.back();
    const BatterInnings& b = inn.batters.at(w.playerId);
    muted(FontRole::Hint, cat("Last wicket: ", P(m, w.playerId).name, " ", b.runs, " (", b.balls, ") at ", w.runs, "/", w.wicket,
                              ", over ", w.over));
  }
  endPanel();
}

}  // namespace t20::ui
