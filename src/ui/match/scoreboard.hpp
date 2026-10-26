#pragma once

#include "core/types.hpp"
#include "ui/match/win_and_fans.hpp"
#include <optional>
#include <string>
#include <vector>

namespace t20::ui {

struct ScoreboardBatter {
  std::string name;
  int runs = 0;
  int balls = 0;
  int fours = 0;
  int sixes = 0;
  bool isStriker = false;
};

struct ScoreboardBowler {
  std::string name;
  int legalBalls = 0;
  int maidens = 0;
  int runs = 0;
  int wickets = 0;
};

struct ScoreboardStat {
  std::string label;  // "Run rate", "Partnership", "Target", "Req. rate"
  std::string value;
  bool gold = false;
};

struct ScoreboardViewData {
  TeamMeta battingTeam;
  TeamMeta bowlingTeam;
  std::string inningsLabel = "1st innings";
  int runs = 0;
  int wickets = 0;
  int balls = 0;
  int targetRuns = 0; // if chasing
  std::string headline;
  ScoreboardBatter striker;
  ScoreboardBatter nonStriker;
  ScoreboardBowler currentBowler;
  std::vector<BallPipData> currentOverPips;

  std::vector<ScoreboardStat> stats;   // shown beside the score
  std::string overLabel = "This over"; // or "Last over"
  int overRuns = 0;
  std::vector<std::string> tags;       // phase, conditions notes
  bool freeHit = false;
  std::optional<float> battingWinPct;  // none before the toss / while computing
  bool winPending = false;             // first estimate still running: keep the row, say so
  std::optional<FansSideData> fansHome, fansAway;
  int groundCapacity = 0;
  std::string conditions;              // "Flat deck · pitch wear 22% · dew expected"
};

// Draw broadcast TV-style scoreboard with custom ImDrawList graphics (at the cursor, full width)
void drawScoreboard(void* drawList, const ScoreboardViewData& data, float width);

} // namespace t20::ui
