#pragma once
// S06 §3-5: what the Match screen shows above the scoreboard once a match is complete: the result
// banner (Victory / Defeat / Tied, the result, player of the match, the way out), the milestones
// and achievements this match brought, and the development report.

#include <string>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"
#include "engine/season_types.hpp"
#include "engine/world_types.hpp"
#include "ui/match/captain_panel.hpp"

namespace t20::ui {

void drawResultBanner(const Match& m, const std::vector<MatchExit>& exits);

// Only when this match reached a milestone or unlocked an achievement (career.lastMilestones /
// lastUnlocked for this match id)
void drawMatchMilestones(const Match& m, const Career& career, const std::vector<SeasonArchive>& history,
                         const std::vector<BoardEntry>& board);

struct DevReportState {
  int side = -1;  // -1: the user's side
};
// From career.lastReport for this match id
void drawDevelopmentReport(const Match& m, const Career& career, DevReportState& state);

}  // namespace t20::ui
