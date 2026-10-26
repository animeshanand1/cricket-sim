// Off-season: what the year did to the league (ageing, retirements, the academy, coaching), then
// the user's decisions stage by stage: contracts (or retention in a mega year), the youth draft
// and the auction. Finishing it starts the next season.

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/ageing.hpp"
#include "engine/auction.hpp"
#include "engine/board.hpp"
#include "engine/club.hpp"
#include "engine/finance.hpp"
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
#include "ui/widgets/tags.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

std::string moneyL(double l) { return money(std::optional<double>(l)); }

bool contains(const std::vector<PlayerId>& v, const PlayerId& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

class OffseasonScreen : public IScreenView {
 public:
  explicit OffseasonScreen(UiContext& ctx) : ctx_(ctx) {}
  void onEnter() override { limit_ = 0; }
  void draw(float width) override;

 private:
  void drawStages(const Offseason& o);
  void drawPurse(const TeamId& me, float width);
  void drawReport(const OffseasonReport& r, const TeamId& me);
  void drawContracts(const TeamId& me);
  void drawRetention(const Offseason& o, const TeamId& me);
  void drawDraft(const TeamId& me);
  void drawAuction(const TeamId& me);
  void playerLine(const Player& p);

  UiContext& ctx_;
  double limit_ = 0;
  double raise_ = 0;
};

void OffseasonScreen::playerLine(const Player& p) {
  nameLink(ctx_, p);
  playerTags(p, {false, false, false});
  ImGui::SameLine(0, 8);
  ImGui::TextColored(vec(currentTheme().muted), "%s · %d yrs · %d", roleText(p.role).c_str(), p.age, headlineRating(p));
}

void OffseasonScreen::draw(float width) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  if (!w.offseason) {
    pageHeader("Off-season", "", "Off-season");
    emptyState("No off-season under way. It starts when a season ends.");
    if (w.lastOffseason) drawReport(*w.lastOffseason, ctx_.userTeam());
    if (button("Back", ButtonKind::Primary)) ctx_.goTo(ctx_.seasonActive() ? Screen::season : Screen::home);
    return;
  }
  const Offseason& o = *w.offseason;
  const TeamId me = o.userTeam.value_or(ctx_.userTeam());
  const TeamMeta tm = teamMetaOf(me);
  pageHeader("Off-season " + std::to_string(o.year), o.mega ? "A mega auction year: squads are rebuilt from scratch." : "Contracts, the youth draft and the mini auction.",
             "Building next season's squad", &tm);
  ImGui::Dummy(ImVec2(0, 6));
  drawStages(o);
  ImGui::Dummy(ImVec2(0, 6));
  drawPurse(me, width);
  ImGui::Dummy(ImVec2(0, 8));
  if (o.stage == "contracts") drawContracts(me);
  else if (o.stage == "retention") drawRetention(o, me);
  else if (o.stage == "draft") drawDraft(me);
  else if (o.stage == "auction") drawAuction(me);
  // the stage may have ended the off-season
  if (game.state().world.lastOffseason && game.state().world.offseason) {
    ImGui::Dummy(ImVec2(0, 8));
    drawReport(*game.state().world.lastOffseason, me);
  }
}

void OffseasonScreen::drawStages(const Offseason& o) {
  const auto& th = currentTheme();
  const std::vector<std::pair<std::string, std::string>> stages = {
      {o.mega ? "retention" : "contracts", o.mega ? "Retention" : "Contracts"}, {"draft", "Youth draft"}, {"auction", "Auction"}};
  int cur = 0;
  for (size_t i = 0; i < stages.size(); ++i)
    if (stages[i].first == o.stage) cur = static_cast<int>(i);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  float x = p.x;
  for (size_t i = 0; i < stages.size(); ++i) {
    const bool done = static_cast<int>(i) < cur, now = static_cast<int>(i) == cur;
    const std::string label = std::to_string(i + 1) + "  " + stages[i].second;
    const ImVec2 sz = pill(dl, ImVec2(x, p.y), FontRole::BodyStrong, label, now ? col(th.goldSoft) : col(th.surface2),
                           now ? col(th.goldText) : done ? col(th.green) : col(th.muted), now ? col(th.gold) : col(th.line), 14, 6);
    x += sz.x + 10;
  }
  ImGui::Dummy(ImVec2(x - p.x, 30));
}

void OffseasonScreen::drawPurse(const TeamId& me, float width) {
  const World& w = ctx_.game.state().world;
  const int squad = static_cast<int>(w.teams.at(me).squad.size());
  if (beginGrid("##purse", columnsFor(width, 180, 5), 10)) {
    gridNext();
    kpiTile("Purse left", moneyL(purseLeft(w, me)), TagTone::Hot);
    gridNext();
    kpiTile("Payroll", moneyL(payroll(w, me)));
    gridNext();
    kpiTile("Squad", std::to_string(squad) + " (" + std::to_string(SQUAD_MIN) + "-" + std::to_string(SQUAD_MAX) + ")",
            squad < SQUAD_MIN ? TagTone::Weak : TagTone::Default);
    gridNext();
    kpiTile("Overseas", std::to_string(overseasCount(w, me)) + "/" + std::to_string(MAX_OVERSEAS_SQUAD));
    gridNext();
    const RoleCounts rc = roleCounts(w, me);
    kpiTile("WK·BAT·AR·BOWL", std::to_string(rc.wk) + "·" + std::to_string(rc.bat) + "·" + std::to_string(rc.ar) + "·" + std::to_string(rc.bowl));
    endGrid();
  }
}

void OffseasonScreen::drawReport(const OffseasonReport& r, const TeamId& me) {
  const auto& th = currentTheme();
  beginPanel("##report", "The year in review");
  const std::string summary = std::to_string(r.retired.size()) + " retired · " + std::to_string(r.academy.size()) + " academy · " +
                              std::to_string(r.drafted.size()) + " drafted";
  if (foldRow("##aged", "Ageing: your squad", nullptr, true)) {
    for (const auto& a : r.aged) {
      if (a.team != me) continue;
      const double before = std::max({a.before.bat, a.before.bowl}), after = std::max({a.after.bat, a.after.bowl});
      const double d = after - before;
      if (std::fabs(d) < 0.5) continue;
      ImGui::PushID(a.id.c_str());
      nameLink(ctx_, a.id);
      ImGui::SameLine(0, 8);
      ImGui::TextColored(vec(th.muted), "%d yrs", a.age);
      ImGui::SameLine(0, 8);
      ImGui::TextColored(vec(d > 0 ? th.green : th.red), "%s", signedNum(d, 1).c_str());
      ImGui::PopID();
    }
    endFoldRow();
  }
  if (foldRow("##league", "Around the league", summary.c_str())) {
    if (!r.retired.empty()) {
      eyebrowText("Retired");
      for (const auto& x : r.retired) {
        ImGui::PushID(x.id.c_str());
        nameLink(ctx_, x.id);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(vec(th.muted), "%s · %d yrs", x.team.c_str(), x.age);
        ImGui::PopID();
      }
    }
    if (!r.academy.empty()) {
      ImGui::Dummy(ImVec2(0, 4));
      eyebrowText("Academy graduates");
      for (const auto& x : r.academy) {
        ImGui::PushID(x.id.c_str());
        nameLink(ctx_, x.id);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(vec(th.muted), "%s", x.team.c_str());
        ImGui::PopID();
      }
    }
    if (!r.released.empty()) {
      ImGui::Dummy(ImVec2(0, 4));
      eyebrowText("Released");
      std::string names;
      for (const auto& x : r.released) names += (names.empty() ? "" : ", ") + ctx_.playerName(x.id) + " (" + x.team + ")";
      hint(names);
    }
    endFoldRow();
  }
  if (r.coaching && !r.coaching->empty() && foldRow("##coaching", "Your coaches' work", nullptr)) {
    for (const auto& c : *r.coaching) {
      ImGui::PushID(c.id.c_str());
      nameLink(ctx_, c.id);
      ImGui::SameLine(0, 8);
      ImGui::TextColored(vec(c.gain > 0 ? th.green : th.muted), "%s → %s", num(c.before).c_str(), num(c.after).c_str());
      ImGui::PopID();
    }
    endFoldRow();
  }
  endPanel();
}

void OffseasonScreen::drawContracts(const TeamId& me) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  beginPanel("##contracts", "Expiring contracts");
  hint("Renew the players you want to keep. Anyone left unsigned when you close contracts is released into the auction.");
  ImGui::Dummy(ImVec2(0, 4));
  const auto list = expiring(w, me);
  if (list.empty()) emptyState("No contracts left to settle.");
  for (const auto& id : list) {
    const Player* p = ctx_.player(id);
    if (!p) continue;
    ImGui::PushID(id.c_str());
    playerLine(*p);
    ImGui::Indent(14);
    ImGui::TextColored(vec(th.muted), "Was %s · worth %s", moneyL(p->contract ? p->contract->cost : 0).c_str(), moneyL(marketValue(*p)).c_str());
    for (int years : renewalYears(*p)) {
      ImGui::SameLine(0, 8);
      const std::string label = std::to_string(years) + (years == 1 ? " year · " : " years · ") + moneyL(renewalPrice(w, me, *p, years));
      ImGui::PushID(years);
      if (button(label.c_str(), ButtonKind::Small)) game.contractRenewed(id, years);
      ImGui::PopID();
    }
    ImGui::SameLine(0, 8);
    if (button("Release", ButtonKind::Danger)) game.contractReleased(id);
    ImGui::Unindent(14);
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PopID();
  }
  if (w.lastOffseason) {
    std::string renewed;
    for (const auto& r : w.lastOffseason->renewed)
      if (r.team == me) renewed += (renewed.empty() ? "" : ", ") + ctx_.playerName(r.id) + " (" + std::to_string(r.years) + "y, " + moneyL(r.cost) + ")";
    if (!renewed.empty()) hint("Renewed: " + renewed);
  }
  endPanel();
  beginStickyActions();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(vec(th.muted), list.empty() ? "All settled." : "%d unsigned: they'll be released.", static_cast<int>(list.size()));
  ImGui::SameLine();
  rightAlign(buttonWidth("Close contracts", ButtonKind::Primary));
  if (button("Close contracts", ButtonKind::Primary)) game.contractsClosed();
  endStickyActions();
}

void OffseasonScreen::drawRetention(const Offseason& o, const TeamId& me) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  beginPanel("##retention", "Retention");
  hint("Mega auction: keep up to " + std::to_string(MAX_RETAIN) + " players (at most " + std::to_string(MAX_RETAIN_CAPPED) + " capped, " +
       std::to_string(MAX_RETAIN_UNCAPPED) + " uncapped). Each one costs a slab of your purse; every slot you don't use is a Right to Match card at the auction. Rookies on draft deals stay anyway.");
  ImGui::Dummy(ImVec2(0, 4));
  const auto costs = retentionCosts(w, o.retain);
  double total = 0;
  for (double c : costs) total += c;
  ImGui::Text("Keeping %d · costs %s · %d RTM card(s)", static_cast<int>(o.retain.size()), moneyL(total).c_str(),
              MAX_RETAIN - static_cast<int>(o.retain.size()));
  ImGui::Dummy(ImVec2(0, 4));
  std::vector<const Player*> squad;
  for (const auto& id : w.teams.at(me).squad)
    if (const Player* p = ctx_.player(id)) squad.push_back(p);
  std::stable_sort(squad.begin(), squad.end(), [](const Player* a, const Player* b) { return mainRating(*a) > mainRating(*b); });
  for (const Player* p : squad) {
    ImGui::PushID(p->id.c_str());
    bool on = contains(o.retain, p->id);
    std::vector<PlayerId> next = o.retain;
    if (on) next.erase(std::find(next.begin(), next.end(), p->id));
    else next.push_back(p->id);
    const bool can = on || retentionValid(w, next);
    if (checkbox("##keep", &on, can)) game.retentionSet(next);
    ImGui::SameLine(0, 8);
    playerLine(*p);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(vec(th.muted), "· %s · worth %s", isCapped(*p) ? "capped" : "uncapped", moneyL(marketValue(*p)).c_str());
    ImGui::PopID();
  }
  endPanel();
  beginStickyActions();
  rightAlign(buttonWidth("Confirm retention", ButtonKind::Primary));
  if (button("Confirm retention", ButtonKind::Primary)) game.retentionClosed();
  endStickyActions();
}

void OffseasonScreen::drawDraft(const TeamId& me) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  if (!w.draft) return;
  const Draft& d = *w.draft;
  const auto clock = draftOnTheClock(w);
  const bool myPick = clock && *clock == me;
  int myLeft = 0;
  for (size_t i = d.made.size(); i < d.order.size(); ++i) myLeft += d.order[i] == me ? 1 : 0;

  beginPanel("##draft", "Youth draft");
  hint("Two rounds, worst team first, the order reversed in round two. Scout reports (" + std::to_string(d.scoutsLeft) +
       " left) show a prospect's real potential; otherwise you see your scouts' range.");
  ImGui::Dummy(ImVec2(0, 4));
  if (myPick) notice("You're on the clock: pick " + std::to_string(d.made.size() + 1) + ".", NoticeKind::Warn);
  else if (clock) hint(teamNameOf(*clock) + " are picking.");
  else notice("Every pick has been made.", NoticeKind::Ok);
  ImGui::Dummy(ImVec2(0, 4));

  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, vec(ColorRGBA(th.surface2.r, th.surface2.g, th.surface2.b, 0.55f)));
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
  if (ImGui::BeginTable("##pool", 6, flags)) {
    ImGui::TableSetupColumn("Prospect", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableSetupColumn("Potential", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.8f);
    ImGui::TableHeadersRow();
    for (const auto& id : draftAvailable(w)) {
      const Player* p = ctx_.player(id);
      if (!p) continue;
      ImGui::PushID(id.c_str());
      ImGui::TableNextRow(0, 30);
      ImGui::TableNextColumn();
      nameLink(ctx_, *p);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(roleText(p->role).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->age);
      ImGui::TableNextColumn();
      ratingNum(mainRating(*p));
      ImGui::TableNextColumn();
      const auto pv = potentialView(w, *p, me);
      if (pv && pv->exact) ImGui::TextColored(vec(th.goldText), "%s (%d)", std::string(t20::potentialGrade(pv->lo)).c_str(), static_cast<int>(std::lround(pv->lo)));
      else if (pv) ImGui::TextColored(vec(th.ink2), "%d-%d", static_cast<int>(std::lround(pv->lo)), static_cast<int>(std::lround(pv->hi)));
      else ImGui::TextColored(vec(th.muted), "?");
      ImGui::TableNextColumn();
      const bool scouted = d.scouted.contains(id);
      if (!scouted && button("Scout", ButtonKind::Small, d.scoutsLeft > 0)) game.scout(id);
      if (!scouted) ImGui::SameLine(0, 6);
      if (button("Draft", ButtonKind::Small, myPick)) game.makeDraftPick(id);
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleColor(2);
  endPanel();

  if (!d.made.empty()) {
    ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##picks", "Picks so far");
    for (auto it = d.made.rbegin(); it != d.made.rend(); ++it) {
      ImGui::PushID(it->no);
      ImGui::TextColored(vec(it->team == me ? th.goldText : th.muted), "%2d. %s", it->no, it->team.c_str());
      ImGui::SameLine(0, 8);
      if (it->id) nameLink(ctx_, *it->id);
      else ImGui::TextColored(vec(th.muted), "passed (squad full)");
      ImGui::PopID();
    }
    endPanel();
  }

  beginStickyActions();
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(vec(th.muted), "Your picks left: %d", myLeft);
  ImGui::SameLine();
  const char* label = myLeft > 0 ? "Let the scouts pick, then the auction" : "On to the auction";
  rightAlign(buttonWidth(label, ButtonKind::Primary));
  if (button(label, ButtonKind::Primary)) game.completeDraft();
  endStickyActions();
}

void OffseasonScreen::drawAuction(const TeamId& me) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  if (!w.auction) {
    beginPanel("##auction_done", "Auction");
    hint("The auction is over.");
    if (button("Finish the off-season", ButtonKind::Primary)) game.finishOffseason();
    endPanel();
    return;
  }
  const Auction& a = *w.auction;
  const auto waiting = awaitingUser(w);

  beginPanel("##auction", (a.type == "mega" ? "Mega auction" : "Mini auction"));
  progressBar(a.lots.empty() ? 1.0f : static_cast<float>(a.idx) / static_cast<float>(a.lots.size()),
              "Lot " + std::to_string(std::min<int>(a.idx, static_cast<int>(a.lots.size()))) + " of " + std::to_string(a.lots.size()));
  ImGui::Dummy(ImVec2(0, 6));
  if (a.lot) {
    const Lot& lot = *a.lot;
    const Player* p = ctx_.player(lot.id);
    if (p) {
      playerIdentity(ctx_, *p, 48);
      const std::string* set = a.setOf.find(lot.id);
      hint((set ? *set + " · " : std::string()) + "Base " + moneyL(lot.base) + " · Bat " + std::to_string(static_cast<int>(std::lround(p->bat))) +
           " · Bowl " + (p->bowlType ? std::to_string(static_cast<int>(std::lround(p->bowl))) : std::string("–")) +
           (p->formerTeam ? " · formerly " + *p->formerTeam : ""));
    }
    ImGui::Dummy(ImVec2(0, 6));
    {
      FontScope fs(FontRole::Kpi);
      ImGui::TextColored(vec(th.goldText), "%s", lot.price ? moneyL(*lot.price).c_str() : "No bids yet");
    }
    if (lot.leader) {
      ImGui::SameLine(0, 12);
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(vec(*lot.leader == me ? th.green : th.ink2), "%s", *lot.leader == me ? "You lead" : (teamNameOf(*lot.leader) + " lead").c_str());
    }
    if (p) hint("Most you can bid on him: " + moneyL(userCeiling(w, me, *p)) + (lot.userMax ? " · your limit " + moneyL(*lot.userMax) : ""));
    ImGui::Dummy(ImVec2(0, 6));
    if (waiting == std::optional<std::string>("bid")) {
      const double next = lot.price ? nextBid(*lot.price) : lot.base;
      if (button(("Bid " + moneyL(next)).c_str(), ButtonKind::Primary)) game.auctionBid();
      ImGui::SameLine();
      if (button("Pass")) game.auctionPass();
      ImGui::SameLine(0, 20);
      if (limit_ < next) limit_ = next;
      const double ceiling = p ? userCeiling(w, me, *p) : next;
      numberStepperD("##limit", &limit_, next, ceiling, nextBid(next) - next, moneyL(limit_));
      ImGui::SameLine();
      if (button("Bid up to this", ButtonKind::Default)) game.auctionLimit(limit_);
    } else if (waiting == std::optional<std::string>("rtm-invoke")) {
      notice("He played for you last season. Use a Right to Match card?", NoticeKind::Warn);
      if (button("Use RTM", ButtonKind::Primary)) game.auctionRtm("invoke", 1);
      ImGui::SameLine();
      if (button("Let him go")) game.auctionRtm("invoke", 0);
    } else if (waiting == std::optional<std::string>("rtm-raise")) {
      notice("His old team will use their RTM card. You get one last raise before they decide whether to match.", NoticeKind::Warn);
      const double price = lot.price.value_or(lot.base);
      if (raise_ < price) raise_ = price;
      numberStepperD("##raise", &raise_, price, p ? userCeiling(w, me, *p) : price, nextBid(price) - price, moneyL(raise_));
      ImGui::SameLine();
      if (button("Final offer", ButtonKind::Primary)) {
        game.auctionRtm("raise", raise_);
        raise_ = 0;
      }
    } else if (waiting == std::optional<std::string>("rtm-match")) {
      notice("Match " + moneyL(lot.price.value_or(0)) + " and he stays with you?", NoticeKind::Warn);
      if (button("Match it", ButtonKind::Primary)) game.auctionRtm("match", 1);
      ImGui::SameLine();
      if (button("Decline")) game.auctionRtm("match", 0);
    }
    ImGui::Dummy(ImVec2(0, 6));
    if (button("Auto-bid the rest", ButtonKind::Quiet)) game.auctionAutoRest();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Your scouts bid for you, up to what each player is worth to the squad.");
  } else {
    notice("Every lot has gone under the hammer.", NoticeKind::Ok);
    ImGui::Dummy(ImVec2(0, 4));
    if (button("Finish the off-season", ButtonKind::Primary)) game.finishOffseason();
    hint("Squads still short of " + std::to_string(SQUAD_MIN_AUCTION) + " sign unsold players at their base price.");
  }
  endPanel();

  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##sales", "Sold");
  int shown = 0;
  for (auto it = a.results.rbegin(); it != a.results.rend() && shown < 14; ++it, ++shown) {
    ImGui::PushID(it->id.c_str());
    teamInline(it->team, false, 16);
    ImGui::SameLine(0, 8);
    nameLink(ctx_, it->id);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(vec(it->team == me ? th.goldText : th.muted), "%s%s", moneyL(it->price).c_str(), it->rtm ? " (RTM)" : "");
    ImGui::PopID();
  }
  if (a.results.empty()) hint("Nobody sold yet.");
  if (!a.unsold.empty()) hint(std::to_string(a.unsold.size()) + " unsold so far.");
  endPanel();
}

// ---------------------------------------------------------------------------

class SackedScreen : public IScreenView {
 public:
  explicit SackedScreen(UiContext& ctx) : ctx_(ctx) {}
  void draw(float width) override {
    auto& game = ctx_.game;
    const Manager& m = game.state().world.manager;
    if (!m.sacked) {
      pageHeader("Still in a job", "", "The board");
      if (button("Home", ButtonKind::Primary)) ctx_.goTo(Screen::home);
      return;
    }
    const TeamMeta was = teamMetaOf(m.sacked->team);
    pageHeader("You've been sacked", "After season " + std::to_string(m.sacked->season) + ", " + was.name +
                   "'s board has lost confidence in you.", "The board", &was);
    ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##offers", "Offers on the table");
    hint("Other franchises want you. Pick one: you'll take them through the off-season and into next season.");
    ImGui::Dummy(ImVec2(0, 4));
    if (beginGrid("##offergrid", columnsFor(width - 36, 260, 3), 10)) {
      for (const auto& id : m.sacked->offers) {
        gridNext();
        const Team* t = ctx_.team(id);
        const int rank = strengthRank(ctx_.league(), id);
        std::string sub = ordinal(rank) + " strongest squad";
        (void)t;
        if (teamCard(teamMetaOf(id), sub, false, ImGui::GetContentRegionAvail().x)) game.acceptJob(id);
      }
      endGrid();
    }
    endPanel();
    if (!m.history.empty()) {
      ImGui::Dummy(ImVec2(0, 8));
      beginPanel("##history", "Your record with the boards");
      for (const auto& e : m.history) {
        if (e.event) {
          ImGui::BulletText("%s: %s to %s", e.event->c_str(), e.from.value_or("-").c_str(), e.to.value_or("-").c_str());
        } else {
          ImGui::BulletText("Season %d · %s · target %s · finished %s · %s", e.season.value_or(0), e.team.value_or("").c_str(),
                            e.target.value_or("").c_str(), e.position ? ordinal(*e.position).c_str() : "-", e.verdict.value_or("").c_str());
        }
      }
      endPanel();
    }
  }

 private:
  UiContext& ctx_;
};

}  // namespace

std::unique_ptr<IScreenView> makeOffseasonScreen(UiContext& ctx) { return std::make_unique<OffseasonScreen>(ctx); }
std::unique_ptr<IScreenView> makeSackedScreen(UiContext& ctx) { return std::make_unique<SackedScreen>(ctx); }

}  // namespace t20::ui
