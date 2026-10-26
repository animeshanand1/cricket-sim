#pragma once
// Career milestones (milestones.js): round numbers a player is closing in on, the ones he reaches,
// and personal bests. Display only: draws no random numbers and never changes a result.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"

namespace t20 {

inline constexpr size_t MILESTONE_LOG = 50;
inline constexpr int APPEARANCES[] = {50, 100, 150, 200, 250, 300};

struct Counts {
  int runs = 0, wickets = 0, sixes = 0, fours = 0, catches = 0;
  int get(std::string_view type) const;
};
Counts careerCounts(const CareerStats* s);
OrderedMap<PlayerId, Counts> matchCounts(const Match& match);

std::string milestoneText(std::string_view type, int value);

struct Snap {
  Counts c;
  int matches = 0, hundreds = 0;
};
OrderedMap<PlayerId, Snap> snapshot(const OrderedMap<PlayerId, CareerStats>& careerPlayers,
                                    const std::vector<PlayerId>& ids);
std::vector<MilestoneHit> milestonesReached(const OrderedMap<PlayerId, Snap>& before,
                                            const OrderedMap<PlayerId, CareerStats>& careerPlayers, const Match& match);

struct Chase {
  std::string kind, type, text;  // kind: reached | record | best | chase
};
struct ChaseInput {
  const CareerStats* stats = nullptr;
  const Counts* inMatch = nullptr;
  std::string_view as;  // "bat" / "bowl"
  const BatterInnings* bat = nullptr;
  const BowlingFigures* fig = nullptr;
  const Records* records = nullptr;
  size_t limit = 3;
};
std::vector<Chase> playerChases(const ChaseInput& in);

struct Reached {
  std::string key;
  PlayerId id;
  std::string type;
  int value;
};
std::vector<Reached> reachedInMatch(const OrderedMap<PlayerId, CareerStats>& careerPlayers, const Match& match);

struct Upcoming {
  PlayerId id;
  std::string type;
  int need, target;
  std::string text;
};
std::vector<Upcoming> upcoming(const OrderedMap<PlayerId, CareerStats>& careerPlayers, const std::vector<PlayerId>& ids,
                               double reach = 1.5, size_t perPlayer = 2);

struct NextMilestone {
  std::string type;
  int need, target;
  std::string text;
};
std::optional<NextMilestone> nextMilestone(const CareerStats* stats);

}  // namespace t20
