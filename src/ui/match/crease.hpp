#pragma once
// S05 §6 At the crease (Crease.jsx): both batter cards, the striker's mind line, the bowler card
// with plan, rhythm and special delivery, and the last wicket.
//
// The JS chase lists ("14 runs to 1,000 career runs") need career stats and the records book,
// which arrive with the career save (A4, milestones.js); until then a friendly has none to show.

#include "engine/match_types.hpp"

namespace t20::ui {

void drawCrease(const Match& m);

}  // namespace t20::ui
