#pragma once
// T20 rules and small helpers shared by the engine, the AI captain and the UI (rules.js).

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/match_types.hpp"

namespace t20 {

inline constexpr int OVERS = 20;
inline constexpr int BALLS = OVERS * 6;
inline constexpr int MAX_OVERS_PER_BOWLER = 4;

// In a shortened innings each bowler gets a fifth of the overs (rounded up)
int bowlerQuota(const Innings* inn);
int inningsOvers(const Innings* inn);
inline Innings& inningsOf(Match& m) { return m.innings[static_cast<size_t>(m.current)]; }
inline const Innings& inningsOf(const Match& m) { return m.innings[static_cast<size_t>(m.current)]; }
inline int overIndex(const Innings& inn) { return inn.legalBalls / 6; }
inline Phase phaseOf(int over) { return over < 6 ? Phase::pp : over < 15 ? Phase::mid : Phase::death; }
// An over on the 20-over scale, so a 10-over game still has a powerplay and a death
int over20(const Innings* inn, int over);
inline Phase phaseAt(const Innings* inn, int over) { return phaseOf(over20(inn, over)); }
std::string_view phaseLabel(Phase p);

std::string oversText(int balls);
std::string ballTag(int balls);

inline double runRate(double runs, double balls) { return balls == 0 ? 0 : (runs * 6) / balls; }
std::optional<double> requiredRate(const Innings& inn);
int oversBowledBy(const Innings& inn, const PlayerId& id);

// Can `remaining` overs still be bowled given each bowler's remaining quota (caps), with nobody
// bowling consecutive overs, when `last` bowled the previous over? Greedy "largest quota first".
bool canFinishOvers(const std::vector<std::pair<PlayerId, int>>& caps, const std::optional<PlayerId>& last,
                    int remaining);

struct BowlOption {
  PlayerId id;
  int bowled = 0;
  bool ok = false;
  std::string reason;  // empty when ok
};
// Who can bowl the next over, and why the others can't.
std::vector<BowlOption> bowlingOptions(const Match& match);

}  // namespace t20
