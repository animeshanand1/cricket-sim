#pragma once
// Turns detailed skills into the numbers the ball model uses for this delivery (battle.js).
// Bowler: threat (wicket-taking now) and control (run-stopping now).
// Batter: survive, hit4, hit6, rotate. Fielding side: catching, ground fielding, throwing, keeping.

#include <utility>
#include <vector>

#include "engine/match_types.hpp"

namespace t20 {

struct BowlerNow {
  double threat, control;
  bool pace;
};
struct BallContext {
  int over;
  Phase phase;
  Pitch pitch;
  double wear;
  bool dew;
};
BowlerNow bowlerNow(const Player& bowler, const BallContext& ctx);

struct BatterNow {
  double survive, hit4, hit6, rotate, running, lofted, temperament;
};
BatterNow batterNow(const Player& batter, bool spin, bool pressure);

struct FieldingNow {
  double catching, ground, throwing, keeping;
};
FieldingNow fieldingNow(const Match& match, const Innings& inn);

using DismissalWeights = std::vector<std::pair<Out, double>>;
// How each kind of dismissal becomes more or less likely, given who is bowling and fielding.
// Order matters (it is the JS object's key order): pace = caught, bowled, lbw, caughtKeeper,
// runout; spin = caught, bowled, lbw, stumped, caughtKeeper, runout.
DismissalWeights dismissalWeights(const Player& bowler, Phase phase, bool spin, double running,
                                  const FieldingNow& field);

// Chance a catch goes down / a stumping is missed
double dropChance(double skill);
double missedStumpingChance(double keeping);

struct Edge {
  double wicket, boundary;
};
// Specialist edges: where a bowler's particular strength suits the moment
Edge specialistEdge(const Player& bowler, const BallContext& ctx);

}  // namespace t20
