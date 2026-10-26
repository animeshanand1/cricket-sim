#pragma once
// S06 §8 The match charts (Worm.jsx, Manhattan.jsx, WinProb.jsx), drawn with ImDrawList:
//  - Worm: cumulative runs by over per innings, wickets as dots, the target, the powerplay band,
//    end labels, a hover crosshair, and an over-by-over table
//  - Manhattan: runs per over as paired bars, wicket dots above them, hover band
//  - Win chance: the side batting first's chance across the match from the session's checkpoints

#include <vector>

#include "app/match_session.hpp"
#include "engine/match_types.hpp"

namespace t20::ui {

void drawWorm(const Match& m, float width, float height);
void drawManhattan(const Match& m, float width, float height);
void drawWinChart(const Match& m, const std::vector<app::MatchSession::WinPoint>& points, float width, float height);

}  // namespace t20::ui
