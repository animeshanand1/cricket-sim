#pragma once
// Career statistics (career.js): aggregates completed matches into per-player records, and runs
// everything else that happens after a match: records book, match-ups, popularity, development,
// recent form, milestones and the manager's achievements.

#include <optional>
#include <string>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/development.hpp"
#include "engine/match_types.hpp"

namespace t20 {

inline constexpr size_t RECENT_LIMIT = 5;
inline constexpr size_t RECORDED_LIMIT = 500;

// Add one completed match to the career. Returns false if it was skipped (not complete, or
// already recorded). baseOf: the players' own skills for development (null: no development).
bool recordMatch(Career& career, const Match& match, const BaseOf* baseOf = nullptr);

struct CareerSummary {
  const CareerStats* s;
  std::optional<double> batAvg, batSR, bowlAvg, econ, bowlSR;
  std::string hsText, overs, bestText;
};
CareerSummary summarize(const CareerStats* s);
std::string fmt(std::optional<double> v, int digits = 2);

struct Form {
  double bonus = 0;
  std::string label;
  std::vector<int> scores;
};
// Recent batting form from the last three innings: feeds a player's starting confidence
Form formOf(const CareerStats* stats);

}  // namespace t20
