#include "engine/rules.hpp"

#include <cmath>

#include "core/str.hpp"

namespace t20 {

int bowlerQuota(const Innings* inn) {
  const double maxBalls = inn ? inn->maxBalls : BALLS;
  return std::min(MAX_OVERS_PER_BOWLER, static_cast<int>(std::ceil(maxBalls / 6 / 5)));
}

int inningsOvers(const Innings* inn) {
  const double maxBalls = inn ? inn->maxBalls : BALLS;
  return static_cast<int>(std::ceil(maxBalls / 6));
}

int over20(const Innings* inn, int over) {
  if (inn && !inn->superOver && inn->maxBalls < BALLS) return (over * BALLS) / inn->maxBalls;
  return over;
}

std::string_view phaseLabel(Phase p) {
  switch (p) {
    case Phase::pp: return "Powerplay";
    case Phase::mid: return "Middle overs";
    default: return "Death overs";
  }
}

std::string oversText(int balls) {
  if (balls % 6 == 0) return std::to_string(balls / 6);
  return cat(balls / 6, '.', balls % 6);
}

std::string ballTag(int balls) { return cat(balls / 6, '.', balls % 6 + 1); }

std::optional<double> requiredRate(const Innings& inn) {
  if (!inn.target) return std::nullopt;
  const int left = inn.maxBalls - inn.legalBalls;
  if (left <= 0) return std::nullopt;
  return (static_cast<double>(*inn.target - inn.runs) * 6) / left;
}

int oversBowledBy(const Innings& inn, const PlayerId& id) {
  const BowlingFigures* f = inn.bowling.find(id);
  return (f ? f->balls : 0) / 6;
}

bool canFinishOvers(const std::vector<std::pair<PlayerId, int>>& caps, const std::optional<PlayerId>& last,
                    int remaining) {
  std::vector<int> c;
  c.reserve(caps.size());
  int prev = -1;
  for (size_t i = 0; i < caps.size(); ++i) {
    c.push_back(caps[i].second);
    if (last && caps[i].first == *last) prev = static_cast<int>(i);
  }
  for (int i = 0; i < remaining; ++i) {
    int best = -1;
    for (int id = 0; id < static_cast<int>(c.size()); ++id) {
      if (id == prev || c[static_cast<size_t>(id)] <= 0) continue;
      if (best == -1 || c[static_cast<size_t>(id)] > c[static_cast<size_t>(best)]) best = id;
    }
    if (best == -1) return false;
    c[static_cast<size_t>(best)] -= 1;
    prev = best;
  }
  return true;
}

std::vector<BowlOption> bowlingOptions(const Match& match) {
  const Innings& inn = inningsOf(match);
  const MatchTeam& team = match.teams[static_cast<size_t>(inn.bowlingSide)];
  const int over = overIndex(inn);
  const int remainingAfter = inningsOvers(&inn) - over - 1;
  const int quota = bowlerQuota(&inn);
  std::vector<std::pair<PlayerId, int>> caps;
  for (const PlayerId& id : team.players)
    if (id != team.keeperId) caps.emplace_back(id, quota - oversBowledBy(inn, id));

  std::vector<BowlOption> out;
  out.reserve(caps.size());
  for (size_t i = 0; i < caps.size(); ++i) {
    const PlayerId& id = caps[i].first;
    const int bowled = oversBowledBy(inn, id);
    std::string reason;
    if (bowled >= quota) {
      reason = "Quota done";
    } else if (inn.lastOverBowler && id == *inn.lastOverBowler) {
      reason = "Bowled last over";
    } else {
      auto trial = caps;
      trial[i].second -= 1;
      if (!canFinishOvers(trial, id, remainingAfter)) reason = "Would leave overs unbowlable";
    }
    out.push_back({id, bowled, reason.empty(), reason});
  }
  return out;
}

}  // namespace t20
