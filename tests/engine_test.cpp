// Port of t20-coach/tests/engine.test.js: scorecards reconcile, the rules hold, decisions work.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

#include "engine/league.hpp"
#include "engine/match.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "test_sides.hpp"

using namespace t20;

namespace {
const std::vector<Team>& league() {
  static const std::vector<Team> l = generateLeague();
  return l;
}

Weather dry() {
  Weather w;
  w.sky = Sky::clear;
  return w;
}

// Dry weather: rain-shortened games are covered by the golden parity runs
Match aiMatch(int seed, Pitch pitch = Pitch::balanced, int a = 0, int b = 1) {
  MatchSetup s;
  s.seed = seed;
  s.pitch = pitch;
  s.userTeam.reset();
  s.teams = {testSide(league()[static_cast<size_t>(a)], pitch, false),
             testSide(league()[static_cast<size_t>(b)], pitch, false)};
  s.weather = dry();
  return createMatch(s);
}

void checkInnings(const Match& m, const Innings& inn) {
  // Totals reconcile: batter runs + extras = team runs
  int batRuns = 0;
  for (const auto& [id, b] : inn.batters) batRuns += b.runs;
  const int extras = inn.extras.wd + inn.extras.nb + inn.extras.lb + inn.extras.b;
  CHECK(batRuns + extras == inn.runs);
  // Over-by-over runs reconcile too
  int overRuns = 0;
  for (const Over& o : inn.overs) overRuns += o.runs;
  CHECK(overRuns == inn.runs);
  // Bowler runs = team runs minus byes and leg byes
  int bowlRuns = 0, bowlBalls = 0;
  for (const auto& [id, f] : inn.bowling) {
    bowlRuns += f.runs;
    bowlBalls += f.balls;
  }
  CHECK(bowlRuns == inn.runs - inn.extras.lb - inn.extras.b);
  // Legal balls
  CHECK(inn.legalBalls <= 120);
  CHECK(bowlBalls == inn.legalBalls);
  // Wickets
  CHECK(inn.wickets <= 10);
  int outs = 0;
  for (const auto& [id, b] : inn.batters) outs += b.out ? 1 : 0;
  CHECK(outs == inn.wickets);
  CHECK(static_cast<int>(inn.fow.size()) == inn.wickets);
  // Bowling limits: max 4 overs, never consecutive overs, keeper never bowls
  const PlayerId& keeper = m.teams[static_cast<size_t>(inn.bowlingSide)].keeperId;
  for (const auto& [id, f] : inn.bowling) {
    CHECK(f.balls <= MAX_OVERS_PER_BOWLER * 6);
    CHECK(id != keeper);
  }
  for (size_t k = 1; k < inn.overs.size(); ++k) CHECK(inn.overs[k].bowler != inn.overs[k - 1].bowler);
  // Every over except the last has 6 legal balls (wides, no-balls and a stumping off a wide don't count)
  for (size_t k = 0; k + 1 < inn.overs.size(); ++k) {
    int legal = 0;
    for (const Ball& b : inn.overs[k].balls)
      if (b.kind != BallKind::extra && !b.wide) ++legal;
    CHECK(legal == 6);
  }
}
}  // namespace

TEST_CASE("match engine produces a consistent result across many seeds and pitches", "[engine]") {
  int superOvers = 0;
  for (int seed = 1; seed <= 600; ++seed) {
    const Pitch pitch = PITCH_KEYS[static_cast<size_t>(seed) % PITCH_KEYS.size()];
    Match m = aiMatch(seed, pitch, seed % 8, (seed + 3) % 8);
    simMatch(m);
    REQUIRE(m.status == MatchStatus::complete);
    for (const Innings& inn : m.innings)
      if (!inn.superOver) checkInnings(m, inn);
    const Innings& a = m.innings[0];
    const Innings& b = m.innings[1];
    CHECK(*b.target == a.runs + 1);
    if (b.runs >= *b.target) {
      CHECK(m.result->winner == b.batting);
    } else if (b.runs == a.runs) {
      // IPL: a tie goes to a Super Over (repeated until there's a winner)
      CHECK(m.result->superOver);
      std::vector<const Innings*> so;
      for (const Innings& i : m.innings)
        if (i.superOver) so.push_back(&i);
      CHECK(so.size() % 2 == 0);
      const Innings& last = *so.back();
      CHECK(m.result->winner == (last.runs >= *last.target ? last.batting : last.bowlingSide));
      CHECK(so[0]->batting == b.batting);  // the side that batted second bats first
      for (const Innings* inn : so) {
        CHECK(inn->legalBalls <= 6);
        CHECK(inn->wickets <= 2);
        CHECK(inn->battedOrder.size() <= 3);
      }
      ++superOvers;
    } else {
      CHECK(m.result->winner == a.batting);
    }
    // 2nd innings stops as soon as the target is reached
    if (b.runs >= *b.target) CHECK(b.runs - b.overs.back().runs < *b.target);
    CHECK(m.potm.has_value());
  }
  CHECK(superOvers > 0);
}

TEST_CASE("match engine is deterministic for the same seed", "[engine]") {
  Match a = aiMatch(42);
  Match b = aiMatch(42);
  simMatch(a);
  simMatch(b);
  REQUIRE(a.innings.size() == b.innings.size());
  for (size_t i = 0; i < a.innings.size(); ++i) {
    CHECK(a.innings[i].runs == b.innings[i].runs);
    CHECK(a.innings[i].wickets == b.innings[i].wickets);
    CHECK(a.innings[i].legalBalls == b.innings[i].legalBalls);
  }
  CHECK(a.rngState == b.rngState);
}

TEST_CASE("asks the user for bowler and batter decisions and respects them", "[engine]") {
  int sawBowler = 0, sawBatter = 0, sawReview = 0;
  for (int seed = 1; seed <= 20; ++seed) {
    MatchSetup s;
    s.seed = seed;
    s.userTeam = 0;
    s.teams = {testSide(league()[0], Pitch::balanced, false), testSide(league()[1], Pitch::balanced, false)};
    Match m = createMatch(s);
    callToss(m, "heads");
    if (!m.toss->decision) chooseTossDecision(m, seed % 2 ? TossDecision::bat : TossDecision::bowl);
    int guard = 0;
    while (m.status != MatchStatus::complete && guard++ < 2000) {
      if (m.status == MatchStatus::break_) startSecondInnings(m);
      if (m.awaiting == Awaiting::review) {
        CHECK(reviewDecision(m, seed % 3 == 0));
        CHECK(m.awaiting != Awaiting::review);
        ++sawReview;
      } else if (m.awaiting == Awaiting::bowler) {
        const auto opts = bowlingOptions(m);
        const auto ok = std::find_if(opts.begin(), opts.end(), [](const BowlOption& o) { return o.ok; });
        REQUIRE(ok != opts.end());
        CHECK(setBowler(m, ok->id));
        ++sawBowler;
        const auto after = bowlingOptions(m);
        const auto bad = std::find_if(after.begin(), after.end(), [](const BowlOption& o) { return !o.ok; });
        if (bad != after.end()) CHECK_FALSE(setBowler(m, bad->id));
      } else if (m.awaiting == Awaiting::batter) {
        const Innings& inn = m.innings[static_cast<size_t>(m.current)];
        const PlayerId pick = inn.yetToBat.back();  // send the No. 11 in: user choice honoured
        CHECK(setNextBatter(m, pick));
        const Innings& now = m.innings[static_cast<size_t>(m.current)];
        CHECK((now.striker == pick || now.nonStriker == pick));
        ++sawBatter;
      } else if (seed % 2) {
        playBall(m);
      } else {
        playOver(m);
      }
    }
    REQUIRE(m.status == MatchStatus::complete);
    for (const Innings& inn : m.innings) checkInnings(m, inn);
  }
  CHECK(sawBowler > 50);
  CHECK(sawBatter > 10);
  CHECK(sawReview > 5);  // DRS calls go to the user too
}

TEST_CASE("bowling feasibility detects impossible over sequences", "[engine]") {
  using Caps = std::vector<std::pair<PlayerId, int>>;
  CHECK(canFinishOvers(Caps{{"a", 4}, {"b", 4}, {"c", 4}, {"d", 4}, {"e", 4}}, std::nullopt, 20));
  CHECK_FALSE(canFinishOvers(Caps{{"a", 3}, {"b", 1}}, PlayerId("b"), 4));  // a would bowl back-to-back
  CHECK(canFinishOvers(Caps{{"a", 2}, {"b", 1}}, PlayerId("b"), 3));        // a b a
  CHECK_FALSE(canFinishOvers(Caps{{"a", 4}, {"b", 4}, {"c", 4}, {"d", 4}}, std::nullopt, 20));  // only 16 overs
}

TEST_CASE("lets the captain swap the bowler until the first ball, then locks it", "[engine]") {
  // Find a seed where the user (team 0) wins the toss, then bowl first
  Match m;
  for (int seed = 1; seed < 50; ++seed) {
    MatchSetup s;
    s.seed = seed;
    s.userTeam = 0;
    s.teams = {testSide(league()[0], Pitch::balanced), testSide(league()[1], Pitch::balanced)};
    m = createMatch(s);
    const Toss t = callToss(m, "heads");
    if (t.winner == 0) {
      chooseTossDecision(m, TossDecision::bowl);
      break;
    }
  }
  REQUIRE(m.awaiting == Awaiting::bowler);
  std::vector<BowlOption> opts;
  for (const auto& o : bowlingOptions(m))
    if (o.ok) opts.push_back(o);
  setBowler(m, opts[0].id);
  CHECK(canChangeBowler(m));
  CHECK(changeBowler(m));
  CHECK(m.awaiting == Awaiting::bowler);
  const Innings& inn = m.innings[static_cast<size_t>(m.current)];
  CHECK_FALSE(inn.bowling.contains(opts[0].id));
  CHECK(inn.overs.empty());
  // Pick someone else, bowl a ball: now it's locked
  setBowler(m, opts[1].id);
  CHECK(m.innings[static_cast<size_t>(m.current)].currentBowler == opts[1].id);
  playBall(m);
  CHECK_FALSE(canChangeBowler(m));
  CHECK_FALSE(changeBowler(m));
}

TEST_CASE("ends the innings when a side short of 11 runs out of batters", "[engine]") {
  for (int seed = 1; seed <= 40; ++seed) {
    SideInput a = testSide(league()[0], Pitch::green, false);
    SideInput b = testSide(league()[1], Pitch::green, false);
    a.players.resize(10);
    b.players.resize(7);
    MatchSetup s;
    s.seed = seed;
    s.pitch = Pitch::green;
    s.userTeam.reset();
    s.teams = {a, b};
    s.weather = dry();
    Match m = createMatch(s);
    REQUIRE_NOTHROW(simMatch(m));
    CHECK(m.status == MatchStatus::complete);
    for (const Innings& inn : m.innings)
      if (!inn.superOver) CHECK(inn.wickets <= static_cast<int>(m.teams[static_cast<size_t>(inn.batting)].players.size()) - 1);
  }
}

TEST_CASE("counts wickets in hand from the batters a short side actually has", "[engine]") {
  int chases = 0;
  for (int seed = 1; seed <= 80; ++seed) {
    std::array<SideInput, 2> sides = {testSide(league()[0], Pitch::flat, false), testSide(league()[1], Pitch::flat, false)};
    for (SideInput& sd : sides) {
      // drop the last player who isn't the keeper: 10 players, 9 wickets
      for (auto it = sd.players.rbegin(); it != sd.players.rend(); ++it) {
        if (it->id != sd.keeperId) {
          sd.players.erase(std::next(it).base());
          break;
        }
      }
    }
    MatchSetup s;
    s.seed = seed;
    s.pitch = Pitch::flat;
    s.userTeam.reset();
    s.teams = sides;
    s.weather = dry();
    Match m = createMatch(s);
    simMatch(m);
    const Innings& chase = m.innings[1];
    if (m.result->superOver || chase.runs < *chase.target) continue;
    ++chases;
    CHECK(chase.maxWickets == 9);
    const int wk = 9 - chase.wickets;
    CHECK(m.result->text.find("won by " + std::to_string(wk) + (wk == 1 ? " wicket" : " wickets")) != std::string::npos);
  }
  CHECK(chases > 5);
}

TEST_CASE("still names 11 when fewer than 11 players are fit", "[engine]") {
  Team team = league()[2];
  for (size_t i = 0; i < team.squad.size(); ++i) {
    team.squad[i].unavailable = i >= 9;
    if (i >= 9) team.squad[i].injury = Injury{"", "", static_cast<int>(i), 0, std::nullopt};
  }
  const BestXI xi = pickBestXI(team);
  CHECK(xi.ids.size() == 11);
  std::vector<PlayerId> sorted = xi.ids;
  std::sort(sorted.begin(), sorted.end());
  CHECK(std::unique(sorted.begin(), sorted.end()) == sorted.end());
}
