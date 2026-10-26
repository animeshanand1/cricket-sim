#pragma once
// The records book (records.js): top-10 lists of the best (and worst) performances across every
// match, updated after each one. Super Overs don't count. Plus the hall of fame.

#include <functional>
#include <string>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

void updateRecords(Records& rec, const Match& match, const std::string& when);

struct HallEntry {
  PlayerId id;
  std::optional<TeamId> team;
  int year = 0, age = 0;
  double peak = 0;
  std::vector<std::string> reasons;
  bool legend = false;
  const CareerStats* stats = nullptr;
};
std::vector<HallEntry> hallOfFame(const World& world, const OrderedMap<PlayerId, CareerStats>& careerPlayers);

}  // namespace t20
