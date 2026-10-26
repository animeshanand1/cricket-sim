#pragma once
// The crowd (fans.js): how many fans of each side are in the ground, ball by ball. Display only:
// it never changes what happens on the field and never draws from the match stream.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/match_types.hpp"

namespace t20 {

// Seats at each franchise's ground (club.js GROUND_CAPACITY; moves to club when that is ported)
double groundCapacity(const TeamId& id);

struct GroundInfo {
  std::optional<std::optional<TeamId>> home;  // unset = the first-named side; set to nullopt = neutral
  std::optional<double> capacity;
  bool big = false;
};
Fans createFans(const Match& match, const GroundInfo& ground = {});

// Move the crowd after a ball. Returns commentary notes for the big swings.
std::vector<std::string> updateFans(Match& match, int batRuns, bool wicket, bool overDone);

std::string_view fansMoodLabel(double mood);

}  // namespace t20
