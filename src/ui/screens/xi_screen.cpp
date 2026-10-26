// Playing XI: pick eleven from the squad, set the batting order, the keeper and the captain.
// The XI must have 11 fit players, a keeper, at most 4 overseas players and 5 bowling options.

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/pitch.hpp"
#include "engine/season.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
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

bool contains(const std::vector<PlayerId>& v, const PlayerId& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

struct XiCheck {
  int count = 0, overseas = 0, bowlers = 0;
  bool keeper = false;
  std::vector<std::string> problems;
};

XiCheck checkXI(const std::vector<PlayerId>& xi, const std::optional<PlayerId>& keeperId, const Team& team) {
  XiCheck c;
  c.count = static_cast<int>(xi.size());
  for (const auto& id : xi)
    for (const auto& p : team.squad)
      if (p.id == id) {
        c.overseas += p.overseas ? 1 : 0;
        c.bowlers += p.bowlType && p.bowl >= 40 ? 1 : 0;
        if (p.unavailable || isInjured(p)) c.problems.push_back(p.name + " is injured.");
      }
  c.keeper = keeperId && contains(xi, *keeperId);
  if (c.count != 11) c.problems.push_back("Pick " + std::to_string(11 - c.count) + " more player" + (11 - c.count == 1 ? "." : "s."));
  if (!c.keeper) c.problems.push_back("Choose a wicketkeeper.");
  if (c.overseas > MAX_OVERSEAS) c.problems.push_back("At most " + std::to_string(MAX_OVERSEAS) + " overseas players.");
  if (c.bowlers < 5) c.problems.push_back("You need five bowling options.");
  return c;
}

class XiScreen : public IScreenView {
 public:
  explicit XiScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawXI(const Team& team);
  void drawBench(const Team& team);
  UiContext& ctx_;
};

void XiScreen::draw(float width) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  const Team* team = ctx_.team(st.setup.userTeamId);
  if (!team) {
    emptyState("Pick a team on the Home screen first.");
    return;
  }
  const TeamMeta me = teamMetaOf(team->id);
  std::string sub = "v " + teamNameOf(st.setup.oppTeamId);
  if (st.setup.fixture) {
    sub = (st.setup.fixture->stage == Stage::league ? "Match " + std::to_string(st.setup.fixture->no)
                                                    : std::string(stageLabel(st.setup.fixture->stage))) +
          " " + sub + " · " + st.setup.fixture->venue;
  }
  if (st.setup.pitch != "random") {
    for (Pitch p : PITCH_KEYS)
      if (name(p) == st.setup.pitch) sub += " · " + std::string(pitchDef(p).label);
  }
  pageHeader("Playing XI", sub, st.setup.fixture ? "Season fixture" : "Friendly", &me);
  ImGui::Dummy(ImVec2(0, 6));

  const XiCheck c = checkXI(st.setup.xi, st.setup.keeperId, *team);
  if (beginGrid("##xikpi", 4, 10)) {
    gridNext();
    kpiTile("Players", std::to_string(c.count) + "/11", c.count == 11 ? TagTone::Strong : TagTone::Weak);
    gridNext();
    kpiTile("Overseas", std::to_string(c.overseas) + "/" + std::to_string(MAX_OVERSEAS), c.overseas > MAX_OVERSEAS ? TagTone::Weak : TagTone::Default);
    gridNext();
    kpiTile("Bowling options", std::to_string(c.bowlers), c.bowlers >= 5 ? TagTone::Default : TagTone::Weak);
    gridNext();
    kpiTile("Keeper", c.keeper ? surname(ctx_.playerName(*st.setup.keeperId)) : "None", c.keeper ? TagTone::Default : TagTone::Weak);
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 6));

  const bool two = width >= 960;
  if (beginGrid("##xicols", two ? 2 : 1, 18)) {
    gridNext();
    drawXI(*team);
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    drawBench(*team);
    endGrid();
  }

  beginStickyActions();
  if (button("Back")) {
    if (st.setup.fixture) game.backToSeason();
    else game.goTo("home");
  }
  ImGui::SameLine();
  if (button("Auto pick", ButtonKind::Quiet)) game.autoPick();
  ImGui::SameLine();
  const bool ok = c.problems.empty();
  if (!ok) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(vec(currentTheme().red), "%s", c.problems.front().c_str());
  }
  ImGui::SameLine();
  rightAlign(buttonWidth("Training", ButtonKind::Primary));
  if (button("Training", ButtonKind::Primary, ok)) game.goTo("training");
  endStickyActions();
}

void XiScreen::drawXI(const Team& team) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  const auto& th = currentTheme();
  beginPanel("##xi", "Batting order");
  hint("Top to bottom is the batting order. Pick the keeper and captain here.");
  ImGui::Dummy(ImVec2(0, 2));
  const std::vector<PlayerId> xi = st.setup.xi;  // copy: the buttons change it
  for (size_t i = 0; i < xi.size(); ++i) {
    const Player* p = nullptr;
    for (const auto& q : team.squad)
      if (q.id == xi[i]) p = &q;
    if (!p) continue;
    ImGui::PushID(static_cast<int>(i));
    const bool keeper = st.setup.keeperId == p->id, captain = st.setup.captainId == p->id;
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    {
      FontScope fs(FontRole::H3);
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(vec(th.muted), "%2zu", i + 1);
    }
    ImGui::SameLine(0, 10);
    ImGui::BeginGroup();
    nameLink(ctx_, *p);
    playerTags(*p, {false, false, true});
    {
      FontScope fs(FontRole::Hint);
      ImGui::TextColored(vec(th.muted), "%s · Bat %d · Bowl %s", roleText(p->role).c_str(), static_cast<int>(std::lround(p->bat)),
                         p->bowlType ? std::to_string(static_cast<int>(std::lround(p->bowl))).c_str() : "–");
    }
    ImGui::EndGroup();
    // controls on the right: up, down, keeper, captain, drop
    const float cw = 4 * 32 + 2 * 40;
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - cw, rowStart.y + 2));
    if (iconWidget("up", 16, i > 0 ? th.ink2 : th.line, "Up the order") && i > 0) game.movePlayer(p->id, -1);
    ImGui::SameLine(0, 2);
    if (iconWidget("down", 16, i + 1 < xi.size() ? th.ink2 : th.line, "Down the order") && i + 1 < xi.size()) game.movePlayer(p->id, 1);
    ImGui::SameLine(0, 4);
    if (chip("WK", keeper)) game.setKeeper(p->id);
    ImGui::SameLine(0, 4);
    if (chip("C", captain)) {
      if (captain) game.captainCleared();
      else game.chooseCaptain(p->id);
    }
    ImGui::SameLine(0, 4);
    if (iconWidget("x", 16, th.red, "Drop from the XI")) game.togglePlayer(p->id);
    ImGui::PopID();
    ImGui::PushStyleColor(ImGuiCol_Separator, vec(th.line));
    ImGui::Separator();
    ImGui::PopStyleColor();
  }
  if (xi.empty()) emptyState("Nobody picked yet. Add players from the squad, or Auto pick.");
  endPanel();
}

void XiScreen::drawBench(const Team& team) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  const auto& th = currentTheme();
  beginPanel("##bench", "Squad");
  hint("Everyone not in the XI. Injured players can't be picked.");
  ImGui::Dummy(ImVec2(0, 2));
  std::vector<const Player*> bench;
  for (const auto& p : team.squad)
    if (!contains(st.setup.xi, p.id)) bench.push_back(&p);
  std::stable_sort(bench.begin(), bench.end(), [](const Player* a, const Player* b) { return a->role < b->role; });
  const bool full = st.setup.xi.size() >= 11;
  for (const Player* p : bench) {
    ImGui::PushID(p->id.c_str());
    const bool out = p->unavailable || isInjured(*p);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    ImGui::BeginGroup();
    if (out) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
    nameLink(ctx_, *p);
    playerTags(*p);
    {
      FontScope fs(FontRole::Hint);
      ImGui::TextColored(vec(th.muted), "%s · Bat %d · Bowl %s · %d yrs", roleText(p->role).c_str(),
                         static_cast<int>(std::lround(p->bat)),
                         p->bowlType ? std::to_string(static_cast<int>(std::lround(p->bowl))).c_str() : "–", p->age);
    }
    if (out) ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - 70, rowStart.y + 2));
    if (button("Pick", ButtonKind::Small, !out && !full)) game.togglePlayer(p->id);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 2));
  }
  if (bench.empty()) emptyState("The whole squad is in the XI.");
  endPanel();
}

}  // namespace

std::unique_ptr<IScreenView> makeXiScreen(UiContext& ctx) { return std::make_unique<XiScreen>(ctx); }

}  // namespace t20::ui
