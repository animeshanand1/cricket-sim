#pragma once
// The AI captain (ai.js): bowling changes, batting tempo, next batter and the toss call. Also
// the "auto captain" for the user's side when they sim ahead.

#include <string>

#include "engine/match_types.hpp"

namespace t20 {

// How much a bowler's history against the two batters at the crease is worth
double matchupBonus(const Match& match, const Innings& inn, const PlayerId& bowlerId);
// Draws one number from the match stream per legal option
PlayerId aiChooseBowler(Match& match);
// Batting tempo 1 (defend) .. 5 (all-out attack)
double aiAggression(const Match& match);

struct NextBatterCall {
  PlayerId id;
  std::string reason;  // empty = the batting order
};
NextBatterCall nextBatterCall(const Match& match);
PlayerId aiNextBatter(const Match& match);
TossDecision aiTossDecision(Match& match);

}  // namespace t20
