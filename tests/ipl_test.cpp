// Port of t20-coach/tests/ipl.test.js

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <unordered_map>

#include "engine/league.hpp"
#include "engine/match.hpp"
#include "test_sides.hpp"

using namespace t20;

namespace {
const std::vector<Team>& league() {
  static const std::vector<Team> l = generateLeague();
  return l;
}
const Player& byId(const PlayerId& id) {
  static const std::unordered_map<PlayerId, const Player*> idx = [] {
    std::unordered_map<PlayerId, const Player*> m;
    for (const Team& t : league())
      for (const Player& p : t.squad) m[p.id] = &p;
    return m;
  }();
  return *idx.at(id);
}
bool contains(const std::vector<PlayerId>& v, const PlayerId& id) { return std::find(v.begin(), v.end(), id) != v.end(); }
}  // namespace

TEST_CASE("10 teams, 4-6 overseas per squad and never more than 4 in an XI", "[ipl]") {
  REQUIRE(league().size() == 10);
  for (const Team& t : league()) {
    const auto os = std::count_if(t.squad.begin(), t.squad.end(), [](const Player& p) { return p.overseas; });
    CHECK(os >= 4);
    CHECK(os <= 6);
    const BestXI xi = pickBestXI(t, Pitch::dusty);
    CHECK(std::count_if(xi.ids.begin(), xi.ids.end(), [](const PlayerId& id) { return byId(id).overseas; }) <= MAX_OVERSEAS);
  }
}

TEST_CASE("Impact Player: one substitution per side from the bench, keeping 11 players", "[ipl]") {
  for (int seed = 1; seed <= 60; ++seed) {
    MatchSetup s;
    s.seed = seed;
    s.userTeam.reset();
    s.teams = {testSide(league()[static_cast<size_t>(seed % 10)], Pitch::balanced),
               testSide(league()[static_cast<size_t>((seed + 3) % 10)], Pitch::balanced)};
    Match m = createMatch(s);
    simMatch(m);
    for (const MatchTeam& t : m.teams) {
      CHECK(t.players.size() == 11);
      CHECK(std::count_if(t.players.begin(), t.players.end(), [&](const PlayerId& id) { return m.players.at(id).overseas; }) <=
            MAX_OVERSEAS);
      if (t.impact) {
        CHECK(contains(t.players, t.impact->in));
        CHECK_FALSE(contains(t.players, t.impact->out));
      }
    }
    // At most 11 batters per innings
    for (const Innings& inn : m.innings)
      if (!inn.superOver) CHECK(inn.battedOrder.size() <= 11);
  }
}

TEST_CASE("Impact Player only between overs, after a wicket or at the break, and only once", "[ipl]") {
  MatchSetup s;
  s.seed = 7;
  s.userTeam = 0;
  s.settings.autoBowl = true;
  s.settings.autoBat = true;
  s.teams = {testSide(league()[0], Pitch::balanced), testSide(league()[1], Pitch::balanced)};
  Match m = createMatch(s);
  autoToss(m);
  playBall(m);  // mid-over now (unless it was a wide)
  const Innings& inn = m.innings[0];
  if (inn.legalBalls % 6 != 0 && m.awaiting == Awaiting::none) CHECK_FALSE(impactOptions(m, 0).allowed);
  int guard = 0;
  while (m.status == MatchStatus::live && guard++ < 1000) playBall(m);
  REQUIRE(m.status == MatchStatus::break_);
  const ImpactOptions opts = impactOptions(m, 0);
  REQUIRE(opts.allowed);
  const auto inIt = std::find_if(opts.ins.begin(), opts.ins.end(), [](const ImpactIn& x) { return x.ok; });
  REQUIRE(inIt != opts.ins.end());
  const PlayerId inId = inIt->id;
  const auto outIt = std::find_if(opts.outs.begin(), opts.outs.end(),
                                  [&](const PlayerId& o) { return byId(o).overseas || !byId(inId).overseas; });
  REQUIRE(outIt != opts.outs.end());
  CHECK(useImpactPlayer(m, 0, *outIt, inId));
  CHECK_FALSE(impactOptions(m, 0).allowed);
  startSecondInnings(m);
  CHECK(contains(m.teams[0].players, inId));
}

TEST_CASE("roster: 16 per squad in the 2/5/4/5 shape, unique names and ids", "[ipl]") {
  std::set<std::string> names;
  for (const Team& t : generateLeague()) {
    REQUIRE(t.squad.size() == 16);
    auto n = [&](Role r) { return std::count_if(t.squad.begin(), t.squad.end(), [&](const Player& p) { return p.role == r; }); };
    CHECK(n(Role::WK) == 2);
    CHECK(n(Role::BAT) == 5);
    CHECK(n(Role::AR) == 4);
    CHECK(n(Role::BOWL) == 5);
    for (const Player& p : t.squad) {
      CHECK(names.insert(p.name).second);
      CHECK(p.id.rfind(t.id + "-", 0) == 0);
    }
    CHECK(std::count_if(t.squad.begin(), t.squad.end(), [](const Player& p) { return p.bowl >= 60; }) >= 5);
  }
}
