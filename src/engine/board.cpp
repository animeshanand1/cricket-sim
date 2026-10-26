#include "engine/board.hpp"

#include <algorithm>

#include "engine/league.hpp"
#include "engine/season.hpp"

namespace t20 {

const std::vector<TargetDef> TARGETS = {
    {"title", "Win the title", 1},          {"final", "Reach the final", 2},
    {"playoffs", "Make the playoffs", 4},   {"top6", "Finish in the top six", 6},
    {"top8", "Finish in the top eight", 8},
};

namespace {
double strengthScore(const Team& t) {
  const TeamRatings r = teamRatings(t);
  return r.batting + r.bowling;
}
}  // namespace

int strengthRank(const std::vector<Team>& league, const TeamId& teamId) {
  std::vector<const Team*> sorted;
  for (const auto& t : league) sorted.push_back(&t);
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const Team* a, const Team* b) { return strengthScore(*b) - strengthScore(*a) < 0; });
  for (size_t i = 0; i < sorted.size(); ++i)
    if (sorted[i]->id == teamId) return static_cast<int>(i) + 1;
  return 0;  // indexOf -1 + 1
}

const TargetDef& boardTarget(int rank) {
  if (rank <= 1) return TARGETS[1];
  if (rank <= 3) return TARGETS[2];
  if (rank <= 6) return TARGETS[3];
  return TARGETS[4];
}

std::string_view boardConfidenceLabel(double c) {
  if (c >= 80) return "Delighted";
  if (c >= 60) return "Happy";
  if (c >= 40) return "Watching closely";
  if (c >= 25) return "Concerned";
  return "Your job is at risk";
}

int finalPosition(const Season& season, const std::vector<TableRow>& table, const TeamId& teamId) {
  if (season.champion == teamId) return 1;
  if (season.runnerUp == teamId) return 2;
  auto find = [&](Stage s) -> const Fixture* {
    for (const auto& f : season.fixtures)
      if (f.stage == s) return &f;
    return nullptr;
  };
  auto loser = [](const Fixture* fx) -> std::optional<TeamId> {
    if (!fx || !fx->result) return std::nullopt;
    return fx->result->winner == fx->home ? fx->away : fx->home;
  };
  if (loser(find(Stage::q2)) == teamId) return 3;
  if (loser(find(Stage::elim)) == teamId) return 4;
  for (size_t i = 0; i < table.size(); ++i)
    if (table[i].team == teamId) return static_cast<int>(i) + 1;
  return 0;
}

Verdict seasonVerdict(const Target& target, int position) {
  const int margin = target.need - position;
  double delta;
  if (position == 1)
    delta = 32;
  else if (margin >= 2)
    delta = 22;
  else if (margin >= 0)
    delta = 12;
  else if (margin >= -2)
    delta = -18;
  else
    delta = -32;
  const std::string_view verdict = margin >= 0
                                       ? (margin >= 2 || position == 1 ? "Exceeded expectations" : "Target met")
                                       : margin >= -2 ? "Target missed"
                                                      : "Well short of the target";
  return {delta, verdict};
}

void applyConfidence(Manager& manager, double delta) {
  manager.confidence = clamp(js_round((manager.confidence + delta) * 10) / 10, 0, 100);
}

std::vector<TeamId> jobOffers(const std::vector<Team>& league, const TeamId& exclude) {
  std::vector<const Team*> rest;
  for (const auto& t : league)
    if (t.id != exclude) rest.push_back(&t);
  std::stable_sort(rest.begin(), rest.end(),
                   [](const Team* a, const Team* b) { return strengthScore(*a) - strengthScore(*b) < 0; });
  std::vector<TeamId> out;
  for (size_t i = 0; i < rest.size() && i < 3; ++i) out.push_back(rest[i]->id);
  return out;
}

}  // namespace t20
