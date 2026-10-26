#pragma once

#include "core/types.hpp"
#include <string>
#include <optional>
#include <functional>

namespace t20::ui {

struct SeasonNavInfo {
  int seasonNumber = 1;
  int tablePosition = 1;
  int points = 0;
};

struct NavState {
  View currentView = View::home;
  bool hasActiveMatch = false;
  bool isMatchLive = false; // if true, draws pulsating red dot next to Match
  std::optional<SeasonNavInfo> seasonInfo;
};

// Render the sticky top navigation bar
// Returns a new View if a link was clicked, or std::nullopt
std::optional<View> navBar(const NavState& state, float screenWidth);

} // namespace t20::ui
