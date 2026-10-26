#pragma once
// Training and player development (development.js).
//  - Training: before a match up to 3 players do a drill: the drilled skills are sharper for that
//    match only, and they walk out a little more confident.
//  - Development: after each match every performance is rated -1..+1 against what's normal for his
//    batting position and rating; skills move by up to ±0.5 a match, capped at ±8% (min 3 points).

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"
#include "engine/types.hpp"

namespace t20 {

inline constexpr int MAX_TRAINING_SLOTS = 3;
inline constexpr double TRAINING_CONFIDENCE = 4;
inline constexpr double STEP = 0.5;
inline constexpr double CAP = 0.08;
inline constexpr double CAPTAIN_GAIN = 1.5;

struct Drill {
  std::string_view key, label, area;  // area: Batting / Bowling / Fielding
  std::string_view type;              // "pace" / "spin" / ""
  std::vector<std::pair<BatK, double>> bat;
  std::vector<std::pair<BowlK, double>> bowl;
  std::vector<std::pair<FieldK, double>> field;
  std::string_view help;
};
extern const std::vector<Drill> DRILLS;
const Drill* drillByKey(std::string_view key);
std::vector<const Drill*> drillsFor(const Player& p);

struct TrainingSlot {
  PlayerId playerId;
  std::string drill;
};

// A copy of the player with the drill's boosts applied (for one match)
Player applyTraining(const Player& p, std::string_view drillKey);
// The AI coach's plan: best batter, the bowler who suits the pitch, and a death bowler
std::vector<TrainingSlot> aiTrainingPlan(const std::vector<const Player*>& players, Pitch pitch);

double expectedRuns(double bat, int k);
double expectedSR(double bat, int k);
double expectedEcon(double bowl);
std::optional<double> battingPerformance(const BatterInnings* b, int orderIndex, double rating);
std::optional<double> bowlingPerformance(const BowlingFigures* f, double rating);
std::optional<std::string_view> performanceLabel(std::optional<double> v);

// baseOf(id): the player's own (undeveloped) skills, or nullptr
using BaseOf = std::function<const Skills*(const PlayerId&)>;
// Update development for everyone in a completed match; returns the per-player reports
std::vector<DevReport> developFromMatch(OrderedMap<PlayerId, Progress>& progress, const Match& match,
                                        const BaseOf& baseOf);

// The player as he is now: his own skills plus development, headline ratings recomputed
Player applyProgress(const Player& p, const Progress* prog);

// Headline ratings recomputed from the skills (bat, bowl, field, power, awareness)
void recomputeRatings(Player& p);

}  // namespace t20
