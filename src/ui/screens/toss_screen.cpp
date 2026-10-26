// Toss: the conditions (pitch report, sky, dew), the two XIs, then call heads or tails. Win it and
// choose to bat or bowl; lose it and the other captain decides. Then out to the middle.

#include <imgui.h>

#include "engine/mindset.hpp"
#include "engine/pitch.hpp"
#include "engine/season.hpp"
#include "engine/weather.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

class TossScreen : public IScreenView {
 public:
  explicit TossScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawSide(const Match& m, int side);
  UiContext& ctx_;
};

void TossScreen::drawSide(const Match& m, int side) {
  const MatchTeam& t = m.teams[static_cast<size_t>(side)];
  beginPanel(("##side" + std::to_string(side)).c_str(), t.name.c_str());
  int n = 1;
  for (const auto& id : t.players) {
    const Player& p = m.players.at(id);
    ImGui::TextColored(vec(currentTheme().muted), "%2d", n++);
    ImGui::SameLine(0, 10);
    nameLink(ctx_, p.id);
    playerTags(p, {t.captainId == id, t.keeperId == id, false});
  }
  if (!t.bench.empty()) {
    ImGui::Dummy(ImVec2(0, 4));
    std::string bench;
    for (const auto& id : t.bench) bench += (bench.empty() ? "" : ", ") + m.players.at(id).name;
    hint("Impact Player subs: " + bench);
  }
  endPanel();
}

void TossScreen::draw(float width) {
  auto& game = ctx_.game;
  const auto& th = currentTheme();
  if (!game.state().match) {
    emptyState("No match set up. Pick your XI first.");
    if (button("Home")) game.goTo("home");
    return;
  }
  const Match& m = *game.state().match;
  const int user = m.userTeam.value_or(0);
  const MatchTeam& me = m.teams[static_cast<size_t>(user)];
  const MatchTeam& them = m.teams[static_cast<size_t>(1 - user)];
  std::string eyebrow = "Friendly";
  if (m.fixture)
    eyebrow = "Season " + std::to_string(m.fixture->seasonNo) + " · " +
              (m.fixture->stage == Stage::league ? "Match " + std::to_string(m.fixture->no) : std::string(stageLabel(m.fixture->stage)));
  const TeamMeta tm = teamMetaOf(me.id);
  pageHeader(me.name + " v " + them.name, m.fixture ? m.fixture->venue : "", eyebrow, &tm);
  ImGui::Dummy(ImVec2(0, 6));

  const bool two = width >= 900;
  if (beginGrid("##tosscols", two ? 2 : 1, 18)) {
    gridNext();
    // Conditions
    beginPanel("##conditions", "Conditions");
    const PitchDef& pd = pitchDef(m.pitch);
    {
      FontScope fs(FontRole::H3);
      ImGui::TextUnformatted(std::string(pd.label).c_str());
    }
    hint(std::string(pd.report));
    keyValue("Par score", std::to_string(pd.par));
    const SkyDef& sky = skyDef(m.weather.sky);
    keyValue("Sky", std::string(sky.label));
    if (!sky.note.empty()) hint(std::string(sky.note));
    if (m.weather.rainChance > 0) keyValue("Chance of rain", pct(m.weather.rainChance * 100));
    if (m.dew) keyValue("Dew", "Expected in the second innings");
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));

    // The toss itself
    beginPanel("##toss", "The toss");
    if (!m.toss) {
      ImGui::TextWrapped("Call it while the coin's in the air.");
      ImGui::Dummy(ImVec2(0, 6));
      if (button("Heads", ButtonKind::Primary)) game.callToss("heads");
      ImGui::SameLine();
      if (button("Tails", ButtonKind::Primary)) game.callToss("tails");
    } else {
      const Toss& t = *m.toss;
      const bool won = t.winner == user;
      {
        FontScope fs(FontRole::H2);
        ImGui::TextColored(vec(won ? th.green : th.ink), "%s", t.coin ? (upper(*t.coin) + "!").c_str() : "");
      }
      ImGui::Text("You called %s. %s won the toss.", t.call.value_or("?").c_str(), m.teams[static_cast<size_t>(t.winner)].name.c_str());
      ImGui::Dummy(ImVec2(0, 6));
      if (!t.decision) {
        hint("Bat first and set a total, or bowl and know what you're chasing?");
        if (button("Bat first", ButtonKind::Primary)) game.chooseToss(TossDecision::bat);
        ImGui::SameLine();
        if (button("Bowl first", ButtonKind::Primary)) game.chooseToss(TossDecision::bowl);
      } else {
        ImGui::Text("%s chose to %s first.", m.teams[static_cast<size_t>(t.winner)].name.c_str(),
                    std::string(name(*t.decision)).c_str());
        ImGui::Dummy(ImVec2(0, 6));
        if (button("Out to the middle", ButtonKind::Primary)) ctx_.startLiveMatch();
      }
    }
    endPanel();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  if (beginGrid("##xis", two ? 2 : 1, 18)) {
    gridNext();
    drawSide(m, user);
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    drawSide(m, 1 - user);
    endGrid();
  }

  if (!m.toss) {
    beginStickyActions();
    if (button("Back to training")) game.goTo("training");
    endStickyActions();
  }
}

}  // namespace

std::unique_ptr<IScreenView> makeTossScreen(UiContext& ctx) { return std::make_unique<TossScreen>(ctx); }

}  // namespace t20::ui
