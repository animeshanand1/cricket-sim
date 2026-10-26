// Home: pick your team, set up a friendly (opponent, pitch) or start a season, and the career at a
// glance (the manager's record, past seasons, achievements).

#include <imgui.h>

#include <cmath>

#include "engine/achievements.hpp"
#include "engine/board.hpp"
#include "engine/league.hpp"
#include "engine/pitch.hpp"
#include "engine/season.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

class HomeScreen : public IScreenView {
 public:
  explicit HomeScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawSeasonBanner();
  void drawTeams(float width);
  void drawFriendly(float width);
  void drawSeasonStart();
  void drawCareer(float width);
  UiContext& ctx_;
};

std::string ratingsLine(const Team& t) {
  const TeamRatings r = teamRatings(t);
  return "Batting " + std::to_string(static_cast<int>(std::lround(r.batting))) + " · Bowling " +
         std::to_string(static_cast<int>(std::lround(r.bowling)));
}

void HomeScreen::draw(float width) {
  const auto& st = ctx_.game.state();
  const TeamMeta me = teamMetaOf(st.setup.userTeamId);
  pageHeader("Your dugout", "Pick a franchise, play a friendly or take them through a season.", "T20 Coach",
             st.setup.userTeamId.empty() ? nullptr : &me);
  ImGui::Dummy(ImVec2(0, 8));
  drawSeasonBanner();
  drawTeams(width);
  ImGui::Dummy(ImVec2(0, 8));
  const bool two = width >= 900;
  if (beginGrid("##home_cols", two ? 2 : 1, 18)) {
    gridNext();
    drawFriendly(two ? width * 0.5f : width);
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    drawSeasonStart();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  drawCareer(width);
}

void HomeScreen::drawSeasonBanner() {
  const auto& st = ctx_.game.state();
  const auto& w = st.world;
  if (w.manager.sacked) {
    notice("The board has let you go. Offers from other franchises are waiting.", NoticeKind::Error);
    if (button("See the offers", ButtonKind::Primary)) ctx_.goTo(Screen::sacked);
    ImGui::Dummy(ImVec2(0, 8));
    return;
  }
  if (w.offseason) {
    notice("The off-season is under way (" + w.offseason->stage + ").", NoticeKind::Warn);
    if (button("Back to the off-season", ButtonKind::Primary)) ctx_.goTo(Screen::offseason);
    ImGui::Dummy(ImVec2(0, 8));
    return;
  }
  if (ctx_.game.state().match && !ctx_.session.hasMatch() && ctx_.game.state().match->status == MatchStatus::toss) {
    notice("A match is waiting for the toss.", NoticeKind::Warn);
    if (button("Go to the toss", ButtonKind::Primary)) ctx_.goTo(Screen::toss);
    ImGui::Dummy(ImVec2(0, 8));
  } else if (ctx_.session.hasMatch() && ctx_.session.isLive()) {
    notice("A match is in progress.", NoticeKind::Warn);
    if (button("Back to the match", ButtonKind::Primary)) ctx_.goTo(Screen::match);
    ImGui::Dummy(ImVec2(0, 8));
  }
}

void HomeScreen::drawTeams(float width) {
  const auto& st = ctx_.game.state();
  const bool locked = ctx_.seasonActive() || st.world.offseason;
  beginPanel("##teams", "Your franchise");
  if (locked) hint("You're managing " + teamNameOf(ctx_.userTeam()) + " this season. Friendlies use them too.");
  else hint("Choose the side you manage. Their squad, XI and club go with you into friendlies and seasons.");
  ImGui::Dummy(ImVec2(0, 4));
  const int cols = columnsFor(width - 36, 220, 5);
  if (beginGrid("##teamgrid", cols, 10)) {
    for (const auto& t : ctx_.league()) {
      gridNext();
      const bool sel = t.id == st.setup.userTeamId;
      if (teamCard(teamMetaOf(t.id), ratingsLine(t), sel, ImGui::GetContentRegionAvail().x, !locked || sel) && !locked)
        ctx_.game.chooseTeam(t.id);
    }
    endGrid();
  }
  endPanel();
}

void HomeScreen::drawFriendly(float /*width*/) {
  const auto& st = ctx_.game.state();
  beginPanel("##friendly", "Friendly");
  hint("A one-off game: nothing rides on it, but form, fitness and development still count.");
  ImGui::Dummy(ImVec2(0, 4));
  eyebrowText("Opponent");
  int i = 0;
  for (const auto& t : ctx_.league()) {
    if (t.id == st.setup.userTeamId) continue;
    if (i++ % 5 != 0) ImGui::SameLine(0, 6);
    if (chip((t.id + "##opp").c_str(), t.id == st.setup.oppTeamId)) ctx_.game.selectOpponent(t.id);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.name.c_str());
  }
  ImGui::Dummy(ImVec2(0, 6));
  eyebrowText("Pitch");
  if (chip("Surprise me##pitch", st.setup.pitch == "random")) ctx_.game.setPitch("random");
  for (Pitch p : PITCH_KEYS) {
    ImGui::SameLine(0, 6);
    if (ImGui::GetContentRegionAvail().x < 90) ImGui::NewLine();
    const PitchDef& d = pitchDef(p);
    if (chip((std::string(d.label) + "##pitch").c_str(), st.setup.pitch == name(p))) ctx_.game.setPitch(std::string(name(p)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (par %d)", std::string(d.report).c_str(), d.par);
  }
  ImGui::Dummy(ImVec2(0, 8));
  const bool ready = !st.setup.userTeamId.empty() && !st.setup.oppTeamId.empty() && !st.world.offseason;
  if (button("Pick your XI", ButtonKind::Primary, ready)) {
    ctx_.game.state().setup.fixture.reset();
    ctx_.game.goTo("xi");
  }
  endPanel();
}

void HomeScreen::drawSeasonStart() {
  const auto& st = ctx_.game.state();
  beginPanel("##season_start", "Season");
  if (const auto& season = st.season.season) {
    const Fixture* next = nextUserFixture(*season);
    ImGui::Text("Season %d with %s", season->number, teamNameOf(season->userTeam).c_str());
    if (st.world.manager.target) hint("Board target: " + st.world.manager.target->label);
    if (next) {
      const TeamId opp = next->home == season->userTeam ? next->away : next->home;
      hint("Next: " + std::string(next->stage == Stage::league ? "match " + std::to_string(next->no) : std::string(stageLabel(next->stage))) +
           " v " + teamNameOf(opp) + (next->home == season->userTeam ? " (home)" : " (away)"));
    } else if (season->champion) {
      hint(teamNameOf(*season->champion) + " are champions.");
    }
    ImGui::Dummy(ImVec2(0, 6));
    if (button("Go to the season", ButtonKind::Primary)) ctx_.goTo(Screen::season);
  } else {
    hint("Fourteen league games in two groups, then the playoffs. The board sets a target from your "
         "squad's strength and judges you on where you finish.");
    ImGui::Dummy(ImVec2(0, 4));
    const int number = static_cast<int>(st.season.history.size()) + 1;
    ImGui::Text("Season %d", number);
    if (!st.setup.userTeamId.empty()) {
      const int rank = strengthRank(ctx_.league(), st.setup.userTeamId);
      hint(teamNameOf(st.setup.userTeamId) + " rank " + ordinal(rank) + " on paper. The board will want: " +
           std::string(boardTarget(rank).label) + ".");
    }
    ImGui::Dummy(ImVec2(0, 6));
    const bool can = !st.setup.userTeamId.empty() && !st.world.offseason && !st.world.manager.sacked;
    if (button("Start the season", ButtonKind::Primary, can)) ctx_.game.startSeason(st.setup.userTeamId);
  }
  endPanel();
}

void HomeScreen::drawCareer(float width) {
  const auto& st = ctx_.game.state();
  const auto& rec = st.career.manager;
  beginPanel("##career", "Career");
  if (beginGrid("##kpis", columnsFor(width - 36, 150, 5), 10)) {
    gridNext();
    kpiTile("Matches", std::to_string(rec.played));
    gridNext();
    kpiTile("Wins", std::to_string(rec.wins), TagTone::Strong);
    gridNext();
    kpiTile("Win rate", rec.played ? pct(100.0 * rec.wins / rec.played) : "–");
    gridNext();
    kpiTile("Best streak", std::to_string(rec.bestStreak), TagTone::Hot);
    gridNext();
    kpiTile("Seasons", std::to_string(st.season.history.size()));
    endGrid();
  }
  if (!st.season.history.empty()) {
    ImGui::Dummy(ImVec2(0, 6));
    sectionHeading("Past seasons");
    for (auto it = st.season.history.rbegin(); it != st.season.history.rend(); ++it) {
      std::string line = "Season " + std::to_string(it->number) + ": " + teamNameOf(it->champion) + " won";
      if (it->userPos) line += " · " + teamNameOf(it->userTeam) + " finished " + ordinal(*it->userPos);
      if (it->orangeCap) line += " · Orange Cap " + ctx_.playerName(it->orangeCap->id);
      ImGui::TextUnformatted(line.c_str());
    }
  }
  const auto list = achievementList(rec, st.season.history, st.world.manager.history);
  int done = 0;
  for (const auto& a : list) done += a.done ? 1 : 0;
  ImGui::Dummy(ImVec2(0, 6));
  if (foldRow("##ach", "Achievements", (std::to_string(done) + " of " + std::to_string(list.size())).c_str())) {
    if (beginGrid("##achgrid", columnsFor(width - 60, 260, 3), 8)) {
      for (const auto& a : list) {
        gridNext();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a.done ? 1.0f : 0.5f);
        {
          FontScope fs(FontRole::BodyStrong);
          ImGui::TextColored(vec(a.done ? currentTheme().goldText : currentTheme().ink2), "%s%s", a.done ? "\xe2\x98\x85 " : "",
                             std::string(a.title).c_str());
        }
        std::string sub(a.text);
        if (!a.done && a.progress && a.goal) sub += " (" + std::to_string(*a.progress) + "/" + std::to_string(*a.goal) + ")";
        if (a.done && a.when) sub += " · " + *a.when;
        hint(sub);
        ImGui::PopStyleVar();
      }
      endGrid();
    }
    endFoldRow();
  }
  endPanel();
}

}  // namespace

std::unique_ptr<IScreenView> makeHomeScreen(UiContext& ctx) { return std::make_unique<HomeScreen>(ctx); }

}  // namespace t20::ui
