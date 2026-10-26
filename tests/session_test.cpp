// MatchSession: the game state behind the Match screen (S05 5a/5b). Checks that the captain's
// calls go through to the engine and that the dock behaves like MatchDock.jsx.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "app/match_session.hpp"
#include "engine/field.hpp"
#include "engine/league.hpp"
#include "engine/match.hpp"
#include "engine/rules.hpp"

using namespace t20;

namespace {

// Play until the engine asks the captain for `what` (the AI answers any other call)
bool playUntil(app::MatchSession& s, Awaiting what, int guard = 600) {
  for (int i = 0; i < guard && s.match().status != MatchStatus::complete; ++i) {
    if (s.match().awaiting == what) return true;
    if (s.match().awaiting != Awaiting::none) s.letAiDecide();
    else s.nextBall();
  }
  return s.match().awaiting == what;
}

int ballsBowled(const Match& m) {
  int n = 0;
  for (const auto& inn : m.innings) n += inn.legalBalls;
  return n;
}

}  // namespace

TEST_CASE("session: the user captains the home side and makes the calls", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(s.hasMatch());
  REQUIRE(s.match().userTeam == 0);
  CHECK_FALSE(s.match().settings.autoBowl);
  CHECK_FALSE(s.match().settings.autoBat);
  CHECK_FALSE(s.match().settings.autoReview);
}

TEST_CASE("session: Next ball and Next over wait while a call is pending", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(playUntil(s, Awaiting::bowler));
  const int before = ballsBowled(s.match());
  s.nextBall();
  s.nextOver();
  CHECK(ballsBowled(s.match()) == before);
  CHECK(s.match().awaiting == Awaiting::bowler);
}

TEST_CASE("session: picking a bowler", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(playUntil(s, Awaiting::bowler));
  const auto opts = bowlingOptions(s.match());
  const auto bad = std::find_if(opts.begin(), opts.end(), [](const BowlOption& o) { return !o.ok; });
  const auto good = std::find_if(opts.begin(), opts.end(), [](const BowlOption& o) { return o.ok; });
  REQUIRE(good != opts.end());
  if (bad != opts.end()) {
    s.pickBowler(bad->id);  // not allowed: nothing changes
    CHECK(s.match().awaiting == Awaiting::bowler);
  }
  s.pickBowler(good->id);
  CHECK(s.match().awaiting == Awaiting::none);
  CHECK(*inningsOf(s.match()).currentBowler == good->id);
}

TEST_CASE("session: auto-bowl answers a pending bowler call at once", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(playUntil(s, Awaiting::bowler));
  s.setAutoBowl(true);
  CHECK(s.match().awaiting == Awaiting::none);
  CHECK(inningsOf(s.match()).currentBowler.has_value());
}

TEST_CASE("session: sending in the next batter", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(playUntil(s, Awaiting::batter));
  const PlayerId last = inningsOf(s.match()).yetToBat.back();
  s.pickBatter(last);  // out of order is allowed
  CHECK(s.match().awaiting == Awaiting::none);
  const Innings& inn = inningsOf(s.match());
  CHECK(((inn.striker && *inn.striker == last) || (inn.nonStriker && *inn.nonStriker == last)));
}

TEST_CASE("session: plan, field, tempo and target reach the engine", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  // the home side bowls first with this seed
  REQUIRE(playUntil(s, Awaiting::bowler));
  s.letAiDecide();
  REQUIRE(userIsBowling(s.match()));
  const PlayerId bowler = *inningsOf(s.match()).currentBowler;

  s.setBowlingPlan(Plan::yorkers);
  CHECK(inningsOf(s.match()).plans.at(bowler) == Plan::yorkers);
  s.setBowlingPlan(std::nullopt);
  CHECK_FALSE(inningsOf(s.match()).plans.at(bowler).has_value());

  const std::vector<Pos> custom = {Pos::slip1, Pos::point, Pos::cover, Pos::midOff, Pos::midOn,
                                   Pos::squareLeg, Pos::deepPoint, Pos::fineLeg, Pos::midWicket};
  REQUIRE(fieldProblems(custom, Phase::pp).empty());
  s.setField(FieldPreset::custom, custom);
  REQUIRE(inningsOf(s.match()).userField);
  CHECK(inningsOf(s.match()).userField->preset == FieldPreset::custom);
  CHECK(*inningsOf(s.match()).userField->positions == custom);
  s.nextBall();  // the engine bowls with the custom field
  CHECK(ballsBowled(s.match()) >= 1);

  // batting controls are match settings: usable for the chase later
  s.setBatterTempo("someone", Tempo{5.0});
  CHECK(batterTempoOf(s.match(), "someone") == Tempo{5.0});
  s.setBatterTarget(bowler, TargetMode::attack);
  CHECK(s.match().settings.targets.at(bowler) == TargetMode::attack);
}

TEST_CASE("session: a review call settles the pending ball", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  REQUIRE(playUntil(s, Awaiting::review));
  const Innings& inn = inningsOf(s.match());
  const int reviewsBefore = inn.reviewsLeft.bat + inn.reviewsLeft.bowl;
  s.takeEvents();
  s.reviewCall(true);
  CHECK(s.match().awaiting != Awaiting::review);
  CHECK_FALSE(s.match().pendingBall.has_value());
  const Innings& after = inningsOf(s.match());
  CHECK(after.reviewsLeft.bat + after.reviewsLeft.bowl <= reviewsBefore);
  CHECK(s.takeEvents().size() == 1);  // the ball now counts, with its event
}

TEST_CASE("session: matches with every call made finish, and Rematch starts afresh", "[session]") {
  for (int32_t seed : {1, 2, 3, 2026}) {
    app::MatchSession s;
    s.newMatch(seed % 10, (seed + 3) % 10, seed);
    for (int i = 0; i < 2000 && s.match().status != MatchStatus::complete; ++i) {
      if (s.match().awaiting != Awaiting::none) s.letAiDecide();
      else s.nextBall();
    }
    REQUIRE(s.match().status == MatchStatus::complete);
    REQUIRE(s.match().result.has_value());
    s.rematch();
    CHECK(s.match().status == MatchStatus::live);
    CHECK(ballsBowled(s.match()) == 0);
  }
}

TEST_CASE("session: the Impact Player substitution (5c)", "[session]") {
  app::MatchSession s;
  s.newMatch(5, 7, 2026);
  const int side = *s.match().userTeam;
  // Mid-over the window is shut
  REQUIRE(playUntil(s, Awaiting::bowler));
  s.letAiDecide();
  s.nextBall();
  if (s.match().awaiting == Awaiting::none && inningsOf(s.match()).legalBalls % 6 != 0) {
    const ImpactOptions shut = impactOptions(s.match(), side);
    CHECK_FALSE(shut.allowed);
    CHECK(shut.reason == "Only at an innings break, the end of an over, or after a wicket");
  }
  // At the next bowler call it opens, and the vice-captain's suggestion goes through
  REQUIRE(playUntil(s, Awaiting::bowler));
  const ImpactOptions open = impactOptions(s.match(), side);
  REQUIRE(open.allowed);
  const auto plan = aiImpactPlan(s.match(), side);
  REQUIRE(plan);
  const MatchTeam& before = s.match().teams[static_cast<size_t>(side)];
  REQUIRE(std::find(before.players.begin(), before.players.end(), plan->out) != before.players.end());

  // An overseas player in for a local one when four are already on is refused
  int overseas = 0;
  for (const auto& id : before.players) overseas += s.match().players.at(id).overseas ? 1 : 0;
  const auto osIn = std::find_if(open.ins.begin(), open.ins.end(), [&](const ImpactIn& x) { return s.match().players.at(x.id).overseas; });
  const auto localOut = std::find_if(open.outs.begin(), open.outs.end(), [&](const PlayerId& id) { return !s.match().players.at(id).overseas; });
  if (overseas >= MAX_OVERSEAS && osIn != open.ins.end() && localOut != open.outs.end()) {
    CHECK_FALSE(s.impactSub(side, *localOut, osIn->id));
    CHECK_FALSE(s.match().teams[static_cast<size_t>(side)].impact);
  }

  REQUIRE(s.impactSub(side, plan->out, plan->in));
  const MatchTeam& after = s.match().teams[static_cast<size_t>(side)];
  REQUIRE(after.impact);
  CHECK(after.impact->in == plan->in);
  CHECK(std::find(after.players.begin(), after.players.end(), plan->in) != after.players.end());
  CHECK(std::find(after.players.begin(), after.players.end(), plan->out) == after.players.end());
  // The replaced player is no longer offered as a bowler
  for (const auto& o : bowlingOptions(s.match())) CHECK(o.id != plan->out);
  // Only one per match
  CHECK(impactOptions(s.match(), side).reason == "Impact Player already used");
  CHECK_FALSE(s.impactSub(side, after.players.back(), before.bench.front()));
}
