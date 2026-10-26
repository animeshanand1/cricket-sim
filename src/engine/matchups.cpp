#include "engine/matchups.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "core/str.hpp"

namespace t20 {

MatchupBook matchupsOf(const Match& match) {
  MatchupBook out;
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const Over& over : inn.overs) {
      for (const Ball& b : over.balls) {
        // a run out can dismiss the non-striker: `faced` is who took the ball
        const std::optional<PlayerId>& batId = b.faced ? b.faced : b.bat;
        if (!batId) continue;  // wides
        MatchupRow& row = out[matchupKey(*batId, over.bowler)];
        if (!b.wide) row[0] += 1;  // stumped off a wide: a wicket, but not a ball faced
        row[1] += b.runs;
        if (b.kind == BallKind::wicket && b.wicketType != Out::runout) row[2] += 1;
        if (b.runs == 4) row[3] += 1;
        if (b.runs == 6) row[4] += 1;
      }
    }
  }
  return out;
}

void recordMatchups(MatchupBook& saved, const Match& match) {
  for (const auto& [k, row] : matchupsOf(match)) {
    MatchupRow& s = saved[k];
    for (size_t i = 0; i < row.size(); ++i) s[i] += row[i];
  }
}

namespace {
constexpr double FULL_BALLS = 36;  // a history this long counts in full
}

OrderedMap<std::string, double> matchupEdges(const MatchupBook* saved, const std::array<std::vector<PlayerId>, 2>& ids) {
  OrderedMap<std::string, double> edges;
  if (!saved) return edges;
  std::unordered_set<PlayerId> inMatch;
  for (const auto& side : ids) inMatch.insert(side.begin(), side.end());
  std::unordered_map<PlayerId, std::array<double, 3>> bat, bowl;
  for (const auto& [k, row] : *saved) {
    const auto bar = k.find('|');
    const PlayerId b = k.substr(0, bar);
    const PlayerId w = k.substr(bar + 1);
    if (!inMatch.count(b) && !inMatch.count(w)) continue;
    auto& x = bat[b];
    auto& y = bowl[w];
    for (size_t i = 0; i < 3; ++i) {
      x[i] += row[i];
      y[i] += row[i];
    }
  }
  for (size_t s = 0; s < 2; ++s) {
    for (const PlayerId& b : ids[s]) {
      for (const PlayerId& w : ids[1 - s]) {
        const MatchupRow* row = saved->find(matchupKey(b, w));
        if (!row || (*row)[0] < MIN_BALLS) continue;
        const double n = (*row)[0], runs = (*row)[1], outs = (*row)[2];
        const auto& bb = bat[b];
        const auto& ww = bowl[w];
        const double expSR = ((bb[1] / bb[0] + ww[1] / ww[0]) / 2) * 100;
        const double expOuts = n * ((bb[2] / bb[0] + ww[2] / ww[0]) / 2);
        // dismissals are judged against the noise expected over that many balls
        const double d = clamp(((runs * 100) / n - expSR) / 90 + (expOuts - outs) / (1.2 + 1.2 * std::sqrt(expOuts)), -1, 1);
        const double e = js_round(d * std::min(1.0, n / FULL_BALLS) * 100) / 100;
        if (std::abs(e) >= 0.05) edges.set(matchupKey(b, w), e);
      }
    }
  }
  return edges;
}

EdgeEffect edgeEffect(double e) { return {js_exp(-0.14 * e), js_exp(0.06 * e)}; }

std::string_view edgeVerdict(double e) { return e >= 0.25 ? "batter" : e <= -0.25 ? "bowler" : ""; }

std::optional<HeadToHead> headToHead(const MatchupBook* saved, const MatchupBook* live, const PlayerId& bat,
                                     const PlayerId& bowl) {
  const std::string k = matchupKey(bat, bowl);
  const MatchupRow* a = saved ? saved->find(k) : nullptr;
  const MatchupRow* b = live ? live->find(k) : nullptr;
  if (!a && !b) return std::nullopt;
  std::array<int, 5> v{};
  for (size_t i = 0; i < 5; ++i) v[i] = (a ? (*a)[i] : 0) + (b ? (*b)[i] : 0);
  return HeadToHead{v[0], v[1], v[2], v[3], v[4], v[0] ? (v[1] * 100.0) / v[0] : 0};
}

std::string h2hText(const std::optional<HeadToHead>& h) {
  if (!h) return "first meeting";
  std::string outs;
  if (h->outs)
    outs = cat(", out ", h->outs == 1 ? std::string("once") : h->outs == 2 ? std::string("twice") : cat(h->outs, " times"));
  else
    outs = ", not out";
  return cat(h->runs, " off ", h->balls, outs);
}

}  // namespace t20
