#include "engine/popularity.hpp"

#include <algorithm>

namespace t20 {

namespace {
double round1(double v) { return js_round(v * 10) / 10; }
}  // namespace

double startPopularity(const Player* p) {
  const double best = std::max(p ? p->bat : 0, p ? p->bowl : 0);
  return clamp(18 + (best - 55) * 1.3, 5, 65);
}

OrderedMap<PlayerId, Appeal> matchAppeal(const Match& match) {
  OrderedMap<PlayerId, Appeal> out;
  for (int side = 0; side < 2; ++side) {
    const MatchTeam& team = match.teams[static_cast<size_t>(side)];
    std::vector<PlayerId> ids = team.players;
    if (team.impact) ids.push_back(team.impact->out);
    for (const auto& id : ids) {
      if (match.result && match.result->winner == side)
        out[id].bonus += 4;
      else
        out[id];
    }
  }
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings& b = inn.batters.at(id);
      double pts = b.runs + b.fours + 2 * b.sixes + (b.runs >= 100 ? 25 : b.runs >= 50 ? 10 : 0);
      if (b.balls >= 10) pts += clamp(((b.runs * 100.0) / b.balls - 130) / 10, -5, 5);
      out[id].bat += pts;
      const auto& d = b.dismissal;
      if (d && d->fielder && d->type != Out::caughtBowled) out[*d->fielder].field += 7;
    }
    for (const DropRecord& d : inn.drops) out[d.fielder].field -= 6;
    for (const PlayerId& id : inn.bowlingOrder) {
      const BowlingFigures& f = inn.bowling.at(id);
      if (!f.balls) continue;
      double pts = f.wickets * 18 + f.dots * 0.8 + f.maidens * 8 + (f.wickets >= 4 ? 12 : 0);
      if (f.balls >= 12) pts += clamp((8 - (f.runs * 6.0) / f.balls) * 3, -10, 10);
      out[id].bowl += pts;
    }
  }
  if (match.potm) out[*match.potm].bonus += 20;
  for (auto& [id, a] : out) a.total = a.bat + a.bowl + a.field + a.bonus;
  return out;
}

void updatePopularity(Career& career, const Match& match) {
  auto& pop = career.popularity;
  for (const auto& [id, a] : matchAppeal(match)) {
    const Popularity* had = pop.find(id);
    const Player* mp = match.players.find(id);
    const double cur = had ? had->v : startPopularity(mp);
    const double raw = clamp((a.total - 12) / 12, -1.5, 7);
    const double d = raw >= 0 ? raw * std::min(1.2, (100 - cur) / 70) : raw * std::min(1.2, cur / 50);
    const double v = clamp(cur + d, 1, 99);
    pop.set(id, Popularity{round1(v), round1(v - cur)});
  }
}

std::vector<PopularRow> topPopular(const OrderedMap<PlayerId, Popularity>& popularity, size_t n,
                                   const std::function<bool(const PlayerId&)>& keep) {
  std::vector<PopularRow> rows;
  for (const auto& [id, x] : popularity)
    if (!keep || keep(id)) rows.push_back({id, x.v, x.d});
  std::stable_sort(rows.begin(), rows.end(), [](const PopularRow& a, const PopularRow& b) {
    const double d = b.v - a.v;
    return (d ? d : (a.id < b.id ? -1 : 1)) < 0;
  });
  if (rows.size() > n) rows.resize(n);
  return rows;
}

}  // namespace t20
