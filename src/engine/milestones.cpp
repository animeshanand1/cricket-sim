#include "engine/milestones.hpp"

#include <algorithm>

#include "core/str.hpp"

namespace t20 {

namespace {

struct Ladder {
  std::string_view type;
  std::vector<int> steps;
  int near;
  std::string_view one, many;
};
const std::vector<Ladder>& LADDERS() {
  static const std::vector<Ladder> l = {
      {"runs", {250, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 7500, 10000}, 40, "run", "runs"},
      {"wickets", {10, 25, 50, 75, 100, 150, 200, 250, 300}, 3, "wicket", "wickets"},
      {"sixes", {25, 50, 100, 150, 200, 300, 400}, 3, "six", "sixes"},
      {"fours", {50, 100, 200, 300, 400, 500}, 5, "four", "fours"},
      {"catches", {10, 25, 50, 75, 100}, 2, "catch", "catches"},
  };
  return l;
}
const Ladder& ladder(std::string_view type) {
  for (const auto& l : LADDERS())
    if (l.type == type) return l;
  return LADDERS()[0];
}
constexpr int BEST_SCORE_REACH = 20;

std::string_view units(std::string_view type, int n) { return n == 1 ? ladder(type).one : ladder(type).many; }

std::vector<int> crossed(std::string_view type, int before, int after) {
  std::vector<int> out;
  for (int s : ladder(type).steps)
    if (before < s && s <= after) out.push_back(s);
  return out;
}
std::optional<int> nextStep(std::string_view type, int value) {
  for (int s : ladder(type).steps)
    if (s > value) return s;
  return std::nullopt;
}
int appearanceIndex(int v) {
  for (size_t i = 0; i < std::size(APPEARANCES); ++i)
    if (APPEARANCES[i] == v) return static_cast<int>(i);
  return -1;
}
int stepIndex(std::string_view type, int v) {
  const auto& s = ladder(type).steps;
  auto it = std::find(s.begin(), s.end(), v);
  return it == s.end() ? -1 : static_cast<int>(it - s.begin());
}

}  // namespace

int Counts::get(std::string_view type) const {
  if (type == "runs") return runs;
  if (type == "wickets") return wickets;
  if (type == "sixes") return sixes;
  if (type == "fours") return fours;
  return catches;
}

Counts careerCounts(const CareerStats* s) {
  if (!s) return {};
  return {s->bat.runs, s->bowl.wickets, s->bat.sixes, s->bat.fours, s->field.catches};
}

OrderedMap<PlayerId, Counts> matchCounts(const Match& match) {
  OrderedMap<PlayerId, Counts> out;
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings& b = inn.batters.at(id);
      Counts& c = out[id];
      c.runs += b.runs;
      c.sixes += b.sixes;
      c.fours += b.fours;
      const auto& d = b.dismissal;
      if (d && d->fielder && d->type != Out::stumped && d->type != Out::runout) out[*d->fielder].catches += 1;
    }
    for (const PlayerId& id : inn.bowlingOrder) out[id].wickets += inn.bowling.at(id).wickets;
  }
  return out;
}

std::string milestoneText(std::string_view type, int value) {
  if (type == "matches") return cat(value, " matches");
  if (type == "hundred") return "First hundred";
  return cat(fmt_thousands(value), " career ", ladder(type).many);
}

OrderedMap<PlayerId, Snap> snapshot(const OrderedMap<PlayerId, CareerStats>& careerPlayers,
                                    const std::vector<PlayerId>& ids) {
  OrderedMap<PlayerId, Snap> out;
  for (const auto& id : ids) {
    const CareerStats* s = careerPlayers.find(id);
    out.set(id, Snap{careerCounts(s), s ? s->matches : 0, s ? s->bat.hundreds : 0});
  }
  return out;
}

std::vector<MilestoneHit> milestonesReached(const OrderedMap<PlayerId, Snap>& before,
                                            const OrderedMap<PlayerId, CareerStats>& careerPlayers, const Match& match) {
  std::vector<MilestoneHit> out;
  for (const MatchTeam& team : match.teams) {
    std::vector<PlayerId> ids = team.players;
    if (team.impact) ids.push_back(team.impact->out);
    for (const auto& id : ids) {
      const Snap* was = before.find(id);
      const CareerStats* s = careerPlayers.find(id);
      if (!was || !s) continue;
      const Counts now = careerCounts(s);
      for (const auto& l : LADDERS())
        for (int value : crossed(l.type, was->c.get(l.type), now.get(l.type)))
          out.push_back({id, team.id, std::string(l.type), value, std::nullopt});
      if (appearanceIndex(s->matches) >= 0 && was->matches < s->matches)
        out.push_back({id, team.id, "matches", s->matches, std::nullopt});
      if (was->hundreds == 0 && s->bat.hundreds > 0) out.push_back({id, team.id, "hundred", 1, std::nullopt});
    }
  }
  auto weight = [](const MilestoneHit& x) {
    if (x.type == "hundred") return 2;
    if (x.type == "matches") return appearanceIndex(x.value);
    return stepIndex(x.type, x.value);
  };
  std::stable_sort(out.begin(), out.end(),
                   [&](const MilestoneHit& a, const MilestoneHit& b) { return weight(b) - weight(a) < 0; });
  return out;
}

std::vector<Chase> playerChases(const ChaseInput& in) {
  struct W {
    Chase c;
    double w;
  };
  const Counts before = careerCounts(in.stats);
  const Counts now = in.inMatch ? *in.inMatch : Counts{};
  std::vector<W> out;
  std::vector<std::string_view> types;
  if (in.as == "bat") types = {"runs", "sixes", "fours"};
  if (in.as == "bowl") types = {"wickets"};
  for (auto type : types) {
    const int total = before.get(type) + now.get(type);
    const auto done = crossed(type, before.get(type), total);
    if (!done.empty()) out.push_back({{"reached", std::string(type), cat("Reached ", milestoneText(type, done.back()))}, 0});
    const auto next = nextStep(type, total);
    if (next) {
      const int need = *next - total;
      if (need <= ladder(type).near)
        out.push_back({{"chase", std::string(type), cat(need, " ", units(type, need), " to ", milestoneText(type, *next))},
                       2 + static_cast<double>(need) / ladder(type).near});
    }
  }
  if (in.as == "bat" && in.bat && !in.bat->out) {
    if (in.records && !in.records->highScores.empty() && in.records->highScores[0].runs >= 50) {
      const int top = in.records->highScores[0].runs;
      const int need = top + 1 - in.bat->runs;
      if (need <= 0)
        out.push_back({{"reached", "record", cat("League record score (was ", top, ")")}, 0});
      else if (need <= BEST_SCORE_REACH)
        out.push_back({{"record", "record", cat(need, " to beat the league record score (", top, ")")}, 1});
    }
    const int hs = in.stats ? in.stats->bat.hs : 0;
    if ((in.stats ? in.stats->bat.inns : 0) >= 5 && hs >= 30) {
      const int need = hs + 1 - in.bat->runs;
      if (need <= 0)
        out.push_back({{"reached", "best", cat("Career-best score (was ", hs, ")")}, 0.5});
      else if (need <= BEST_SCORE_REACH)
        out.push_back({{"best", "best", cat(need, " to beat his best score (", hs, ")")}, 1.5});
    }
  }
  if (in.as == "bowl" && in.fig) {
    const int bestW = in.stats ? in.stats->bowl.bestW : 0;
    if ((in.stats ? in.stats->bowl.inns : 0) >= 5 && bestW >= 3) {
      if (in.fig->wickets > bestW)
        out.push_back({{"reached", "best", cat("Career-best haul (was ", bestW, ")")}, 0.5});
      else if (in.fig->wickets == bestW - 1 && in.fig->wickets >= 2)
        out.push_back({{"best", "best", cat("1 wicket to equal his best haul (", bestW, ")")}, 1.5});
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const W& a, const W& b) { return a.w - b.w < 0; });
  std::vector<Chase> res;
  for (size_t i = 0; i < out.size() && i < in.limit; ++i) res.push_back(out[i].c);
  return res;
}

std::vector<Reached> reachedInMatch(const OrderedMap<PlayerId, CareerStats>& careerPlayers, const Match& match) {
  std::vector<Reached> out;
  for (const auto& [id, now] : matchCounts(match)) {
    const Counts before = careerCounts(careerPlayers.find(id));
    for (const auto& l : LADDERS())
      for (int value : crossed(l.type, before.get(l.type), before.get(l.type) + now.get(l.type)))
        out.push_back({cat(id, ':', l.type, ':', value), id, std::string(l.type), value});
  }
  return out;
}

std::vector<Upcoming> upcoming(const OrderedMap<PlayerId, CareerStats>& careerPlayers, const std::vector<PlayerId>& ids,
                               double reach, size_t perPlayer) {
  struct R {
    Upcoming u;
    double ratio;
  };
  std::vector<R> out;
  for (const auto& id : ids) {
    const CareerStats* s = careerPlayers.find(id);
    std::vector<R> mine;
    const int played = s ? s->matches : 0;
    if (appearanceIndex(played + 1) >= 0)
      mine.push_back({{id, "matches", 1, played + 1, cat("Plays his ", played + 1, "th match")}, 0});
    const Counts c = careerCounts(s);
    for (const auto& l : LADDERS()) {
      const auto target = nextStep(l.type, c.get(l.type));
      if (!target) continue;
      const int need = *target - c.get(l.type);
      const double ratio = static_cast<double>(need) / l.near;
      if (ratio <= reach)
        mine.push_back({{id, std::string(l.type), need, *target,
                         cat(need, " ", units(l.type, need), " to ", milestoneText(l.type, *target))},
                        ratio});
    }
    std::stable_sort(mine.begin(), mine.end(), [](const R& a, const R& b) { return a.ratio - b.ratio < 0; });
    for (size_t i = 0; i < mine.size() && i < perPlayer; ++i) out.push_back(mine[i]);
  }
  std::stable_sort(out.begin(), out.end(), [](const R& a, const R& b) { return a.ratio - b.ratio < 0; });
  std::vector<Upcoming> res;
  for (auto& r : out) res.push_back(r.u);
  return res;
}

std::optional<NextMilestone> nextMilestone(const CareerStats* stats) {
  const Counts c = careerCounts(stats);
  std::optional<NextMilestone> best;
  double bestDone = -1;
  for (const auto& l : LADDERS()) {
    const int v = c.get(l.type);
    const auto target = nextStep(l.type, v);
    if (!target) continue;
    int prev = 0;
    for (int s : l.steps)
      if (s <= v) prev = s;
    const double done = static_cast<double>(v - prev) / (*target - prev);
    if (!best || done > bestDone) {
      const int need = *target - v;
      best = NextMilestone{std::string(l.type), need, *target,
                           cat(need, " ", units(l.type, need), " to ", milestoneText(l.type, *target))};
      bestDone = done;
    }
  }
  return best;
}

}  // namespace t20
