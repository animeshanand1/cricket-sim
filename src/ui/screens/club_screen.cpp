// Club: the franchise as a business, apart from the player purse. Every decision is a plan for the
// next season: the ground (stands, hospitality, fan zone, curator), the academy and the
// high-performance centre, ticket prices, the home pitch, the prize-money share, coaching staff,
// personal coaches, sponsors, and the books.

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/club.hpp"
#include "engine/finance.hpp"
#include "engine/pitch.hpp"
#include "engine/world.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/screens/common.hpp"
#include "ui/screens/screens.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

std::string moneyL(double l) { return money(std::optional<double>(l)); }

std::string titleCase(std::string s) {
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
  return s;
}

class ClubScreen : public IScreenView {
 public:
  explicit ClubScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override;

 private:
  void drawOverview(const Club& club, const TeamId& team, float width);
  void drawGround(const Club& club, const TeamId& team, float width);
  void drawPeople(const Club& club, const TeamId& team, float width);
  void drawSponsors(const Club& club, const TeamId& team);
  void drawBooks(const Club& club);

  UiContext& ctx_;
  int tab_ = 0;
  int coachPlayer_ = -1, coachArea_ = -1, coachTier_ = 1;
  std::string msg_;
};

void ClubScreen::draw(float width) {
  const World& w = ctx_.game.state().world;
  const TeamId team = w.manager.team.value_or(ctx_.game.state().setup.userTeamId);
  if (team.empty()) {
    pageHeader("Club", "", "Club");
    emptyState("Pick a team on the Home screen first.");
    return;
  }
  const Club club = clubOf(w, team);
  const int plan = planYear(w, club);
  const TeamMeta tm = teamMetaOf(team);
  pageHeader(tm.name, "The club as a business. Every decision here is for season " + std::to_string(plan + 1) + ".", "Club", &tm);
  ImGui::Dummy(ImVec2(0, 6));
  if (beginGrid("##clubkpi", columnsFor(width, 200, 4), 10)) {
    gridNext();
    kpiTile("Bank", moneyL(club.bank), club.bank < 0 ? TagTone::Weak : TagTone::Hot);
    gridNext();
    kpiTile("Free to spend", moneyL(available(w, club)));
    gridNext();
    kpiTile("Popularity", pct(club.popularity));
    gridNext();
    kpiTile("Ground", fmt_indian(static_cast<int64_t>(capacity(club, team))) + " seats");
    endGrid();
  }
  if (!msg_.empty()) notice(msg_, NoticeKind::Warn);
  ImGui::Dummy(ImVec2(0, 6));
  tab_ = tabs({{"overview", "Overview", ""}, {"ground", "Ground and youth", ""}, {"people", "Coaching", ""},
               {"sponsors", "Sponsors", ""}, {"books", "Books", ""}},
              tab_);
  ImGui::Dummy(ImVec2(0, 8));
  switch (tab_) {
    case 0: drawOverview(club, team, width); break;
    case 1: drawGround(club, team, width); break;
    case 2: drawPeople(club, team, width); break;
    case 3: drawSponsors(club, team); break;
    default: drawBooks(club); break;
  }
}

void ClubScreen::drawOverview(const Club& club, const TeamId& team, float width) {
  const World& w = ctx_.game.state().world;
  const bool two = width >= 900;
  if (beginGrid("##ov", two ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##money", "Where the money goes");
    keyValue("Central share (each season)", moneyL(CENTRAL_SHARE));
    keyValue("Running costs (each season)", moneyL(RUNNING_COSTS));
    keyValue("Facility upkeep", moneyL(upkeep(club.levels)));
    keyValue("Coaching staff", moneyL(staffCost(club.staffPlan)));
    keyValue("Committed for next season", moneyL(committed(w, club)));
    ImGui::Dummy(ImVec2(0, 4));
    hint("Prize money, gate receipts, hospitality and sponsors come in on top. Finish a season in the red and the "
         "board loses " + std::to_string(static_cast<int>(RED_INK_PENALTY)) + " confidence.");
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##popularity", "Fans");
    meter(static_cast<float>(club.popularity), (pct(club.popularity) + " popularity").c_str());
    const GateTake g = projectGate(w, team, club.plan.tier);
    keyValue("Expected crowd", fmt_indian(static_cast<int64_t>(g.att)) + " of " + fmt_indian(static_cast<int64_t>(g.cap)));
    keyValue("Takings a home game", moneyL(g.revenue));
    hint("Winning, cheap tickets and the fan zone grow the following; elite prices thin it.");
    endPanel();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##history", "Seasons");
  if (club.history.empty()) emptyState("No seasons closed yet.");
  else if (ImGui::BeginTable("##hist", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
    for (const char* h : {"Season", "Finished", "Prize", "Income", "Spend", "Net", "Bank"}) ImGui::TableSetupColumn(h);
    ImGui::TableHeadersRow();
    for (auto it = club.history.rbegin(); it != club.history.rend(); ++it) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%d", it->year);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(ordinal(it->position).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(moneyL(it->prize).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(moneyL(it->income).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(moneyL(it->spend).c_str());
      ImGui::TableNextColumn();
      ImGui::TextColored(vec(it->net < 0 ? currentTheme().red : currentTheme().green), "%s%s", it->net < 0 ? "-" : "+",
                         moneyL(std::abs(it->net)).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(moneyL(it->bank).c_str());
    }
    ImGui::EndTable();
  }
  endPanel();
  if (!club.log.empty()) {
    ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##log", "Club diary");
    int n = 0;
    for (auto it = club.log.rbegin(); it != club.log.rend() && n < 12; ++it, ++n) ImGui::BulletText("%s", it->text.c_str());
    endPanel();
  }
}

void ClubScreen::drawGround(const Club& club, const TeamId& team, float width) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  beginPanel("##facilities", "Facilities");
  hint("Building takes the rest of the year: a new level opens next season. Each level costs " + moneyL(UPKEEP_PER_LEVEL) + " a season to run.");
  ImGui::Dummy(ImVec2(0, 4));
  if (beginGrid("##facgrid", columnsFor(width - 36, 340, 3), 12)) {
    for (const auto& f : FACILITIES()) {
      gridNext();
      const std::string key(f.key);
      ImGui::PushID(key.c_str());
      const int level = club.levels[key], max = maxLevel(f.key);
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::TextUnformatted(std::string(f.label).c_str());
      }
      progressBar(static_cast<float>(level) / static_cast<float>(max), "Level " + std::to_string(level) + "/" + std::to_string(max));
      hint(f.help);
      if (const Building* b = club.building.find(key)) {
        ImGui::TextColored(vec(th.goldText), "Level %d being built, opens %d", b->level, b->ready);
      } else if (const auto cost = upgradeCost(f.key, level)) {
        if (button(("Build level " + std::to_string(level + 1) + " · " + moneyL(*cost)).c_str(), ButtonKind::Small,
                   *cost <= available(w, club))) {
          if (!game.clubUpgradeOrdered(key)) msg_ = "Not enough money free for that.";
        }
      } else {
        ImGui::TextColored(vec(th.muted), "Fully built");
      }
      ImGui::PopID();
    }
    endGrid();
  }
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));

  const bool two = width >= 900;
  if (beginGrid("##groundplan", two ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##tickets", "Ticket prices");
    for (const auto& t : TICKET_TIERS()) {
      const std::string key(t.key);
      if (chip((std::string(t.label) + "##tier").c_str(), club.plan.tier == key)) game.clubTicketsSet(key);
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    if (const TicketTier* t = ticketTier(club.plan.tier)) hint(std::string(t->note));
    const GateTake g = projectGate(w, team, club.plan.tier);
    keyValue("Expected crowd", fmt_indian(static_cast<int64_t>(g.att)) + " (" + pct(g.cap > 0 ? 100 * g.att / g.cap : 0) + " full)");
    keyValue("Tickets", moneyL(g.tickets));
    keyValue("Food, drink, merchandise", moneyL(g.extras));
    keyValue("Hospitality", moneyL(g.hospitality));
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##pitchplan", "Home pitch and prize money");
    const bool curator = levelsAfterBuild(club)["curator"] >= 1;
    eyebrowText("Home pitch");
    if (!curator) hint("Hire a curator (Facilities) to choose the surface for your home games.");
    if (chip("Let it be##hp", !club.plan.pitch)) game.clubPitchSet(std::nullopt);
    for (Pitch p : PITCH_KEYS) {
      ImGui::SameLine(0, 6);
      if (ImGui::GetContentRegionAvail().x < 110) ImGui::NewLine();
      ImGui::BeginDisabled(!curator);
      if (chip((std::string(pitchDef(p).label) + "##hp").c_str(), club.plan.pitch == p)) game.clubPitchSet(p);
      ImGui::EndDisabled();
    }
    ImGui::Dummy(ImVec2(0, 8));
    eyebrowText("Share of prize money with the players");
    for (int share : PRIZE_SHARES) {
      if (chip((std::to_string(share) + "%##ps").c_str(), club.plan.prizeShare == share)) game.clubPrizeShareSet(share);
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    hint("Sharing costs the club money but buys goodwill: cheaper contract renewals next off-season (up to " +
         std::to_string(static_cast<int>(MAX_GOODWILL)) + "% off).");
    endPanel();
    endGrid();
  }
}

void ClubScreen::drawPeople(const Club& club, const TeamId& team, float width) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  const bool two = width >= 900;
  if (beginGrid("##people", two ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##staff", "Coaching staff");
    hint("Between seasons the staff coach every player aged " + std::to_string(STAFF_MAX_AGE) +
         " or under in their area; better coaches lift more.");
    ImGui::Dummy(ImVec2(0, 4));
    std::vector<std::string> tiers;
    for (const auto& t : STAFF_TIERS()) tiers.push_back(std::string(t.label));
    for (const auto& r : STAFF_ROLES()) {
      const std::string key(r.key);
      ImGui::PushID(key.c_str());
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::TextUnformatted(std::string(r.label).c_str());
      }
      const int cur = staffOf(club.staffPlan, key);
      const int next = segmented(tiers, cur);
      if (next != cur && !game.clubStaffSet(key, next)) msg_ = "Not enough money free for that coach.";
      hint(moneyL(STAFF_TIERS()[static_cast<size_t>(cur)].cost) + " a season");
      ImGui::Dummy(ImVec2(0, 4));
      ImGui::PopID();
    }
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));

    beginPanel("##coaches", "Personal coaches");
    const int plan = planYear(w, club);
    const auto mine = coachesIn(club, plan);
    const int slots = coachSlots(levelsAfterBuild(club));
    hint("One player, one area, one season. " + std::to_string(mine.size()) + " of " + std::to_string(slots) +
         " slots used (the high-performance centre adds more).");
    ImGui::Dummy(ImVec2(0, 4));
    for (const Coach* c : mine) {
      ImGui::PushID(c->playerId.c_str());
      nameLink(ctx_, c->playerId);
      ImGui::SameLine(0, 8);
      const CoachTier* t = coachTier(c->tier);
      ImGui::TextColored(vec(th.muted), "%s · %s · %s", titleCase(c->area).c_str(), t ? std::string(t->label).c_str() : "?",
                         t ? moneyL(t->cost).c_str() : "");
      ImGui::SameLine(0, 8);
      if (button("Remove", ButtonKind::Small)) game.clubCoachRemoved(c->playerId);
      ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(0, 6));
    // Assign a new one
    std::vector<std::string> names;
    std::vector<PlayerId> ids;
    if (const WorldTeam* wt = w.teams.find(team))
      for (const auto& id : wt->squad)
        if (const Player* p = ctx_.player(id)) {
          names.push_back(p->name + " (" + std::to_string(p->age) + ")");
          ids.push_back(id);
        }
    selectBox("##cp", "Player", names, &coachPlayer_, "Choose a player", 0);
    if (coachPlayer_ >= 0 && coachPlayer_ < static_cast<int>(ids.size())) {
      const Player* p = w.players.find(ids[static_cast<size_t>(coachPlayer_)]);
      const auto areas = p ? areasFor(*p) : std::vector<std::string>{};
      std::vector<std::string> areaNames;
      for (const auto& a : areas) areaNames.push_back(titleCase(a));
      if (coachArea_ >= static_cast<int>(areas.size())) coachArea_ = -1;
      ImGui::Dummy(ImVec2(0, 4));
      selectBox("##ca", "Area", areaNames, &coachArea_, "Choose an area", 0);
      ImGui::Dummy(ImVec2(0, 4));
      std::vector<std::string> tierNames;
      for (int t = 1; t <= 3; ++t) tierNames.push_back(std::string(coachTier(t)->label) + " " + moneyL(coachTier(t)->cost));
      coachTier_ = segmented(tierNames, coachTier_ - 1) + 1;
      ImGui::Dummy(ImVec2(0, 4));
      if (button("Hire the coach", ButtonKind::Primary, coachArea_ >= 0)) {
        if (game.clubCoachAssigned(ids[static_cast<size_t>(coachPlayer_)], areas[static_cast<size_t>(coachArea_)], coachTier_)) {
          coachPlayer_ = coachArea_ = -1;
          msg_.clear();
        } else {
          msg_ = "Couldn't hire that coach: no free slot, or not enough money free.";
        }
      }
    }
    endPanel();
    endGrid();
  }
}

void ClubScreen::drawSponsors(const Club& club, const TeamId& team) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  const int plan = planYear(w, club);
  const auto offers = sponsorOffers(w, team);
  beginPanel("##sponsors", "Sponsors");
  hint("Six slots. Offers depend on your popularity; a signed deal pays every season it covers.");
  ImGui::Dummy(ImVec2(0, 4));
  for (const auto& slot : SPONSOR_SLOTS()) {
    const std::string key(slot.key);
    ImGui::PushID(key.c_str());
    {
      FontScope fs(FontRole::BodyStrong);
      ImGui::TextUnformatted(std::string(slot.label).c_str());
    }
    if (const Sponsor* d = slotDeal(club, slot.key, plan)) {
      ImGui::SameLine(0, 10);
      ImGui::TextColored(vec(th.green), "%s · %s a season · until %d", d->brand.c_str(), moneyL(d->fee).c_str(), d->from + d->years - 1);
    } else if (const auto* list = offers.find(key)) {
      for (size_t i = 0; i < list->size(); ++i) {
        const Sponsor& o = (*list)[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Indent(12);
        std::string type = o.type;
        for (const auto& t : DEAL_TYPES())
          if (t.key == o.type) type = std::string(t.label);
        ImGui::TextUnformatted(o.brand.c_str());
        ImGui::SameLine(0, 8);
        std::string terms = type + " · " + moneyL(o.fee) + " × " + std::to_string(o.years) + " season" + (o.years == 1 ? "" : "s");
        if (o.bonus) terms += " · bonus " + moneyL(o.bonus->playoffs) + " playoffs, " + moneyL(o.bonus->title) + " title";
        ImGui::TextColored(vec(th.muted), "%s", terms.c_str());
        ImGui::SameLine(0, 8);
        if (button("Sign", ButtonKind::Small)) game.clubSponsorSigned(key, i);
        ImGui::Unindent(12);
        ImGui::PopID();
      }
    }
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::PopID();
  }
  endPanel();
}

void ClubScreen::drawBooks(const Club& club) {
  const auto& th = currentTheme();
  beginPanel("##books", "The books");
  if (club.books.empty()) emptyState("Nothing booked yet.");
  for (auto it = club.books.rbegin(); it != club.books.rend(); ++it) {
    const Books& b = it->second;
    double in = 0, out = 0;
    for (const auto& [k, v] : b.income) in += v;
    for (const auto& [k, v] : b.spend) out += v;
    const std::string title = "Year " + std::to_string(it->first);
    const std::string sum = "in " + moneyL(in) + " · out " + moneyL(out);
    if (foldRow(title.c_str(), title.c_str(), sum.c_str(), it == club.books.rbegin())) {
      if (beginGrid("##inout", 2, 18)) {
        gridNext();
        eyebrowText("Income");
        for (const auto& [k, v] : b.income) keyValue(titleCase(k), moneyL(v));
        gridNext();
        eyebrowText("Spending");
        for (const auto& [k, v] : b.spend) keyValue(titleCase(k), moneyL(v));
        endGrid();
      }
      endFoldRow();
    }
  }
  if (!club.gates.empty()) {
    ImGui::Dummy(ImVec2(0, 8));
    sectionHeading("Home gates");
    int n = 0;
    for (auto it = club.gates.rbegin(); it != club.gates.rend() && n < 10; ++it, ++n) {
      ImGui::TextColored(vec(th.muted), "%d · game %d", it->year, it->no);
      ImGui::SameLine(0, 8);
      ImGui::Text("v %s · %s of %s · %s", it->opp.value_or("?").c_str(), fmt_indian(static_cast<int64_t>(it->att)).c_str(),
                  fmt_indian(static_cast<int64_t>(it->cap)).c_str(), moneyL(it->revenue).c_str());
    }
  }
  endPanel();
}

}  // namespace

std::unique_ptr<IScreenView> makeClubScreen(UiContext& ctx) { return std::make_unique<ClubScreen>(ctx); }

}  // namespace t20::ui
