#pragma once
// Manager achievements (achievements.js): match achievements unlocked by the user's club's games
// (captained or quick-simmed) and kept in career.manager; season achievements read from the season
// history and the board. Display only.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"
#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

std::optional<int> managedSide(const Match& match);
std::vector<std::string> matchFeats(const Match& match, int side);
// Add one completed match of the user's club to the record; returns the keys unlocked
std::vector<std::string> updateManager(Career& career, const Match& match, const std::string& when = "Friendly");

struct AchievementState {
  std::string_view key, group, title, text;
  bool done = false;
  std::optional<std::string> when;
  std::optional<TeamId> vs;
  std::optional<int> progress, goal;
};
std::vector<AchievementState> achievementList(const ManagerRecord& manager, const std::vector<SeasonArchive>& history,
                                              const std::vector<BoardEntry>& board);

}  // namespace t20
