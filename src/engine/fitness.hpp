#pragma once
// Fitness and injuries (fitness.js). Every match takes something out of the players who play it,
// they recover between games (older players more slowly), tired players lose a little sharpness
// and are much more likely to get injured, and injuries rule a player out for some matches.

#include <string_view>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/match_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

inline constexpr double RECOVERY_PER_GAP = 20;

inline double fitnessOf(const Player& p) { return p.fitness.value_or(100); }
inline bool isInjured(const Player& p) { return p.injury && p.injury->matches > 0; }
std::string_view fitnessLabel(double f);

struct Workload {
  std::vector<TeamId> teams;
  OrderedMap<PlayerId, double> loads;
  int32_t seed = 0;
};
// How much a match took out of each player who played (0-40ish)
Workload matchWorkload(const Match& match);

double injuryChance(const Player& p, double load);

// After a match: both squads recover from the gap, the players who played lose fitness, some may
// be injured, injured players count down one match. Returns the new injuries.
std::vector<InjuryNews> applyMatchFitness(World& world, const Workload& workload);
std::vector<InjuryNews> applyMatchFitness(World& world, const Workload& workload, int year);

// Off-season: everyone is rested and patched up
void offseasonRecovery(World& world);

// The player as he takes the field: tiredness takes the edge off pace, running and sharpness
Player applyFatigue(const Player& p);

}  // namespace t20
