#pragma once
// The board (board.js): a target before each season from the squad's strength, a verdict on the
// final position, and confidence that moves with results. Lose it all and you're sacked.

#include <string_view>
#include <vector>

#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

struct TargetDef {
  std::string_view key, label;
  int need;
};
extern const std::vector<TargetDef> TARGETS;
inline constexpr double SACK_AT = 15;

int strengthRank(const std::vector<Team>& league, const TeamId& teamId);
const TargetDef& boardTarget(int rank);
std::string_view boardConfidenceLabel(double c);

struct TableRow;  // season.hpp
int finalPosition(const Season& season, const std::vector<TableRow>& table, const TeamId& teamId);

struct Verdict {
  double delta;
  std::string_view verdict;
};
Verdict seasonVerdict(const Target& target, int position);
inline double matchNudge(bool won, bool tied) { return tied ? 0 : won ? 1.5 : -1.5; }
void applyConfidence(Manager& manager, double delta);
std::vector<TeamId> jobOffers(const std::vector<Team>& league, const TeamId& exclude);

}  // namespace t20
