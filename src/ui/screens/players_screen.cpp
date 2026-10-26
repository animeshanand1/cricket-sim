// Players: every player in the world (filter by team, role, name; sort by any column), the records
// book, the hall of fame, and the designer for new players.

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "engine/ageing.hpp"
#include "engine/editor.hpp"
#include "engine/finance.hpp"
#include "engine/fitness.hpp"
#include "engine/records.hpp"
#include "engine/skills.hpp"
#include "engine/world.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/sortable_table.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

TableCell numCell(double v, int dp = 0, CellTone tone = CellTone::Normal) {
  TableCell c;
  c.text = std::isnan(v) ? "\xe2\x80\x93" : num(v, dp);
  c.sort = v;
  c.tone = tone;
  return c;
}

TableCell ratingCell(double v) {
  return numCell(v, 0, std::isnan(v) ? CellTone::Muted : v >= 80 ? CellTone::Good : v >= 60 ? CellTone::Normal : CellTone::Muted);
}

class PlayersScreen : public IScreenView {
 public:
  explicit PlayersScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawBrowser(float width);
  void drawRecords(float width);
  void drawHall();
  void drawCreate(float width);
  void rebuildRows();

  UiContext& ctx_;
  int tab_ = 0;
  std::string team_ = "mine";  // "all", "free", "mine" or a team id
  int role_ = -1;              // -1 all, else Role
  std::string search_;
  bool retired_ = false;
  SortableTableState state_;
  std::vector<TableRowData> rows_;
  uint64_t rowsAt_ = UINT64_MAX;
  std::string rowsKey_;
  // the new-player designer
  int newRole_ = 1;
  EditDraft newDraft_;
  bool newInit_ = false;
  bool newCaptain_ = false;
  OrderedMap<std::string, std::string> newErrors_;
  std::string newMsg_;
};

void PlayersScreen::draw(float width) {
  pageHeader("Players", "Everyone in the league: squads, free agents and the retired.", "Scouting");
  ImGui::Dummy(ImVec2(0, 6));
  tab_ = tabs({{"all", "All players", ""}, {"records", "Records", ""}, {"hall", "Hall of fame", ""}, {"new", "New player", ""}}, tab_);
  ImGui::Dummy(ImVec2(0, 8));
  switch (tab_) {
    case 0: drawBrowser(width); break;
    case 1: drawRecords(width); break;
    case 2: drawHall(); break;
    default: drawCreate(width); break;
  }
  beginStickyActions();
  if (button("Back")) ctx_.game.closePlayers();
  endStickyActions();
}

void PlayersScreen::rebuildRows() {
  const std::string key = team_ + "|" + std::to_string(role_) + "|" + search_ + "|" + (retired_ ? "r" : "");
  if (rowsAt_ == ctx_.revision() && rowsKey_ == key) return;
  rowsAt_ = ctx_.revision();
  rowsKey_ = key;
  rows_.clear();
  const World& w = ctx_.game.state().world;
  const TeamId me = ctx_.userTeam();
  const std::string q = lower(search_);
  for (const auto& [id, p] : ctx_.allPlayers()) {
    if (p.retired && !retired_) continue;
    const TeamId t = p.team.value_or("");
    if (team_ == "mine" && t != me) continue;
    if (team_ == "free" && (!t.empty() || p.retired)) continue;
    if (team_ != "all" && team_ != "mine" && team_ != "free" && t != team_) continue;
    if (role_ >= 0 && static_cast<int>(p.role) != role_) continue;
    if (!q.empty() && lower(p.name).find(q) == std::string::npos) continue;
    TableRowData r;
    r.mine = !me.empty() && t == me && team_ != "mine";
    r.dim = isInjured(p) || p.retired.has_value();
    TableCell nameCell = textCell(p.name);
    nameCell.link = id;
    TableCell teamCell = textCell(t.empty() ? (p.retired ? "Retired" : "Free") : t, CellTone::Muted);
    if (!t.empty()) teamCell.badgeTeam = t;
    TableCell roleCell = textCell(roleText(p.role) + (p.overseas ? " · OS" : ""), CellTone::Muted);
    const auto pv = potentialView(w, p, me);
    TableCell pot = pv ? numCell(pv->exact ? pv->lo : (pv->lo + pv->hi) / 2) : textCell("\xe2\x80\x93", CellTone::Muted);
    if (pv) pot.text = pv->exact ? std::string(t20::potentialGrade(pv->lo)) + " " + num(pv->lo) : num(pv->lo) + "-" + num(pv->hi);
    TableCell value = numCell(marketValue(p));
    value.text = money(marketValue(p));
    TableCell fit = numCell(isInjured(p) ? -1 : fitnessOf(p));
    fit.text = isInjured(p) ? "Injured" : pct(fitnessOf(p));
    fit.tone = isInjured(p) ? CellTone::Bad : fitnessOf(p) < 75 ? CellTone::Gold : CellTone::Muted;
    r.cells = {nameCell, teamCell, roleCell, numCell(p.age), ratingCell(mainRating(p)), ratingCell(p.bat),
               ratingCell(p.bowlType ? p.bowl : std::nan("")), ratingCell(p.field), pot, value, fit};
    rows_.push_back(std::move(r));
  }
}

void PlayersScreen::drawBrowser(float width) {
  (void)width;
  const TeamId me = ctx_.userTeam();
  beginPanel("##browser", nullptr);
  eyebrowText("Team");
  auto teamChip = [&](const std::string& key, const std::string& label) {
    if (chip((label + "##team").c_str(), team_ == key)) team_ = key;
    ImGui::SameLine(0, 6);
    if (ImGui::GetContentRegionAvail().x < 70) ImGui::NewLine();
  };
  if (!me.empty()) teamChip("mine", "My squad");
  teamChip("all", "All");
  teamChip("free", "Free agents");
  for (const auto& t : ctx_.league()) teamChip(t.id, t.id);
  ImGui::NewLine();
  eyebrowText("Role");
  static const char* roles[] = {"All", "Keepers", "Batters", "All-rounders", "Bowlers"};
  for (int i = -1; i < 4; ++i) {
    if (chip((std::string(roles[i + 1]) + "##role").c_str(), role_ == i)) role_ = i;
    ImGui::SameLine(0, 6);
  }
  ImGui::SameLine(0, 20);
  textInput("##search", "", &search_, 40, 220);
  ImGui::SameLine(0, 12);
  checkbox("Include retired", &retired_);
  ImGui::Dummy(ImVec2(0, 6));
  rebuildRows();
  if (!state_.initialised) state_ = {4, false, true, true};
  const std::vector<TableColumn> cols = {textColumn("Player", 2.4f), textColumn("Team", 1.0f), textColumn("Role", 1.5f),
                                         numColumn("Age"),           numColumn("Rating", "His main skill"),
                                         numColumn("Bat"),           numColumn("Bowl"),
                                         numColumn("Field"),         numColumn("Potential", "Exact for your own players and scouted prospects; otherwise your scouts' range"),
                                         numColumn("Value", "Market value"), numColumn("Fitness")};
  const TableClick click = sortableTable("##players", cols, rows_, state_, 40, true,
                                         std::to_string(rows_.size()) + " players");
  if (!click.link.empty()) ctx_.openCard(click.link);
  if (rows_.empty()) emptyState("Nobody matches.");
  endPanel();
}

void PlayersScreen::drawRecords(float width) {
  const Records& rec = ctx_.game.state().career.records;
  const auto& th = currentTheme();
  auto list = [&](const char* title, auto& rows, auto line) {
    gridNext();
    beginPanel(title, title);
    if (rows.empty()) hint("Nothing yet.");
    int i = 1;
    for (const auto& r : rows) {
      ImGui::PushID(i);
      ImGui::TextColored(vec(th.muted), "%2d", i++);
      ImGui::SameLine(0, 8);
      line(r);
      ImGui::PopID();
    }
    endPanel();
  };
  if (beginGrid("##records", columnsFor(width, 380, 3), 16)) {
    list("Highest scores", rec.highScores, [&](const ScoreRec& r) {
      nameLink(ctx_, r.id);
      ImGui::SameLine(0, 8);
      ImGui::Text("%d%s (%d) v %s", r.runs, r.notOut ? "*" : "", r.balls, r.vs.c_str());
    });
    list("Best bowling", rec.bestBowling, [&](const BowlRec& r) {
      nameLink(ctx_, r.id);
      ImGui::SameLine(0, 8);
      ImGui::Text("%d/%d v %s", r.wickets, r.runs, r.vs.c_str());
    });
    list("Most sixes in an innings", rec.mostSixes, [&](const SixesRec& r) {
      nameLink(ctx_, r.id);
      ImGui::SameLine(0, 8);
      ImGui::Text("%d sixes (%d off %d)", r.sixes, r.runs, r.balls);
    });
    list("Fastest fifties", rec.fastestFifties, [&](const FastRec& r) {
      nameLink(ctx_, r.id);
      ImGui::SameLine(0, 8);
      ImGui::Text("%d balls v %s", r.balls, r.vs.c_str());
    });
    list("Fastest hundreds", rec.fastestHundreds, [&](const FastRec& r) {
      nameLink(ctx_, r.id);
      ImGui::SameLine(0, 8);
      ImGui::Text("%d balls v %s", r.balls, r.vs.c_str());
    });
    list("Highest totals", rec.highTotals, [&](const TotalRec& r) { ImGui::Text("%s %d/%d v %s", r.team.c_str(), r.runs, r.wickets, r.vs.c_str()); });
    list("Lowest totals", rec.lowTotals, [&](const TotalRec& r) { ImGui::Text("%s %d/%d v %s", r.team.c_str(), r.runs, r.wickets, r.vs.c_str()); });
    list("Biggest wins (runs)", rec.bigWinsRuns, [&](const MarginRec& r) { ImGui::Text("%s by %d runs v %s", r.team.c_str(), r.margin, r.vs.c_str()); });
    list("Biggest wins (wickets)", rec.bigWinsWkts, [&](const MarginRec& r) {
      ImGui::Text("%s by %d wickets v %s%s", r.team.c_str(), r.margin, r.vs.c_str(),
                  r.ballsLeft ? (" (" + std::to_string(*r.ballsLeft) + " balls left)").c_str() : "");
    });
    endGrid();
  }
}

void PlayersScreen::drawHall() {
  const auto& st = ctx_.game.state();
  const auto& th = currentTheme();
  beginPanel("##hall", "Hall of fame");
  hint("Retired greats, judged on their peak and what they did in your league.");
  ImGui::Dummy(ImVec2(0, 4));
  const auto hall = hallOfFame(st.world, st.career.players);
  if (hall.empty()) emptyState("Nobody yet: players are inducted when they retire.");
  for (const auto& h : hall) {
    ImGui::PushID(h.id.c_str());
    if (h.legend) {
      tag("Legend", TagTone::Hot);
      ImGui::SameLine(0, 8);
    }
    nameLink(ctx_, h.id);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(vec(th.muted), "%s · retired %d at %d · peak %d", h.team.value_or("-").c_str(), h.year, h.age,
                       static_cast<int>(std::lround(h.peak)));
    std::string why;
    for (const auto& r : h.reasons) why += (why.empty() ? "" : " · ") + r;
    if (!why.empty()) hint(why);
    ImGui::PopID();
  }
  endPanel();
}

void PlayersScreen::drawCreate(float width) {
  auto& game = ctx_.game;
  const auto& th = currentTheme();
  const TeamId me = ctx_.userTeam();
  static const Role roles[] = {Role::WK, Role::BAT, Role::AR, Role::BOWL};
  if (!newInit_) {
    newDraft_ = draftFrom(newPlayerTemplate(roles[newRole_]));
    newDraft_.name.clear();
    newInit_ = true;
  }
  beginPanel("##create", "Design a player");
  if (me.empty()) {
    emptyState("Pick a team on the Home screen first.");
    endPanel();
    return;
  }
  hint("He joins " + teamNameOf(me) + "'s squad straight away (up to " + std::to_string(SQUAD_MAX) + " players).");
  ImGui::Dummy(ImVec2(0, 4));
  eyebrowText("Role");
  static const char* roleNames[] = {"Wicketkeeper", "Batter", "All-rounder", "Bowler"};
  for (int i = 0; i < 4; ++i) {
    if (chip((std::string(roleNames[i]) + "##nr").c_str(), newRole_ == i) && newRole_ != i) {
      newRole_ = i;
      const std::string keep = newDraft_.name;
      newDraft_ = draftFrom(newPlayerTemplate(roles[i]));
      newDraft_.name = keep;
    }
    ImGui::SameLine(0, 6);
  }
  ImGui::NewLine();
  if (beginGrid("##newid", width >= 700 ? 3 : 1, 14)) {
    gridNext();
    textInput("##nname", "Name", &newDraft_.name, PLAYER_NAME_MAX);
    if (const auto* e = newErrors_.find("name")) ImGui::TextColored(vec(th.red), "%s", e->c_str());
    gridNext();
    eyebrowText("Age");
    int age = std::isnan(newDraft_.age) ? 22 : static_cast<int>(newDraft_.age);
    if (numberStepper("##nage", &age, AGE_MIN, AGE_MAX)) newDraft_.age = age;
    gridNext();
    ImGui::Dummy(ImVec2(0, 14));
    checkbox("Overseas", &newDraft_.overseas);
    ImGui::SameLine(0, 16);
    checkbox("Club captain", &newCaptain_);
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 6));
  auto sliders = [&](const char* title, auto& defs, auto& set, const char* prefix) {
    eyebrowText(title);
    for (const auto& d : defs) {
      if (!set.contains(d.key)) continue;
      int v = static_cast<int>(std::lround(set[d.key]));
      const std::string id = std::string(prefix) + "." + std::string(name(d.key));
      if (sliderInt(id.c_str(), std::string(d.label).c_str(), &v, static_cast<int>(SKILL_MIN), static_cast<int>(SKILL_MAX))) set.set(d.key, v);
    }
  };
  if (beginGrid("##newskills", width >= 700 ? 2 : 1, 18)) {
    gridNext();
    sliders("Batting", BAT_SKILLS, newDraft_.skills.bat, "bat");
    gridNext();
    const auto type = parseBowlType(newDraft_.bowlType.value_or(""));
    if (type) {
      sliders(isPaceType(type) ? "Pace bowling" : "Spin bowling", isPaceType(type) ? PACE_SKILLS : SPIN_SKILLS, newDraft_.skills.bowl, "bowl");
      ImGui::Dummy(ImVec2(0, 6));
    }
    sliders("Fielding", FIELD_SKILLS, newDraft_.skills.field, "field");
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  if (button("Sign him", ButtonKind::Primary)) {
    newDraft_.captain = newCaptain_;
    const app::CreateOutcome r = game.createNewPlayer(me, newDraft_);
    newErrors_ = r.errors;
    if (r.ok) {
      newMsg_ = newDraft_.name + " has joined the squad.";
      newInit_ = false;
      newCaptain_ = false;
      ctx_.openCard(r.id);
    } else {
      newMsg_.clear();
    }
  }
  if (const auto* e = newErrors_.find("form")) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(vec(th.red), "%s", e->c_str());
  } else if (!newErrors_.empty()) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(vec(th.red), "%s", newErrors_.begin()->second.c_str());
  }
  if (!newMsg_.empty()) notice(newMsg_, NoticeKind::Ok);
  endPanel();
}

}  // namespace

std::unique_ptr<IScreenView> makePlayersScreen(UiContext& ctx) { return std::make_unique<PlayersScreen>(ctx); }

}  // namespace t20::ui
