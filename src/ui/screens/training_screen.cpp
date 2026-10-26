// Training: up to three players do a drill before the match. The drilled skills are sharper for
// this match only and they walk out a little more confident. Then on to the toss.

#include <imgui.h>

#include <algorithm>

#include "engine/development.hpp"
#include "engine/pitch.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/panel.hpp"

namespace t20::ui {

namespace {

Pitch pitchOf(const std::string& key) {
  for (Pitch p : PITCH_KEYS)
    if (name(p) == key) return p;
  return Pitch::balanced;
}

class TrainingScreen : public IScreenView {
 public:
  explicit TrainingScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  UiContext& ctx_;
};

void TrainingScreen::draw(float width) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  const auto& th = currentTheme();
  const Team* team = ctx_.team(st.setup.userTeamId);
  if (!team) {
    emptyState("Pick a team on the Home screen first.");
    return;
  }
  std::vector<const Player*> xi;
  for (const auto& id : st.setup.xi)
    for (const auto& p : team->squad)
      if (p.id == id) xi.push_back(&p);

  const TeamMeta me = teamMetaOf(team->id);
  pageHeader("Training", "Up to " + std::to_string(MAX_TRAINING_SLOTS) + " players can drill before the match.", "Match prep", &me);
  ImGui::Dummy(ImVec2(0, 6));

  const auto slots = st.setup.training;
  if (beginGrid("##slots", columnsFor(width, 300, 3), 14)) {
    for (int s = 0; s < MAX_TRAINING_SLOTS; ++s) {
      gridNext();
      ImGui::PushID(s);
      beginPanel("##slot", ("Slot " + std::to_string(s + 1)).c_str());
      const TrainingSlot* slot = s < static_cast<int>(slots.size()) ? &slots[static_cast<size_t>(s)] : nullptr;
      if (!slot && s > static_cast<int>(slots.size())) {
        hint("Fill the slot before this one first.");
        endPanel();
        ImGui::PopID();
        continue;
      }
      // Player: anyone in the XI not already drilling in another slot
      std::vector<std::string> names;
      std::vector<PlayerId> ids;
      int sel = -1;
      for (const Player* p : xi) {
        const bool other = std::any_of(slots.begin(), slots.end(), [&](const TrainingSlot& t) {
          return t.playerId == p->id && (!slot || &t != slot);
        });
        if (other) continue;
        if (slot && slot->playerId == p->id) sel = static_cast<int>(ids.size());
        names.push_back(p->name + " (" + roleText(p->role) + ")");
        ids.push_back(p->id);
      }
      int pick = sel;
      if (selectBox("##who", "Player", names, &pick, "Choose a player") && pick >= 0) {
        const Player* p = nullptr;
        for (const Player* q : xi)
          if (q->id == ids[static_cast<size_t>(pick)]) p = q;
        const auto drills = p ? drillsFor(*p) : std::vector<const Drill*>{};
        game.setTraining(static_cast<size_t>(s), ids[static_cast<size_t>(pick)], drills.empty() ? "" : std::string(drills.front()->key));
      }
      if (slot) {
        const Player* p = nullptr;
        for (const Player* q : xi)
          if (q->id == slot->playerId) p = q;
        if (p) {
          const auto drills = drillsFor(*p);
          std::vector<std::string> dn;
          int dsel = -1;
          for (size_t i = 0; i < drills.size(); ++i) {
            dn.push_back(std::string(drills[i]->area) + ": " + std::string(drills[i]->label));
            if (drills[i]->key == slot->drill) dsel = static_cast<int>(i);
          }
          ImGui::Dummy(ImVec2(0, 4));
          if (selectBox("##drill", "Drill", dn, &dsel, "Choose a drill") && dsel >= 0)
            game.setTraining(static_cast<size_t>(s), p->id, std::string(drills[static_cast<size_t>(dsel)]->key));
          if (const Drill* d = drillByKey(slot->drill)) {
            ImGui::Dummy(ImVec2(0, 2));
            hint(std::string(d->help));
          }
        }
        ImGui::Dummy(ImVec2(0, 4));
        if (button("Clear", ButtonKind::Small)) game.setTraining(static_cast<size_t>(s), std::nullopt);
      }
      endPanel();
      ImGui::PopID();
    }
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 4));
  hint("Trained players start with +" + std::to_string(static_cast<int>(TRAINING_CONFIDENCE)) +
       " confidence. The boost lasts one match; development comes from how they play.");
  (void)th;

  beginStickyActions();
  if (button("Back to the XI")) game.goTo("xi");
  ImGui::SameLine();
  if (button("Coach's plan", ButtonKind::Quiet)) {
    // The AI coach's picks for this pitch
    const auto plan = aiTrainingPlan(xi, pitchOf(st.setup.pitch));
    game.clearTraining();
    for (size_t i = 0; i < plan.size() && i < static_cast<size_t>(MAX_TRAINING_SLOTS); ++i)
      game.setTraining(i, plan[i].playerId, plan[i].drill);
  }
  ImGui::SameLine();
  if (button("Rest everyone", ButtonKind::Quiet, !slots.empty())) game.clearTraining();
  ImGui::SameLine();
  rightAlign(buttonWidth("Go to the toss", ButtonKind::Primary));
  if (button("Go to the toss", ButtonKind::Primary, st.setup.xi.size() == 11)) game.startFixture();
  endStickyActions();
}

}  // namespace

std::unique_ptr<IScreenView> makeTrainingScreen(UiContext& ctx) { return std::make_unique<TrainingScreen>(ctx); }

}  // namespace t20::ui
