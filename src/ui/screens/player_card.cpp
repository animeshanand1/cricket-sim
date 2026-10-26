// The player card (a modal over any screen): who he is, his ratings and every skill, fitness,
// contract and value, potential, career numbers and recent form, and the rating history. From here
// the user can make him club captain, or edit him (name, age, role, hand, bowling, special,
// skills) and restore the original.

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/ageing.hpp"
#include "engine/career.hpp"
#include "engine/editor.hpp"
#include "engine/finance.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/skills.hpp"
#include "engine/special.hpp"
#include "engine/world.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

std::string moneyL(double l) { return money(std::optional<double>(l)); }

template <class K>
void skillMeters(const char* title, const std::vector<SkillDef<K>>& defs, const SkillSet<K>& set) {
  eyebrowText(title);
  for (const auto& d : defs) {
    if (!set.contains(d.key)) continue;
    const double v = set[d.key];
    const std::string label = std::string(d.label) + " " + std::to_string(static_cast<int>(std::lround(v)));
    meter(static_cast<float>(v), label.c_str());
    if (ImGui::IsItemHovered() && !d.help.empty()) ImGui::SetTooltip("%s", std::string(d.help).c_str());
  }
}

template <class K>
void skillSliders(const char* title, const std::vector<SkillDef<K>>& defs, SkillSet<K>& set, const std::string& prefix,
                  const OrderedMap<std::string, std::string>& errors) {
  eyebrowText(title);
  for (const auto& d : defs) {
    if (!set.contains(d.key)) continue;
    int v = static_cast<int>(std::lround(std::isnan(set[d.key]) ? SKILL_MIN : set[d.key]));
    const std::string id = prefix + "." + std::string(name(d.key));
    if (sliderInt(id.c_str(), std::string(d.label).c_str(), &v, static_cast<int>(SKILL_MIN), static_cast<int>(SKILL_MAX)))
      set.set(d.key, v);
    if (const std::string* e = errors.find(id)) ImGui::TextColored(vec(currentTheme().red), "%s", e->c_str());
  }
}

const std::vector<SkillDef<BowlK>>& bowlDefs(std::optional<BowlType> t) { return isPaceType(t) ? PACE_SKILLS : SPIN_SKILLS; }

class PlayerCard : public IScreenView {
 public:
  explicit PlayerCard(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawView(const Player& p, float width);
  void drawEditor(const Player& stored, float width);
  UiContext& ctx_;
  PlayerId draftFor_;
  EditDraft draft_;
  OrderedMap<std::string, std::string> errors_;
  std::string msg_;
};

void PlayerCard::draw(float width) {
  const auto& st = ctx_.game.state();
  if (!st.ui.cardPlayerId) return;
  const PlayerId id = *st.ui.cardPlayerId;
  const Player* p = ctx_.player(id);
  if (!p) {
    emptyState("Unknown player.");
    return;
  }
  if (draftFor_ != id) {
    draftFor_ = id;
    errors_ = {};
    msg_.clear();
    if (const Player* stored = st.world.players.find(id)) draft_ = draftFrom(*stored);
  }
  if (st.ui.cardEdit) {
    if (const Player* stored = st.world.players.find(id)) drawEditor(*stored, width);
    return;
  }
  drawView(*p, width);
}

void PlayerCard::drawView(const Player& p, float width) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  const World& w = st.world;
  const auto& th = currentTheme();
  const TeamId team = p.team.value_or("");

  // ---- identity
  avatar(playerMetaOf(p), teamColorOf(team), 64, true);
  ImGui::SameLine(0, 14);
  ImGui::BeginGroup();
  {
    FontScope fs(FontRole::H1);
    ImGui::TextUnformatted(p.name.c_str());
  }
  std::string line = roleText(p.role) + " · " + (p.hand == Hand::L ? "Left-hand bat" : "Right-hand bat");
  if (p.bowlType) line += " · " + bowlText(p);
  if (const auto sp = specialLabel(p)) line += " · " + std::string(*sp);
  ImGui::TextColored(vec(th.ink2), "%s", line.c_str());
  phaseTag(p.age);
  ImGui::SameLine(0, 6);
  ImGui::TextColored(vec(th.muted), "%d yrs", p.age);
  if (p.overseas) {
    ImGui::SameLine(0, 6);
    osTag();
  }
  const bool captain = !team.empty() && clubCaptainOf(w, team) == p.id;
  if (captain) {
    ImGui::SameLine(0, 6);
    tag("Club captain", TagTone::Hot);
  }
  if (p.edited) {
    ImGui::SameLine(0, 6);
    styleChip("Edited", true);
  }
  if (p.retired) {
    ImGui::SameLine(0, 6);
    tag("Retired " + std::to_string(p.retired->year), TagTone::Muted);
  }
  ImGui::EndGroup();
  if (!team.empty()) {
    ImGui::SameLine();
    const float x = ImGui::GetWindowContentRegionMax().x - 90;
    if (ImGui::GetCursorPosX() < x) ImGui::SetCursorPosX(x);
    ImGui::BeginGroup();
    teamBadge(teamMetaOf(team), 40);
    ImGui::TextColored(vec(th.muted), "%s", team.c_str());
    ImGui::EndGroup();
  } else if (!p.retired) {
    ImGui::SameLine();
    tag("Free agent", TagTone::Muted);
  }
  ImGui::Dummy(ImVec2(0, 6));

  // ---- headline numbers
  const auto pv = potentialView(w, p, ctx_.userTeam());
  if (beginGrid("##cardkpi", columnsFor(width, 120, 6), 8)) {
    gridNext();
    kpiTile("Batting", std::to_string(static_cast<int>(std::lround(p.bat))), p.bat >= 80 ? TagTone::Strong : TagTone::Default);
    gridNext();
    kpiTile("Bowling", p.bowlType ? std::to_string(static_cast<int>(std::lround(p.bowl))) : "–", p.bowl >= 80 ? TagTone::Strong : TagTone::Default);
    gridNext();
    kpiTile("Fielding", std::to_string(static_cast<int>(std::lround(p.field))));
    gridNext();
    kpiTile("Potential", pv ? (pv->exact ? std::string(t20::potentialGrade(pv->lo)) : std::to_string(static_cast<int>(pv->lo)) + "-" +
                                                                                       std::to_string(static_cast<int>(pv->hi)))
                            : "–",
            TagTone::Hot);
    gridNext();
    kpiTile("Value", moneyL(marketValue(p)));
    gridNext();
    kpiTile("Fitness", isInjured(p) ? "Out" : pct(fitnessOf(p)), isInjured(p) || fitnessOf(p) < 60 ? TagTone::Weak : TagTone::Default);
    endGrid();
  }
  if (isInjured(p)) notice(p.injury->label + ": out for " + std::to_string(p.injury->matches) + " more match" + (p.injury->matches == 1 ? "." : "es."), NoticeKind::Error);
  if (p.contract && !team.empty())
    hint("Contract: " + moneyL(p.contract->cost) + " a season, " + std::to_string(p.contract->years) + " more season" +
         (p.contract->years == 1 ? "" : "s") + " (" + std::string(name(p.contract->via)) + ")");
  if (const Popularity* pop = st.career.popularity.find(p.id))
    hint("Popularity " + pct(pop->v) + (pop->d != 0 ? " (" + signedNum(pop->d, 1) + " last match)" : ""));
  ImGui::Dummy(ImVec2(0, 6));

  // ---- skills
  const bool two = width >= 640;
  if (beginGrid("##skills", two ? 3 : 1, 14)) {
    gridNext();
    skillMeters("Batting", BAT_SKILLS, p.skills.bat);
    gridNext();
    if (p.bowlType) skillMeters(isPaceType(p.bowlType) ? "Pace bowling" : "Spin bowling", bowlDefs(p.bowlType), p.skills.bowl);
    else eyebrowText("Doesn't bowl");
    if (const auto kph = speedKph(p); kph && isPaceType(p.bowlType)) hint("Speed gun: about " + std::to_string(static_cast<int>(*kph)) + " kph");
    gridNext();
    skillMeters("Fielding", FIELD_SKILLS, p.skills.field);
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 6));

  // ---- career
  const CareerStats* cs = st.career.players.find(p.id);
  const CareerSummary sum = summarize(cs);
  if (foldRow("##career", "Career", cs ? (std::to_string(cs->matches) + " matches").c_str() : "No matches yet", cs != nullptr)) {
    if (cs) {
      if (beginGrid("##careergrid", 2, 18)) {
        gridNext();
        eyebrowText("Batting");
        keyValue("Runs", std::to_string(cs->bat.runs));
        keyValue("Average", fmt(sum.batAvg));
        keyValue("Strike rate", fmt(sum.batSR));
        keyValue("Highest", sum.hsText);
        keyValue("50s / 100s", std::to_string(cs->bat.fifties) + " / " + std::to_string(cs->bat.hundreds));
        keyValue("4s / 6s", std::to_string(cs->bat.fours) + " / " + std::to_string(cs->bat.sixes));
        gridNext();
        eyebrowText("Bowling and fielding");
        keyValue("Wickets", std::to_string(cs->bowl.wickets));
        keyValue("Overs", sum.overs);
        keyValue("Economy", fmt(sum.econ));
        keyValue("Average", fmt(sum.bowlAvg));
        keyValue("Best", sum.bestText);
        keyValue("Catches / stumpings", std::to_string(cs->field.catches) + " / " + std::to_string(cs->field.stumpings));
        endGrid();
      }
      keyValue("Won / played", std::to_string(cs->wins) + " / " + std::to_string(cs->matches) + " · player of the match " + std::to_string(cs->potm));
      if (!cs->recent.empty()) {
        ImGui::Dummy(ImVec2(0, 4));
        eyebrowText("Recent matches");
        for (const auto& r : cs->recent) {
          std::string l = r.result + " v " + r.vs;
          if (r.bat) l += " · " + *r.bat;
          if (r.bowl) l += " · " + *r.bowl;
          ImGui::BulletText("%s", l.c_str());
        }
      }
    }
    endFoldRow();
  }
  if (!p.history.empty() && foldRow("##history", "Rating history", nullptr)) {
    for (const auto& h : p.history)
      ImGui::Text("%d (age %d): bat %d · bowl %d · field %d", h.year, h.age, static_cast<int>(std::lround(h.bat)),
                  static_cast<int>(std::lround(h.bowl)), static_cast<int>(std::lround(h.field)));
    endFoldRow();
  }

  // ---- actions
  ImGui::Dummy(ImVec2(0, 8));
  if (!p.retired) {
    if (button("Edit player")) game.openEditor(p.id);
    if (!team.empty() && !captain) {
      ImGui::SameLine();
      if (button("Make club captain")) {
        const auto r = game.makeClubCaptain(p.id);
        msg_ = !r.ok ? r.error : r.injured ? "He's captain, but injured: someone else leads until he's fit." : "He's the club captain.";
      }
    }
  }
  if (p.edited) {
    ImGui::SameLine();
    if (button("Restore original", ButtonKind::Quiet)) game.restorePlayer(p.id);
  }
  if (!msg_.empty()) notice(msg_, NoticeKind::Ok);
}

void PlayerCard::drawEditor(const Player& stored, float width) {
  auto& game = ctx_.game;
  const auto& th = currentTheme();
  {
    FontScope fs(FontRole::H2);
    ImGui::Text("Edit %s", stored.name.c_str());
  }
  hint("Changes replace his generated values. His id stays, so his career stats stay with him.");
  ImGui::Dummy(ImVec2(0, 6));
  auto err = [&](const char* key) {
    if (const std::string* e = errors_.find(key)) ImGui::TextColored(vec(th.red), "%s", e->c_str());
  };
  if (beginGrid("##identity", width >= 640 ? 2 : 1, 14)) {
    gridNext();
    textInput("##name", "Name", &draft_.name, PLAYER_NAME_MAX);
    err("name");
    gridNext();
    eyebrowText("Age");
    int age = std::isnan(draft_.age) ? stored.age : static_cast<int>(draft_.age);
    if (numberStepper("##age", &age, AGE_MIN, AGE_MAX)) draft_.age = age;
    err("age");
    gridNext();
    const std::vector<std::string> roles = {"WK", "BAT", "AR", "BOWL"};
    const std::vector<std::string> roleNames = {"Wicketkeeper", "Batter", "All-rounder", "Bowler"};
    int ri = 0;
    for (size_t i = 0; i < roles.size(); ++i)
      if (draft_.role.value_or(std::string(name(stored.role))) == roles[i]) ri = static_cast<int>(i);
    if (selectBox("##role", "Role", roleNames, &ri)) draft_.role = roles[static_cast<size_t>(ri)];
    gridNext();
    int hi = draft_.hand.value_or(std::string(name(stored.hand))) == "L" ? 1 : 0;
    if (selectBox("##hand", "Batting hand", {"Right", "Left"}, &hi)) draft_.hand = hi ? "L" : "R";
    gridNext();
    std::vector<std::string> types = {""}, typeNames = {"Doesn't bowl"};
    for (size_t i = 0; i < std::size(kBowlTypeNames); ++i) {
      types.emplace_back(kBowlTypeNames[i]);
      typeNames.emplace_back(bowlTypeDef(static_cast<BowlType>(i)).label);
    }
    const std::string curType = draft_.bowlType.value_or(stored.bowlType ? std::string(name(*stored.bowlType)) : "");
    int ti = 0;
    for (size_t i = 0; i < types.size(); ++i)
      if (types[i] == curType) ti = static_cast<int>(i);
    if (selectBox("##type", "Bowling", typeNames, &ti)) {
      // moving between pace and spin carries over what makes sense
      const std::optional<BowlType> from = parseBowlType(curType), to = parseBowlType(types[static_cast<size_t>(ti)]);
      draft_.skills.bowl = convertBowling(draft_.skills.bowl, from, to);
      draft_.bowlType = types[static_cast<size_t>(ti)];
      draft_.special = "";
    }
    gridNext();
    const auto type = parseBowlType(draft_.bowlType.value_or(curType));
    std::vector<std::string> sps = {""}, spNames = {"Auto (his best skill)"};
    if (type)
      for (SpecialKey k : specialsFor(type)) {
        sps.emplace_back(name(k));
        spNames.emplace_back(specialDef(k).label);
      }
    const std::string curSp = draft_.special.value_or(stored.special ? std::string(name(*stored.special)) : "");
    int si = 0;
    for (size_t i = 0; i < sps.size(); ++i)
      if (sps[i] == curSp) si = static_cast<int>(i);
    if (selectBox("##special", "Special delivery", spNames, &si)) draft_.special = sps[static_cast<size_t>(si)];
    endGrid();
  }
  err("form");
  ImGui::Dummy(ImVec2(0, 6));
  const auto type = parseBowlType(draft_.bowlType.value_or(stored.bowlType ? std::string(name(*stored.bowlType)) : ""));
  if (beginGrid("##sliders", width >= 640 ? 2 : 1, 18)) {
    gridNext();
    skillSliders("Batting", BAT_SKILLS, draft_.skills.bat, "bat", errors_);
    gridNext();
    if (type) {
      skillSliders(isPaceType(type) ? "Pace bowling" : "Spin bowling", bowlDefs(type), draft_.skills.bowl, "bowl", errors_);
      ImGui::Dummy(ImVec2(0, 6));
    }
    skillSliders("Fielding", FIELD_SKILLS, draft_.skills.field, "field", errors_);
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  if (button("Save", ButtonKind::Primary)) {
    const EditResult r = game.savePlayerEdit(stored.id, draft_);
    errors_ = r.errors;
    if (r.ok) {
      msg_ = "Saved.";
      draftFor_.clear();  // rebuild the draft from the saved player
      game.openCard(stored.id);
    }
  }
  ImGui::SameLine();
  if (button("Cancel", ButtonKind::Quiet)) {
    draftFor_.clear();
    game.openCard(stored.id);
  }
  if (!errors_.empty()) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(vec(th.red), "Fix the marked fields.");
  }
}

}  // namespace

std::unique_ptr<IScreenView> makePlayerCard(UiContext& ctx) { return std::make_unique<PlayerCard>(ctx); }

}  // namespace t20::ui
