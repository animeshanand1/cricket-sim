#pragma once

#include "core/types.hpp"
#include <string>

namespace t20::ui {

// Win probability bar: two flex segments in series colours with percentages on each side
void winBar(float battingWinPct, const std::string& battingTeamCode, const std::string& bowlingTeamCode);

struct FansSideData {
  TeamMeta team;
  int fansCount = 0;
  int trend = 0; // +1 gaining, 0 neutral, -1 leaving
};

// Fans meter: badge + count + trend arrow per side, two-colour bar, totals line
void fansMeter(const FansSideData& home, const FansSideData& away, int groundCapacity);

} // namespace t20::ui
