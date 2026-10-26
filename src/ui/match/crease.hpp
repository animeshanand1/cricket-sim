#pragma once
// S05 §6 At the crease (Crease.jsx): both batter cards, the striker's mind line, the bowler card
// with plan, rhythm and special delivery, and the last wicket.
//
// The chase lists under each card ("14 runs to 1,000 career runs", "9 to beat his best score")
// come from the career (milestones.js playerChases); without one (a quick friendly) they are empty,
// and they go quiet in a Super Over and once the match has been recorded.

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"

namespace t20::ui {

void drawCrease(const Match& m, const Career* career = nullptr);

}  // namespace t20::ui
