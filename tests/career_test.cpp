// Ports of development.test.js, career.test.js, season.test.js (engine parts), the editor and
// create-player engine tests, and milestone and achievement checks.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "core/str.hpp"

#include "engine/achievements.hpp"
#include "engine/ageing.hpp"
#include "engine/career.hpp"
#include "engine/editor.hpp"
#include "engine/finance.hpp"
#include "engine/league.hpp"
#include "engine/league_view.hpp"
#include "engine/milestones.hpp"
#include "engine/season.hpp"
#include "engine/sides.hpp"
#include "engine/skills.hpp"
#include "engine/world.hpp"
#include "parity/parity.hpp"

using namespace t20;

namespace {
const Player& rosterPlayer(const char* id) { return *playerById(id); }
const BaseOf kBaseOf = [](const PlayerId& id) -> const Skills* {
  const Player* p = playerById(id);
  return p ? &p->skills : nullptr;
};

// A fake completed match where one batter makes `runs` off `balls` batting at No. 1
Match fakeMatch(const PlayerId& batterId, int runs, int balls, const std::string& id) {
  Match m;
  m.id = id;
  m.status = MatchStatus::complete;
  m.result = Result{};
  m.result->winner = 0;
  const auto& a = LEAGUE()[0].squad;
  const auto& b = LEAGUE()[1].squad;
  m.teams[0].id = "A";
  m.teams[1].id = "B";
  for (size_t i = 0; i < 11; ++i) {
    m.teams[0].players.push_back(a[i].id);
    m.teams[1].players.push_back(b[i].id);
    m.players.set(a[i].id, a[i]);
    m.players.set(b[i].id, b[i]);
  }
  m.teams[0].keeperId = a[0].id;
  m.teams[1].keeperId = b[0].id;
  Innings inn;
  inn.batting = 0;
  inn.bowlingSide = 1;
  for (const auto& pid : m.teams[0].players) inn.batters.set(pid, BatterInnings{});
  BatterInnings bi;
  bi.runs = runs, bi.balls = balls, bi.fours = runs / 10, bi.sixes = runs / 25, bi.out = true;
  inn.batters.set(batterId, bi);
  inn.battedOrder = {batterId};
  m.innings.push_back(inn);
  return m;
}

const Player& goodBatter() {
  for (const auto& p : LEAGUE()[0].squad)
    if (p.role == Role::BAT && p.bat >= 75) return p;
  return LEAGUE()[0].squad[0];
}

}  // namespace

// ---------------------------------------------------------------------------
// development.test.js
// ---------------------------------------------------------------------------

TEST_CASE("training: boosts only the drilled skills, on that copy only", "[development]") {
  const Player* p = nullptr;
  for (const auto& x : LEAGUE()[0].squad)
    if (x.role == Role::BAT) {
      p = &x;
      break;
    }
  const Player t = applyTraining(*p, "range");
  CHECK(t.skills.bat[BatK::lofted] == std::min(99.0, p->skills.bat[BatK::lofted] + 7));
  CHECK(t.skills.bat[BatK::defence] == p->skills.bat[BatK::defence]);
  CHECK(t.bat >= p->bat);
  CHECK(playerById(p->id)->skills.bat[BatK::lofted] == p->skills.bat[BatK::lofted]);
  CHECK(t.training->drill == "range");
  CHECK(*t.startConf == 49);  // 45 + 4
}

TEST_CASE("training: the AI coach trains up to 3 different players", "[development]") {
  const Team& team = LEAGUE()[2];
  std::vector<const Player*> xi;
  for (const auto& id : pickBestXI(team, Pitch::dusty).ids)
    for (const auto& p : team.squad)
      if (p.id == id) xi.push_back(&p);
  const auto plan = aiTrainingPlan(xi, Pitch::dusty);
  CHECK_FALSE(plan.empty());
  CHECK(plan.size() <= 3);
  std::set<PlayerId> ids;
  for (const auto& s : plan) ids.insert(s.playerId);
  CHECK(ids.size() == plan.size());
}

TEST_CASE("development: ten strong innings add 3-6%; ten failures take it away", "[development]") {
  const Player& batter = goodBatter();
  OrderedMap<PlayerId, Progress> prog;
  for (int i = 0; i < 10; ++i) developFromMatch(prog, fakeMatch(batter.id, 62, 38, cat("g", i)), kBaseOf);
  const Player up = applyProgress(batter, prog.find(batter.id));
  const double base = batter.skills.bat[BatK::defence];
  const double pct = (up.skills.bat[BatK::defence] - base) / base;
  CHECK(pct >= 0.03);
  CHECK(pct <= 0.07);
  CHECK(up.bat > batter.bat);
  for (int i = 0; i < 10; ++i) developFromMatch(prog, fakeMatch(batter.id, 3, 9, cat("b", i)), kBaseOf);
  const Player back = applyProgress(batter, prog.find(batter.id));
  CHECK(std::abs(back.skills.bat[BatK::defence] - base) <= 1);
}

TEST_CASE("development: never drifts more than the cap", "[development]") {
  const Player& batter = goodBatter();
  OrderedMap<PlayerId, Progress> prog;
  for (int i = 0; i < 60; ++i) developFromMatch(prog, fakeMatch(batter.id, 110, 55, cat("x", i)), kBaseOf);
  const double cap = std::max(3.0, batter.skills.bat[BatK::defence] * CAP);
  CHECK(prog.find(batter.id)->bat[BatK::defence] <= cap + 1e-9);
}

TEST_CASE("development: an innings is judged against what's normal for the position", "[development]") {
  BatterInnings seven;
  seven.runs = 30, seven.balls = 20, seven.out = true;
  CHECK(*battingPerformance(&seven, 6, 65) > 0.3);
  BatterInnings opener;
  opener.runs = 30, opener.balls = 28, opener.out = true;
  CHECK(*battingPerformance(&opener, 0, 82) < 0.2);
  CHECK_FALSE(battingPerformance(nullptr, 0, 70).has_value());
}

TEST_CASE("development: recorded after real simulated matches, within the cap", "[development]") {
  Career career;
  for (int s = 1; s <= 20; ++s) {
    MatchSetup setup;
    setup.seed = s;
    setup.userTeam = std::nullopt;
    setup.teams = {parity::side(LEAGUE()[0], Pitch::balanced), parity::side(LEAGUE()[3], Pitch::balanced)};
    Match m = createMatch(setup);
    simMatch(m);
    recordMatch(career, m, &kBaseOf);
  }
  CHECK(career.progress.size() > 15);
  CHECK(career.lastReport->reports.size() > 5);
  for (const auto& [id, pr] : career.progress)
    for (size_t i = 0; i < SkillSet<BatK>::N; ++i) {
      const auto k = static_cast<BatK>(i);
      if (pr.bat.contains(k)) CHECK(std::abs(pr.bat[k]) <= std::max(3.0, playerById(id)->skills.bat[k] * CAP) + 1e-9);
    }
}

// ---------------------------------------------------------------------------
// career.test.js
// ---------------------------------------------------------------------------

TEST_CASE("career: stats reconcile with every recorded scorecard", "[career]") {
  Career career;
  int runs = 0, extras = 0, wickets = 0, bowlerWkts = 0, catches = 0, stumpings = 0, runOuts = 0, legal = 0;
  std::map<PlayerId, std::vector<std::pair<int, int>>> figs;
  for (int seed = 1; seed <= 60; ++seed) {
    MatchSetup setup;
    setup.seed = seed;
    setup.userTeam = std::nullopt;
    setup.teams = {parity::side(LEAGUE()[static_cast<size_t>(seed % 8)], Pitch::balanced, false),
                   parity::side(LEAGUE()[static_cast<size_t>((seed + 1) % 8)], Pitch::balanced, false)};
    Match m = createMatch(setup);
    simMatch(m);
    CHECK(recordMatch(career, m));
    CHECK_FALSE(recordMatch(career, m));  // never double counted
    for (const auto& inn : m.innings) {
      if (inn.superOver) continue;
      runs += inn.runs;
      extras += inn.extras.wd + inn.extras.nb + inn.extras.lb + inn.extras.b;
      legal += inn.legalBalls;
      for (const auto& [id, f] : inn.bowling) figs[id].push_back({f.wickets, f.runs});
      for (const auto& [id, b] : inn.batters) {
        if (!b.out) continue;
        ++wickets;
        if (b.dismissal->bowler) ++bowlerWkts;
        if (b.dismissal->type == Out::stumped)
          ++stumpings;
        else if (b.dismissal->type == Out::runout)
          ++runOuts;
        else if (b.dismissal->fielder)
          ++catches;
      }
    }
  }
  auto sum = [&](auto f) {
    long s = 0;
    for (const auto& [id, p] : career.players) s += f(p);
    return s;
  };
  CHECK(sum([](const CareerStats& p) { return p.bat.runs; }) == runs - extras);
  CHECK(sum([](const CareerStats& p) { return p.bat.inns - p.bat.notOuts; }) == wickets);
  CHECK(sum([](const CareerStats& p) { return p.bowl.wickets; }) == bowlerWkts);
  CHECK(sum([](const CareerStats& p) { return p.bowl.balls; }) == legal);
  CHECK(sum([](const CareerStats& p) { return p.field.catches; }) == catches);
  CHECK(sum([](const CareerStats& p) { return p.field.stumpings; }) == stumpings);
  CHECK(sum([](const CareerStats& p) { return p.field.runOuts; }) == runOuts);
  CHECK(sum([](const CareerStats& p) { return p.matches; }) == 60 * 22);
  CHECK(sum([](const CareerStats& p) { return p.potm; }) == 60);
  for (const auto& [id, p] : career.players) {
    CHECK(p.recent.size() <= 5);
    CHECK(p.bat.hs <= p.bat.runs);
    if (p.bowl.inns) CHECK(p.bowl.bestW <= p.bowl.wickets);
  }
  for (auto& [id, list] : figs) {
    std::stable_sort(list.begin(), list.end(), [](auto x, auto y) { return x.first != y.first ? x.first > y.first : x.second < y.second; });
    CHECK(career.players.at(id).bowl.bestW == list[0].first);
    CHECK(career.players.at(id).bowl.bestR == list[0].second);
  }
  // Records book and popularity filled in along the way
  CHECK(career.records.highTotals.size() == 10);
  CHECK_FALSE(career.popularity.empty());
  CHECK_FALSE(career.matchups.empty());
}

TEST_CASE("career: form from the last three innings", "[career]") {
  CareerStats s;
  CHECK(formOf(&s).label == "No recent innings");
  s.recent = {{"X", "80 (40)", std::nullopt, "W"}, {"X", std::nullopt, "2/20", "W"}, {"X", "70* (35)", std::nullopt, "L"},
              {"X", "60 (30)", std::nullopt, "W"}, {"X", "0 (1)", std::nullopt, "L"}};
  const Form f = formOf(&s);
  CHECK(f.scores == std::vector<int>{80, 70, 60});
  CHECK(f.bonus == 12);
  CHECK(f.label == "Red hot");
}

// ---------------------------------------------------------------------------
// season.test.js (engine parts)
// ---------------------------------------------------------------------------

TEST_CASE("season: two groups of five and 70 league games, 14 per team", "[season]") {
  const Season s = createSeason(11, 1, "CHK", LEAGUE());
  CHECK(s.groupA.size() == 5);
  CHECK(s.groupB.size() == 5);
  CHECK(s.fixtures.size() == LEAGUE_MATCHES);
  for (const auto& t : LEAGUE()) {
    int games = 0;
    for (const auto& f : s.fixtures)
      if (f.home == t.id || f.away == t.id) ++games;
    CHECK(games == 14);
    const auto& mine = std::find(s.groupA.begin(), s.groupA.end(), t.id) != s.groupA.end() ? s.groupA : s.groupB;
    for (const auto& o : LEAGUE()) {
      if (o.id == t.id) continue;
      int n = 0, home = 0;
      for (const auto& f : s.fixtures) {
        if ((f.home == t.id && f.away == o.id) || (f.home == o.id && f.away == t.id)) ++n;
        if (f.home == t.id && f.away == o.id) ++home;
      }
      const bool same = std::find(mine.begin(), mine.end(), o.id) != mine.end();
      CHECK(n == (same ? 1 : 2));
      if (!same) CHECK(home == 1);
    }
  }
}

TEST_CASE("season: a full season through the playoffs to a champion", "[season]") {
  Season s = createSeason(5, 1, "CHK", LEAGUE());
  while (const Fixture* fx = nextFixture(s)) {
    const Fixture f = *fx;
    recordResult(s, f.no, summarizeMatch(simFixture(f, LEAGUE(), nullptr)));
  }
  CHECK(leagueComplete(s));
  std::vector<Stage> stages;
  for (const auto& f : s.fixtures)
    if (f.stage != Stage::league) stages.push_back(f.stage);
  CHECK(stages == std::vector<Stage>{Stage::q1, Stage::elim, Stage::q2, Stage::final_});
  const auto table = pointsTable(s);
  const Fixture &q1 = s.fixtures[70], &elim = s.fixtures[71], &q2 = s.fixtures[72], &fin = s.fixtures[73];
  CHECK(q1.home == table[0].team);
  CHECK(q1.away == table[1].team);
  CHECK(elim.home == table[2].team);
  CHECK(elim.away == table[3].team);
  const TeamId q1Loser = q1.result->winner == q1.home ? q1.away : q1.home;
  CHECK((q2.home == q1Loser || q2.away == q1Loser));
  CHECK((q2.home == elim.result->winner || q2.away == elim.result->winner));
  CHECK(fin.home == *q1.result->winner);
  CHECK(fin.away == *q2.result->winner);
  CHECK(s.champion == fin.result->winner);
  int pts = 0, wins = 0;
  for (const auto& r : table) pts += r.pts, wins += r.w;
  CHECK(pts == 140);
  CHECK(wins == 70);
  auto teamOf = [](const PlayerId& id) { return id.substr(0, id.find('-')); };
  const SeasonStats st = seasonStats(s, teamOf);
  long fromStats = 0, fromGames = 0;
  for (const auto& b : st.batting) fromStats += b.runs;
  for (const auto& f : s.fixtures)
    for (const auto& i : f.result->innings) fromGames += i.runs - i.extras;
  CHECK(fromStats == fromGames);
  CHECK(teamAnalytics(s).size() == 10);
  CHECK(positionHistory(s).size() == 70);
  CHECK(seasonRecords(s, teamOf).highestTotals.size() == 5);
  const auto aw = seasonAwards(s, teamOf, [](const PlayerId& id) -> std::optional<int> { return playerById(id)->age; });
  int maxRuns = 0, maxWkts = 0;
  for (const auto& b : st.batting) maxRuns = std::max(maxRuns, b.runs);
  for (const auto& b : st.bowling) maxWkts = std::max(maxWkts, b.wkts);
  CHECK(aw.orangeCap->runs == maxRuns);
  CHECK(aw.purpleCap->wkts == maxWkts);
}

TEST_CASE("season: NRR counts an all-out innings as the full 20 overs", "[season]") {
  Season s = createSeason(3, 1, "CHK", LEAGUE());
  const Fixture fx = s.fixtures[0];
  MatchSummary sum;
  sum.winner = fx.home;
  InningsSummary a, b;
  a.team = fx.home, a.runs = 180, a.wickets = 5, a.balls = 120;
  b.team = fx.away, b.runs = 100, b.wickets = 10, b.balls = 90, b.allOut = true;
  sum.innings = {a, b};
  recordResult(s, fx.no, sum);
  const auto t = pointsTable(s);
  for (const auto& r : t) {
    if (r.team == fx.home) {
      CHECK_THAT(r.nrr, Catch::Matchers::WithinAbs(180.0 / 20 - 100.0 / 20, 1e-6));
      CHECK(r.pts == 2);
    }
    if (r.team == fx.away) CHECK_THAT(r.nrr, Catch::Matchers::WithinAbs(100.0 / 20 - 180.0 / 20, 1e-6));
  }
}

TEST_CASE("season: a washed-out playoff sends the higher-placed side through", "[season]") {
  Season s = createSeason(5, 1, "CHK", LEAGUE());
  while (nextFixture(s) && nextFixture(s)->stage == Stage::league) {
    const Fixture f = *nextFixture(s);
    recordResult(s, f.no, summarizeMatch(simFixture(f, LEAGUE(), nullptr)));
  }
  const Fixture q1 = *nextFixture(s);
  REQUIRE(q1.stage == Stage::q1);
  MatchSummary washout;
  washout.text = "No result";
  washout.noResult = true;
  recordResult(s, q1.no, washout);
  CHECK(s.fixtures[70].result->winner == q1.home);
  CHECK(s.fixtures[70].result->text.find("higher-placed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Editor, new players and the club captain (editor.test.js, create-player.test.js)
// ---------------------------------------------------------------------------

TEST_CASE("editor: validates name, age and skill ranges", "[editor]") {
  const Player& p = rosterPlayer("CHK-1");
  EditDraft d = draftFrom(p);
  d.name = "   ";
  d.age = 12;
  d.skills.bat.set(BatK::defence, 140);
  const EditResult bad = validateEdit(p, d);
  CHECK_FALSE(bad.ok);
  CHECK(bad.errors.contains("name"));
  CHECK(bad.errors.contains("age"));
  CHECK(bad.errors.contains("bat.defence"));
  EditDraft longName = draftFrom(p);
  longName.name = std::string(30, 'x');
  CHECK(validateEdit(p, longName).errors.contains("name"));
}

TEST_CASE("editor: tidies the name and recomputes headline ratings", "[editor]") {
  const Player& p = rosterPlayer("CHK-1");
  EditDraft d = draftFrom(p);
  d.name = "  Rohan   Mehta ";
  d.age = 27;
  for (size_t i = 0; i < SkillSet<BatK>::N; ++i) d.skills.bat.set(static_cast<BatK>(i), 90);
  const EditResult res = validateEdit(p, d);
  REQUIRE(res.ok);
  CHECK(res.edit.name == "Rohan Mehta");
  CHECK(res.edit.age == 27);
  const Player q = applyEdit(p, res.edit);
  CHECK(q.id == p.id);
  CHECK(q.bat == 90);
  CHECK(q.power == 90);
  CHECK(q.awareness == 90);
  CHECK(q.bowl == p.bowl);
  CHECK(q.edited);
}

TEST_CASE("editor: switching pace to spin converts the bowling at about the same level", "[editor]") {
  const Player* quick = nullptr;
  for (const auto& t : LEAGUE())
    for (const auto& p : t.squad)
      if (!quick && p.role == Role::BOWL && p.bowlType == BowlType::fast) quick = &p;
  EditDraft d = draftFrom(*quick);
  d.bowlType = "off";
  const auto conv = convertBowling(quick->skills.bowl, quick->bowlType, BowlType::off);
  d.skills.bowl = conv;
  const EditResult res = validateEdit(*quick, d);
  REQUIRE(res.ok);
  const Player q = applyEdit(*quick, res.edit);
  CHECK(q.bowlType == BowlType::off);
  CHECK(q.skills.bowl.contains(BowlK::turn));
  CHECK(std::abs(q.bowl - quick->bowl) <= 2);
}

TEST_CASE("editor: written into the world and restored, a year older per season since", "[editor]") {
  World w = createWorld(LEAGUE());
  Player& p = w.players.at("CHK-2");
  const std::string name = p.name;
  const int age = p.age;
  EditDraft d = draftFrom(p);
  d.name = "Test Player";
  const EditResult res = validateEdit(p, d);
  REQUIRE(res.ok);
  writeEdit(p, res.edit, w.year);
  CHECK(p.name == "Test Player");
  CHECK(p.edited);
  CHECK(p.preEdit->name == name);
  CHECK(undoEdit(p, 2));
  CHECK(p.name == name);
  CHECK(p.age == age + 2);
  CHECK_FALSE(p.edited);
  CHECK_FALSE(undoEdit(p, 2));
}

TEST_CASE("new player: role templates are full, valid and repeatable", "[editor][create]") {
  for (Role r : {Role::BAT, Role::WK, Role::AR, Role::BOWL}) {
    const Player t = newPlayerTemplate(r);
    CHECK(t.role == r);
    CHECK(t.bat == battingOverall(t.skills.bat));
    CHECK(t.skills == newPlayerTemplate(r).skills);
  }
  CHECK_FALSE(newPlayerTemplate(Role::WK).bowlType.has_value());
  CHECK(isPaceType(newPlayerTemplate(Role::BOWL).bowlType));
  CHECK(newPlayerTemplate(Role::BOWL).bowl > newPlayerTemplate(Role::BOWL).bat);
  CHECK(newPlayerTemplate(Role::BOWL, BowlType::leg).skills.bowl.contains(BowlK::turn));
}

TEST_CASE("new player: validation, duplicate names and creation in the world", "[editor][create]") {
  EditDraft d = draftFrom(newPlayerTemplate(Role::BOWL));
  d.name = "  Kiran   Quick ";
  d.age = 23;
  d.overseas = true;
  for (const auto& def : PACE_SKILLS) d.skills.bowl.set(def.key, 88);
  const EditResult ok = validateNewPlayer(d);
  REQUIRE(ok.ok);
  CHECK(ok.edit.name == "Kiran Quick");
  CHECK(validateNewPlayer(d, {"kiran QUICK"}).errors.contains("name"));
  EditDraft badRole = d;
  badRole.role = "COACH";
  CHECK(validateNewPlayer(badRole).errors.contains("role"));

  World w = createWorld(LEAGUE());
  const int nextId = w.nextId;
  const size_t before = w.teams.at("CHK").squad.size();
  const CreateResult res = createPlayer(w, "CHK", ok.edit);
  REQUIRE(res.ok);
  CHECK(res.id == cat('P', nextId));
  CHECK(w.teams.at("CHK").squad.size() == before + 1);
  const Player& p = w.players.at(res.id);
  CHECK(p.origin == Origin::custom);
  CHECK(p.overseas);
  CHECK(p.bowl == 88);
  CHECK(p.contract->via == ContractVia::custom);
  CHECK(*p.potential >= mainRating(p));
  CHECK_FALSE(p.edited);
  CHECK_FALSE(createPlayer(w, "XXX", ok.edit).ok);
  w.offseason = Offseason{};
  CHECK_FALSE(createPlayer(w, "KNR", ok.edit).ok);
}

TEST_CASE("club captain: only squad members, lapses on leaving, picked whenever fit", "[editor][captain]") {
  World w = createWorld(LEAGUE());
  const PlayerId mine = w.teams.at("CHK").squad[3];
  CHECK_FALSE(setClubCaptain(w, "CHK", w.teams.at("MUI").squad[0]));
  CHECK(setClubCaptain(w, "CHK", mine));
  CHECK(clubCaptainOf(w, "CHK") == mine);
  CHECK(teamById(worldTeams(w), "CHK")->captainId == mine);
  w.players.at(mine).team = "MUI";
  CHECK_FALSE(clubCaptainOf(w, "CHK").has_value());
  w.players.at(mine).team = "CHK";

  // A batter the best XI leaves out is picked once he's captain, replacing his own role
  const Team& team = LEAGUE()[0];
  const BestXI base = pickBestXI(team);
  const Player* bench = nullptr;
  for (const auto& p : team.squad)
    if (p.role == Role::BAT && std::find(base.ids.begin(), base.ids.end(), p.id) == base.ids.end()) bench = &p;
  REQUIRE(bench);
  Team withCap = team;
  withCap.captainId = bench->id;
  const BestXI xi = pickBestXI(withCap);
  CHECK(std::find(xi.ids.begin(), xi.ids.end(), bench->id) != xi.ids.end());
  CHECK(xi.ids.size() == 11);
  // ...and leads the side when no captain is named for the match
  const SideInput side = buildSide(withCap, xi.ids, xi.keeperId);
  CHECK(side.captainId == bench->id);
  // Injured, he sits out
  for (auto& p : withCap.squad)
    if (p.id == bench->id) p.unavailable = true;
  const BestXI hurt = pickBestXI(withCap);
  CHECK(std::find(hurt.ids.begin(), hurt.ids.end(), bench->id) == hurt.ids.end());
}

// ---------------------------------------------------------------------------
// Milestones and achievements
// ---------------------------------------------------------------------------

TEST_CASE("milestones: ladders, reached, upcoming and the nearest", "[milestones]") {
  CareerStats s;
  s.bat.runs = 980;
  s.matches = 49;
  OrderedMap<PlayerId, CareerStats> players;
  players.set("A-1", s);
  const auto up = upcoming(players, {"A-1"});
  REQUIRE_FALSE(up.empty());
  CHECK(up[0].type == "matches");  // plays his 50th
  CHECK(up[1].text == "20 runs to 1,000 career runs");
  const auto next = nextMilestone(&s);
  CHECK(next->type == "runs");
  CHECK(next->target == 1000);
  CHECK(milestoneText("hundred", 1) == "First hundred");
}

TEST_CASE("achievements: a season of the user's club unlocks match achievements", "[milestones]") {
  Career career;
  Season s = createSeason(9, 1, "MUI", LEAGUE());
  for (int i = 0; i < 74; ++i) {
    const Fixture* fx = nextFixture(s);
    if (!fx) break;
    const Fixture f = *fx;
    Match m = simFixture(f, LEAGUE(), &career.players, &career.matchups);
    m.fixture = FixtureRef{f.no, s.id, s.number, f.stage, f.venue, f.home};
    if (f.home == "MUI" || f.away == "MUI") m.managed = "MUI";
    recordMatch(career, m, &kBaseOf);
    recordResult(s, f.no, summarizeMatch(m));
  }
  CHECK(career.manager.played >= 14);
  CHECK(career.manager.wins > 0);
  CHECK(career.manager.unlocked.contains("first_win"));
  CHECK(career.manager.unlocked.at("first_win").when == "Season 1");
  const auto list = achievementList(career.manager, {}, {});
  CHECK(list.size() == 23);
  CHECK(std::count_if(list.begin(), list.end(), [](const AchievementState& a) { return a.done; }) >= 1);
  CHECK_FALSE(career.milestones.empty());
}
