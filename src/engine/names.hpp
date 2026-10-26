#pragma once
// Invented names for new players (academy prospects, draft picks, free agents) (names.js).

#include <string>
#include <unordered_set>

#include "engine/rng.hpp"

namespace t20 {

// A new name not already in `used`. Adds it to the set.
std::string freshName(RandFn rng, std::unordered_set<std::string>& used);

}  // namespace t20
