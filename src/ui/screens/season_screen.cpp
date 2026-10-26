// Season: what's next (play or sim your fixture, sim to it), the board's target and confidence,
// and tabs for the points table, fixtures and results, player stats, team analytics, awards and
// the transfer room (trades in the window, cover for injured players).

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "engine/board.hpp"
#include "engine/fitness.hpp"
#include "engine/finance.hpp"
#include "engine/pitch.hpp"
#include "engine/season.hpp"
#include "engine/trades.hpp"
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
#include "ui/widgets/sortable_table.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

std::string stageText(const Fixture& f) {
  return f.stage == Stage::league ? "Match " + std::to_string(f.no) : std::string(stageLabel(f.stage));
}

TableCell numCell(double v, int dp = 0, CellTone tone = CellTone::Normal) {
  TableCell c;
  c.text = std::isnan(v) ? "–" : num(v, dp);
  c.sort = v;
  c.tone = tone;
  return c;
}
TableCell optCell(const std::optional<double>& v, int dp = 2) { return v ? numCell(*v, dp) : dashCell(); }

class SeasonScreen : public IScreenView {
 public:
  explicit SeasonScreen(UiContext& ctx) : ctx_(ctx) {}
  void onEnter() override { tradeMsg_.clear(); }
  void draw(float width) override;

 private:
  void drawNext(const Season& season);
  void drawBoard();
  void drawTable(const Season& season, float width);
  void drawFixtures(const Season& season);
  void drawStats(const Season& season);
  void drawTeams(const Season& season);
  void drawAwards(const Season& season);
  void drawTransfers(const Season& season, float width);
  void refreshStats(const Season& season);

  UiContext& ctx_;
  int tab_ = 0, statTab_ = 0;
  bool mineOnly_ = false;
  SortableTableState tableA_, tableB_, batState_, bowlState_, mvpState_, fieldState_, teamState_;
  uint64_t statsAt_ = UINT64_MAX;
  SeasonStats stats_;
  std::vector<TeamStat> teams_;
  SeasonAwards awards_;
  // transfer room
  TeamId tradeWith_;
  std::vector<PlayerId> give_, get_;
  double cash_ = 0;
  std::string tradeMsg_;
  bool tradeOk_ = false;
};

void SeasonScreen::refreshStats(const Season& season) {
  if (statsAt_ == ctx_.revision()) return;
  statsAt_ = ctx_.revision();
  const World& w = ctx_.game.state().world;
  const TeamOf teamOf = [&w](const PlayerId& id) { return teamOfId(w, id).value_or(id.substr(0, id.find('-'))); };
  stats_ = seasonStats(season, teamOf);
  teams_ = teamAnalytics(season);
  awards_ = seasonAwards(season, teamOf, [&w](const PlayerId& id) -> std::optional<int> {
    const Player* p = w.players.find(id);
    return p ? std::optional<int>(p->age) : std::nullopt;
  });
}

void SeasonScreen::draw(float width) {
  auto& game = ctx_.game;
  const auto& st = game.state();
  if (!st.season.season) {
    pageHeader("Season", "No season is running.", "Season");
    ImGui::Dummy(ImVec2(0, 8));
    if (st.world.offseason) {
      notice("The off-season is under way.", NoticeKind::Warn);
      if (button("Go to the off-season", ButtonKind::Primary)) ctx_.goTo(Screen::offseason);
    } else {
      emptyState("Start a season from the Home screen.");
      if (button("Home", ButtonKind::Primary)) ctx_.goTo(Screen::home);
    }
    return;
  }
  const Season& season = *st.season.season;
  refreshStats(season);
  const TeamMeta me = teamMetaOf(season.userTeam);
  std::string sub = me.name;
  if (st.world.manager.target) sub += " · Board target: " + st.world.manager.target->label;
  pageHeader("Season " + std::to_string(season.number), sub, "League and playoffs", &me);
  ImGui::Dummy(ImVec2(0, 8));

  const bool two = width >= 900;
  if (beginGrid("##seasontop", two ? 2 : 1, 18)) {
    gridNext();
    drawNext(season);
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    drawBoard();
    endGrid();
  }
  // a sim or a fixture may have changed the season
  if (!game.state().season.season) return;
  const Season& s = *game.state().season.season;
  ImGui::Dummy(ImVec2(0, 8));

  tab_ = tabs({{"table", "Table", ""}, {"fixtures", "Fixtures", ""}, {"player stats", "Player stats", ""}, {"team stats", "Team stats", ""},
               {"awards", "Awards", ""}, {"transfers", "Transfers", ""}},
              tab_);
  ImGui::Dummy(ImVec2(0, 8));
  switch (tab_) {
    case 0: drawTable(s, width); break;
    case 1: drawFixtures(s); break;
    case 2: drawStats(s); break;
    case 3: drawTeams(s); break;
    case 4: drawAwards(s); break;
    default: drawTransfers(s, width); break;
  }
}

void SeasonScreen::drawNext(const Season& season) {
  auto& game = ctx_.game;
  const auto& th = currentTheme();
  beginPanel("##next", "Next up");
  if (season.champion) {
    {
      FontScope fs(FontRole::H2);
      ImGui::TextColored(vec(th.goldText), "%s are champions", teamNameOf(*season.champion).c_str());
    }
    if (season.runnerUp) hint("Runners-up: " + teamNameOf(*season.runnerUp));
    ImGui::Dummy(ImVec2(0, 6));
    hint("The board reviews your season, then the off-season: ageing, contracts, the youth draft and the auction.");
    ImGui::Dummy(ImVec2(0, 4));
    if (button("End the season", ButtonKind::Primary)) game.nextSeason();
    endPanel();
    return;
  }
  const Fixture* next = nextFixture(season);
  const Fixture* mine = nextUserFixture(season);
  if (next && mine && next->no == mine->no) {
    const bool home = next->home == season.userTeam;
    const TeamId opp = home ? next->away : next->home;
    eyebrowText(stageText(*next) + (home ? " · home" : " · away"));
    teamInline(opp, false, 34);
    ImGui::SameLine(0, 10);
    ImGui::BeginGroup();
    {
      FontScope fs(FontRole::H2);
      ImGui::Text("v %s", teamNameOf(opp).c_str());
    }
    hint(next->venue + " · " + std::string(pitchDef(next->pitch).label));
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 6));
    if (button("Play it", ButtonKind::Primary)) game.playNextFixture();
    ImGui::SameLine();
    if (button("Quick sim", ButtonKind::Default)) {
      app::SimOptions o;
      o.includeUser = true;
      o.stopAfter = next->no;
      game.simFixtures(o);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("The vice-captain runs the match; you see the result.");
  } else if (next) {
    const auto status = userPlayoffStatus(season);
    if (mine) {
      hint("Next for you: " + stageText(*mine) + " v " + teamNameOf(mine->home == season.userTeam ? mine->away : mine->home) +
           ". " + std::to_string(mine->no - next->no) + " other game(s) first.");
    } else if (status == std::optional<std::string>("eliminated")) {
      hint("You're out. The rest of the playoffs go on without you.");
    } else if (status == std::optional<std::string>("waiting")) {
      hint("You're through. Waiting for the other playoff results.");
    } else if (leagueComplete(season)) {
      hint("You missed the playoffs. The top four play on.");
    } else {
      hint("Other games are being played.");
    }
    ImGui::Dummy(ImVec2(0, 6));
    if (button(mine ? "Sim to your next game" : "Sim the rest", ButtonKind::Primary)) {
      app::SimOptions o;
      o.includeUser = !mine;
      game.simFixtures(o);
    }
  }
  endPanel();
}

void SeasonScreen::drawBoard() {
  const auto& m = ctx_.game.state().world.manager;
  beginPanel("##board", "The board");
  if (m.target) keyValue("Target", m.target->label);
  keyValue("Confidence", std::to_string(static_cast<int>(std::lround(m.confidence))) + " · " +
                             std::string(boardConfidenceLabel(m.confidence)));
  meter(static_cast<float>(m.confidence));
  hint("Wins and losses move it a little; the season review moves it a lot. At " + std::to_string(static_cast<int>(SACK_AT)) +
       " or below you're sacked.");
  endPanel();
}

void SeasonScreen::drawTable(const Season& season, float width) {
  const auto table = pointsTable(season);
  const std::vector<TableColumn> cols = {
      textColumn("Team", 2.4f, true),
      numColumn("P", "Played"),
      numColumn("W", "Won"),
      numColumn("L", "Lost"),
      numColumn("NR", "No result"),
      numColumn("Pts", "Points"),
      numColumn("NRR", "Net run rate"),
      textColumn("Form", 1.4f, false),
  };
  auto rowsFor = [&](const std::string& group) {
    std::vector<TableRowData> rows;
    for (const auto& r : table) {
      if (!group.empty() && r.group != group) continue;
      TableRowData row;
      row.mine = r.team == season.userTeam;
      TableCell t = textCell(teamNameOf(r.team), row.mine ? CellTone::Gold : CellTone::Normal);
      t.badgeTeam = r.team;
      t.sort = r.pos;
      row.cells = {t, numCell(r.p), numCell(r.w), numCell(r.l), numCell(r.nr), numCell(r.pts, 0, CellTone::Strong),
                   numCell(r.nrr, 3)};
      row.cells[6].text = nrrText(r.nrr);
      std::string form;
      for (const auto& f : r.form) form += f;
      row.cells.push_back(textCell(form, CellTone::Muted));
      rows.push_back(row);
    }
    return rows;
  };
  auto init = [](SortableTableState& s) {
    if (!s.initialised) {
      s.sortColumnIdx = 0;
      s.sortAscending = true;
      s.initialised = true;
    }
  };
  init(tableA_);
  init(tableB_);
  const bool two = width >= 1000;
  if (beginGrid("##tables", two ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##groupA", "Group A");
    sortableTable("##ta", cols, rowsFor("A"), tableA_, 0, false);
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));
    beginPanel("##groupB", "Group B");
    sortableTable("##tb", cols, rowsFor("B"), tableB_, 0, false);
    endPanel();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 4));
  hint("Teams play their own group once and the other group twice. Two points a win, one for no result. The top "
       "four overall go to the playoffs: Qualifier 1 (1st v 2nd), the Eliminator (3rd v 4th), Qualifier 2, the Final.");
}

void SeasonScreen::drawFixtures(const Season& season) {
  const auto& th = currentTheme();
  beginPanel("##fixtures", "Fixtures and results");
  if (chip("All##fx", !mineOnly_)) mineOnly_ = false;
  ImGui::SameLine(0, 6);
  if (chip("Mine##fx", mineOnly_)) mineOnly_ = true;
  ImGui::Dummy(ImVec2(0, 4));
  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, vec(ColorRGBA(th.surface2.r, th.surface2.g, th.surface2.b, 0.55f)));
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
  if (ImGui::BeginTable("##fxt", 4, flags)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.4f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 4.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    const Fixture* next = nextFixture(season);
    for (const auto& f : season.fixtures) {
      const bool mine = f.home == season.userTeam || f.away == season.userTeam;
      if (mineOnly_ && !mine) continue;
      ImGui::TableNextRow(0, 28);
      if (mine) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, col(th.goldSoft));
      ImGui::TableNextColumn();
      ImGui::TextColored(vec(th.muted), "%s", stageText(f).c_str());
      ImGui::TableNextColumn();
      ImGui::PushID(f.no);
      teamInline(f.home, true, 16);
      ImGui::SameLine(0, 6);
      ImGui::TextColored(vec(th.muted), "v");
      ImGui::SameLine(0, 6);
      teamInline(f.away, true, 16);
      ImGui::PopID();
      ImGui::TableNextColumn();
      if (f.result) {
        ImGui::TextUnformatted(f.result->text.c_str());
      } else if (next && next->no == f.no) {
        ImGui::TextColored(vec(th.goldText), "Next up");
      } else {
        ImGui::TextColored(vec(th.muted), "%s", f.venue.c_str());
      }
      ImGui::TableNextColumn();
      if (f.result && f.result->potm) {
        ImGui::TextColored(vec(th.muted), "POTM ");
        ImGui::SameLine(0, 2);
        nameLink(ctx_, *f.result->potm);
      } else {
        ImGui::TextColored(vec(th.muted), "%s", std::string(pitchDef(f.pitch).label).c_str());
      }
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleColor(2);
  endPanel();
}

void SeasonScreen::drawStats(const Season& season) {
  (void)season;
  beginPanel("##stats", "Player stats");
  statTab_ = tabs({{"batting", "Batting", ""}, {"bowling", "Bowling", ""}, {"mvp", "MVP", ""}, {"fielding", "Fielding", ""}}, statTab_, true);
  ImGui::Dummy(ImVec2(0, 6));
  const TeamId me = ctx_.userTeam();
  auto player = [&](const PlayerId& id, const TeamId& team) {
    TableCell c;
    c.text = ctx_.playerName(id);
    c.link = id;
    (void)team;
    return c;
  };
  auto teamCell = [&](const TeamId& t) {
    TableCell c = textCell(t, CellTone::Muted);
    c.badgeTeam = t;
    return c;
  };
  TableClick click;
  if (statTab_ == 0) {
    if (!batState_.initialised) {
      batState_ = {2, false, true, true};
    }
    const std::vector<TableColumn> cols = {
        textColumn("Player", 2.2f), textColumn("Team", 1.0f),
        numColumn("Runs"), numColumn("Inns"), numColumn("HS", "Highest score"),
        numColumn("Avg", "Average", false, "Average: min 3 innings"), numColumn("SR", "Strike rate", false, "Strike rate: min 30 balls"),
        numColumn("50s"), numColumn("4s"), numColumn("6s")};
    std::vector<TableRowData> rows;
    for (const auto& b : stats_.batting) {
      TableRowData r;
      r.mine = b.team == me;
      TableCell hs = numCell(b.hs);
      hs.text = b.hsText;
      r.cells = {player(b.id, b.team), teamCell(b.team), numCell(b.runs, 0, CellTone::Strong), numCell(b.inns), hs,
                 b.inns >= 3 ? optCell(b.avg) : dashCell(), b.balls >= 30 ? optCell(b.sr) : dashCell(),
                 numCell(b.fifties + b.hundreds), numCell(b.fours), numCell(b.sixes)};
      rows.push_back(r);
    }
    click = sortableTable("##bat", cols, rows, batState_, 15);
  } else if (statTab_ == 1) {
    if (!bowlState_.initialised) bowlState_ = {2, false, true, true};
    const std::vector<TableColumn> cols = {
        textColumn("Player", 2.2f), textColumn("Team", 1.0f),
        numColumn("Wkts", "Wickets"), numColumn("Overs"), numColumn("Best"),
        numColumn("Econ", "Economy", true, "Economy: min 10 overs"), numColumn("Avg", "Average", true, "Average: min 5 wickets"),
        numColumn("SR", "Strike rate", true), numColumn("Dot %")};
    std::vector<TableRowData> rows;
    for (const auto& b : stats_.bowling) {
      TableRowData r;
      r.mine = b.team == me;
      TableCell ov = numCell(b.balls);
      ov.text = b.oversText;
      TableCell best = numCell(b.bestW * 1000 - b.bestR);
      best.text = b.bestText;
      r.cells = {player(b.id, b.team), teamCell(b.team), numCell(b.wkts, 0, CellTone::Strong), ov, best,
                 b.balls >= 60 ? optCell(b.econ) : dashCell(), b.wkts >= 5 ? optCell(b.avg) : dashCell(),
                 b.wkts >= 5 ? optCell(b.sr, 1) : dashCell(), b.dotPct ? numCell(*b.dotPct, 0) : dashCell()};
      rows.push_back(r);
    }
    click = sortableTable("##bowl", cols, rows, bowlState_, 15);
  } else if (statTab_ == 2) {
    if (!mvpState_.initialised) mvpState_ = {2, false, true, true};
    const std::vector<TableColumn> cols = {textColumn("Player", 2.2f),
                                           textColumn("Team", 1.0f),
                                           numColumn("Points", "MVP points"),
                                           numColumn("M", "Matches"),
                                           numColumn("Wkts"),
                                           numColumn("4s"),
                                           numColumn("6s"),
                                           numColumn("Catches"),
                                           numColumn("Dots")};
    std::vector<TableRowData> rows;
    for (const auto& b : stats_.mvp) {
      TableRowData r;
      r.mine = b.team == me;
      r.cells = {player(b.id, b.team), teamCell(b.team), numCell(b.pts, 1, CellTone::Strong), numCell(b.m), numCell(b.wkts),
                 numCell(b.fours), numCell(b.sixes), numCell(b.catches), numCell(b.dots)};
      rows.push_back(r);
    }
    click = sortableTable("##mvp", cols, rows, mvpState_, 15);
    hint("Points: 3.5 a wicket, 1 a dot ball, 2.5 a four, 3.5 a six, 2.5 a catch.");
  } else {
    if (!fieldState_.initialised) fieldState_ = {2, false, true, true};
    const std::vector<TableColumn> cols = {textColumn("Player", 2.2f), textColumn("Team", 1.0f),
                                           numColumn("Dismissals"), numColumn("Catches"),
                                           numColumn("Stumpings"), numColumn("Run outs"),
                                           numColumn("Drops", "", true)};
    std::vector<TableRowData> rows;
    for (const auto& b : stats_.fielding) {
      TableRowData r;
      r.mine = b.team == me;
      r.cells = {player(b.id, b.team), teamCell(b.team), numCell(b.dismissals, 0, CellTone::Strong), numCell(b.ct), numCell(b.st),
                 numCell(b.ro), numCell(b.drops)};
      rows.push_back(r);
    }
    click = sortableTable("##field", cols, rows, fieldState_, 15);
  }
  if (!click.link.empty()) ctx_.openCard(click.link);
  if (stats_.matches == 0) emptyState("No games played yet.");
  endPanel();
}

void SeasonScreen::drawTeams(const Season& season) {
  beginPanel("##teamstats", "Team stats");
  if (!teamState_.initialised) teamState_ = {3, false, false, true};
  const std::vector<TableColumn> cols = {textColumn("Team", 2.2f),
                                         numColumn("P"),
                                         numColumn("W"),
                                         numColumn("Win %"),
                                         numColumn("Avg 1st inns", "Average first-innings score"),
                                         numColumn("Run rate"),
                                         numColumn("Economy", "", true),
                                         numColumn("PP rate", "Powerplay run rate"),
                                         numColumn("Death rate", "Run rate at the death"),
                                         numColumn("6s"),
                                         numColumn("Highest")};
  std::vector<TableRowData> rows;
  for (const auto& t : teams_) {
    TableRowData r;
    r.mine = t.team == season.userTeam;
    TableCell name = textCell(teamNameOf(t.team), r.mine ? CellTone::Gold : CellTone::Normal);
    name.badgeTeam = t.team;
    TableCell high = t.highest ? numCell(t.highest->runs) : dashCell();
    if (t.highest) high.text = std::to_string(t.highest->runs) + "/" + std::to_string(t.highest->wickets);
    r.cells = {name, numCell(t.p), numCell(t.w), t.winPct ? numCell(*t.winPct, 0) : dashCell(), optCell(t.avgFirst, 0),
               optCell(t.rr), optCell(t.econ), optCell(t.ppRR), optCell(t.deathRR), numCell(t.sixes), high};
    rows.push_back(r);
  }
  sortableTable("##teams", cols, rows, teamState_, 0, false);
  endPanel();
}

void SeasonScreen::drawAwards(const Season& season) {
  const SeasonAwards& aw = awards_;
  beginPanel("##awards", season.champion ? "Season awards" : "Award races");
  if (!season.champion) hint("Who leads each race right now. The awards are decided when the final is played.");
  ImGui::Dummy(ImVec2(0, 4));
  if (beginGrid("##awgrid", columnsFor(ImGui::GetContentRegionAvail().x, 260, 3), 12)) {
    auto card = [&](const char* title, const std::optional<PlayerId>& id, const std::string& line) {
      gridNext();
      eyebrowText(title);
      if (!id) {
        ImGui::TextColored(vec(currentTheme().muted), "–");
        return;
      }
      ImGui::PushID(title);
      nameLink(ctx_, *id);
      ImGui::PopID();
      hint(line);
    };
    card("Orange Cap", aw.orangeCap ? std::optional<PlayerId>(aw.orangeCap->id) : std::nullopt,
         aw.orangeCap ? std::to_string(aw.orangeCap->runs) + " runs" : "");
    card("Purple Cap", aw.purpleCap ? std::optional<PlayerId>(aw.purpleCap->id) : std::nullopt,
         aw.purpleCap ? std::to_string(aw.purpleCap->wkts) + " wickets" : "");
    card("Most valuable player", aw.mvp ? std::optional<PlayerId>(aw.mvp->id) : std::nullopt,
         aw.mvp ? num(aw.mvp->pts, 1) + " points" : "");
    card("Emerging player", aw.emerging ? std::optional<PlayerId>(aw.emerging->id) : std::nullopt,
         aw.emerging ? num(aw.emerging->pts, 1) + " points" : "");
    card("Most sixes", aw.mostSixes ? std::optional<PlayerId>(aw.mostSixes->id) : std::nullopt,
         aw.mostSixes ? std::to_string(aw.mostSixes->sixes) + " sixes" : "");
    card("Most fours", aw.mostFours ? std::optional<PlayerId>(aw.mostFours->id) : std::nullopt,
         aw.mostFours ? std::to_string(aw.mostFours->fours) + " fours" : "");
    card("Best strike rate", aw.bestStrikeRate ? std::optional<PlayerId>(aw.bestStrikeRate->id) : std::nullopt,
         aw.bestStrikeRate ? num(aw.bestStrikeRate->sr.value_or(0), 1) : "");
    card("Best economy", aw.bestEconomy ? std::optional<PlayerId>(aw.bestEconomy->id) : std::nullopt,
         aw.bestEconomy ? num(aw.bestEconomy->econ.value_or(0), 2) : "");
    card("Most dot balls", aw.mostDots ? std::optional<PlayerId>(aw.mostDots->id) : std::nullopt,
         aw.mostDots ? std::to_string(aw.mostDots->dots) + " dots" : "");
    card("Safest hands", aw.safestHands ? std::optional<PlayerId>(aw.safestHands->id) : std::nullopt,
         aw.safestHands ? std::to_string(aw.safestHands->ct) + " catches" : "");
    endGrid();
  }
  endPanel();
}

void SeasonScreen::drawTransfers(const Season& season, float width) {
  auto& game = ctx_.game;
  const World& w = game.state().world;
  const auto& th = currentTheme();
  const TeamId me = season.userTeam;
  const TradeWindow win = tradeWindow(&season);
  const bool two = width >= 1000;

  if (beginGrid("##transfers", two ? 2 : 1, 18)) {
    gridNext();
    // ---- Trades
    beginPanel("##trades", "Trade window");
    if (win.state == "before") hint("Opens after " + std::to_string(TRADE_OPEN) + " league games (" + std::to_string(win.games) + " to go).");
    else if (win.state == "closed") hint("The window has shut for this season.");
    else
      hint("Open for " + std::to_string(win.games) + " more league games. Trades this season: " + std::to_string(tradesMade(w, me)) + "/" +
           std::to_string(MAX_TRADES) + ".");
    if (win.open) {
      ImGui::Dummy(ImVec2(0, 4));
      eyebrowText("Trade with");
      int i = 0;
      for (const auto& t : ctx_.league()) {
        if (t.id == me) continue;
        if (i++ % 5 != 0) ImGui::SameLine(0, 6);
        if (chip((t.id + "##tw").c_str(), tradeWith_ == t.id)) {
          tradeWith_ = t.id;
          get_.clear();
          tradeMsg_.clear();
        }
      }
      auto pickList = [&](const char* id, const TeamId& team, std::vector<PlayerId>& picked) {
        const WorldTeam* wt = w.teams.find(team);
        if (!wt) return;
        ImGui::PushID(id);
        ImGui::BeginChild("##list", ImVec2(0, 220), ImGuiChildFlags_Borders);
        for (const auto& pid : wt->squad) {
          const Player* p = ctx_.player(pid);
          if (!p) continue;
          bool on = std::find(picked.begin(), picked.end(), pid) != picked.end();
          const bool can = on || picked.size() < MAX_PER_SIDE;
          ImGui::PushID(pid.c_str());
          if (checkbox("##pick", &on, can)) {
            if (on) picked.push_back(pid);
            else picked.erase(std::find(picked.begin(), picked.end(), pid));
            tradeMsg_.clear();
          }
          ImGui::SameLine(0, 8);
          nameLink(ctx_, *p);
          ImGui::SameLine(0, 8);
          ImGui::TextColored(vec(th.muted), "%s · %d · %s", roleText(p->role).c_str(), headlineRating(*p),
                             money(p->contract ? std::optional<double>(p->contract->cost) : std::nullopt).c_str());
          ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::PopID();
      };
      if (!tradeWith_.empty()) {
        ImGui::Dummy(ImVec2(0, 6));
        eyebrowText("You give (up to " + std::to_string(MAX_PER_SIDE) + ")");
        pickList("##give", me, give_);
        eyebrowText("You get (up to " + std::to_string(MAX_PER_SIDE) + ")");
        pickList("##get", tradeWith_, get_);
        eyebrowText("Cash (positive: you pay)");
        numberStepperD("##cash", &cash_, -MAX_CASH_BACK, purseLeft(w, me), CASH_STEP, money(std::abs(cash_)) + (cash_ < 0 ? " to you" : cash_ > 0 ? " from you" : ""));
        Offer offer{give_, get_, cash_};
        const TradeVerdict v = evaluateTrade(w, me, tradeWith_, offer);
        ImGui::Dummy(ImVec2(0, 4));
        if (!v.errors.empty()) {
          for (const auto& e : v.errors) ImGui::TextColored(vec(th.red), "%s", e.c_str());
        } else if (!give_.empty() || !get_.empty()) {
          if (v.accept) ImGui::TextColored(vec(th.green), "They'd take that.");
          else ImGui::TextColored(vec(th.orange), "Not enough for them: about %s short.", money(v.shortBy).c_str());
        }
        ImGui::Dummy(ImVec2(0, 4));
        if (button("Propose the trade", ButtonKind::Primary, v.errors.empty() && (!give_.empty() || !get_.empty()))) {
          const TradeVerdict done = game.proposeTrade(tradeWith_, offer);
          tradeOk_ = done.done;
          tradeMsg_ = done.done ? "Done. The players have moved." : !done.errors.empty() ? done.errors.front() : "They turned it down.";
          if (done.done) {
            give_.clear();
            get_.clear();
            cash_ = 0;
          }
        }
        if (!tradeMsg_.empty()) notice(tradeMsg_, tradeOk_ ? NoticeKind::Ok : NoticeKind::Warn);
      }
    }
    endPanel();
    if (two) gridNext();
    else ImGui::Dummy(ImVec2(0, 8));

    // ---- Injury cover
    beginPanel("##cover", "Injury cover");
    hint("A player out for " + std::to_string(REPLACE_MIN_MATCHES) + "+ matches can be covered by a free agent for the rest of the season.");
    const auto injured = replaceable(w, me);
    if (injured.empty()) emptyState("Nobody needs cover.");
    for (const auto& id : injured) {
      const Player* p = ctx_.player(id);
      if (!p) continue;
      ImGui::PushID(id.c_str());
      ImGui::Dummy(ImVec2(0, 4));
      nameLink(ctx_, *p);
      playerTags(*p);
      for (const auto& o : replacementOptions(w, me, id, 5)) {
        const Player* q = ctx_.player(o.id);
        if (!q) continue;
        ImGui::PushID(o.id.c_str());
        ImGui::Indent(14);
        nameLink(ctx_, *q);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(vec(th.muted), "%s · %d · fee %s", roleText(q->role).c_str(), headlineRating(*q), money(o.fee).c_str());
        ImGui::SameLine(0, 8);
        if (o.block) ImGui::TextColored(vec(th.red), "%s", o.block->c_str());
        else if (button("Sign", ButtonKind::Small)) game.signCover(id, o.id);
        ImGui::Unindent(14);
        ImGui::PopID();
      }
      ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(0, 8));
    sectionHeading("Transfer news");
    int shown = 0;
    for (const auto& t : w.transfers) {
      if (shown++ >= 10) break;
      ImGui::BulletText("%s", t.text.c_str());
    }
    if (w.transfers.empty()) hint("No moves yet.");
    endPanel();
    endGrid();
  }
}

}  // namespace

std::unique_ptr<IScreenView> makeSeasonScreen(UiContext& ctx) { return std::make_unique<SeasonScreen>(ctx); }

}  // namespace t20::ui
