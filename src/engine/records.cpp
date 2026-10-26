#include "engine/records.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"

namespace t20 {

namespace {
constexpr size_t N = 10;

template <class T, class Cmp>
void add(std::vector<T>& list, T item, Cmp cmp) {
  list.push_back(std::move(item));
  std::stable_sort(list.begin(), list.end(), [&](const T& a, const T& b) { return cmp(a, b) < 0; });
  if (list.size() > N) list.resize(N);
}

// Balls a batter took to reach `target` runs (no-balls count as balls faced)
std::optional<int> ballsTo(const Innings& inn, const PlayerId& id, int target) {
  int runs = 0, balls = 0;
  for (const Over& o : inn.overs) {
    for (const Ball& b : o.balls) {
      if (b.bat != id) continue;
      if (b.kind != BallKind::extra || b.label.find("nb") != std::string::npos) balls += 1;
      if (!b.legBye) runs += b.runs;
      if (runs >= target) return balls;
    }
  }
  return std::nullopt;
}
}  // namespace

void updateRecords(Records& rec, const Match& match, const std::string& when) {
  if (match.status != MatchStatus::complete) return;
  std::vector<const Innings*> main;
  for (const auto& i : match.innings)
    if (!i.superOver) main.push_back(&i);
  auto team = [&](int side) { return match.teams[static_cast<size_t>(side)].id; };
  RecBase base{match.id, when};
  for (const Innings* inn : main) {
    const TeamId t = team(inn->batting);
    const TeamId vs = team(inn->bowlingSide);
    for (const PlayerId& id : inn->battedOrder) {
      const BatterInnings& b = inn->batters.at(id);
      if (b.balls == 0 && b.runs == 0) continue;
      if (b.runs >= 30) {
        ScoreRec r;
        static_cast<RecBase&>(r) = base;
        r.id = id, r.team = t, r.vs = vs, r.runs = b.runs, r.balls = b.balls, r.notOut = !b.out, r.fours = b.fours,
        r.sixes = b.sixes;
        add(rec.highScores, r, [](const ScoreRec& a, const ScoreRec& c) {
          const int d = c.runs - a.runs;
          return d ? d : a.balls - c.balls;
        });
      }
      if (b.sixes >= 3) {
        SixesRec r;
        static_cast<RecBase&>(r) = base;
        r.id = id, r.team = t, r.vs = vs, r.sixes = b.sixes, r.runs = b.runs, r.balls = b.balls;
        add(rec.mostSixes, r, [](const SixesRec& a, const SixesRec& c) {
          const int d = c.sixes - a.sixes;
          return d ? d : c.runs - a.runs;
        });
      }
      auto fast = [&](std::vector<FastRec>& list, int target) {
        const auto f = ballsTo(*inn, id, target);
        if (!f || !*f) return;
        FastRec r;
        static_cast<RecBase&>(r) = base;
        r.id = id, r.team = t, r.vs = vs, r.balls = *f, r.runs = b.runs;
        add(list, r, [](const FastRec& a, const FastRec& c) {
          const int d = a.balls - c.balls;
          return d ? d : c.runs - a.runs;
        });
      };
      if (b.runs >= 50) fast(rec.fastestFifties, 50);
      if (b.runs >= 100) fast(rec.fastestHundreds, 100);
    }
    for (const PlayerId& id : inn->bowlingOrder) {
      const BowlingFigures& f = inn->bowling.at(id);
      if (f.wickets >= 3) {
        BowlRec r;
        static_cast<RecBase&>(r) = base;
        r.id = id, r.team = vs, r.vs = t, r.wickets = f.wickets, r.runs = f.runs, r.balls = f.balls;
        add(rec.bestBowling, r, [](const BowlRec& a, const BowlRec& c) {
          const int d = c.wickets - a.wickets;
          return d ? d : a.runs - c.runs;
        });
      }
    }
    const bool full = inn->wickets >= 10 || inn->legalBalls >= inn->maxBalls;
    TotalRec total;
    static_cast<RecBase&>(total) = base;
    total.team = t, total.vs = vs, total.runs = inn->runs, total.wickets = inn->wickets, total.balls = inn->legalBalls;
    total.overs = inn->maxBalls / 6.0;
    add(rec.highTotals, total, [](const TotalRec& a, const TotalRec& c) { return c.runs - a.runs; });
    if (full && !(inn->target && inn->runs >= *inn->target))
      add(rec.lowTotals, total, [](const TotalRec& a, const TotalRec& c) { return a.runs - c.runs; });
  }
  // Margins of victory
  const auto& r = match.result;
  if (r && r->winner && !r->superOver && main.size() >= 2) {
    const Innings& first = *main[0];
    const Innings& second = *main[1];
    const TeamId w = team(*r->winner);
    const TeamId l = team(1 - *r->winner);
    const bool abandoned = match.dls && match.dls->abandoned;
    const int target = second.target.value_or(0);
    if (*r->winner == first.batting && !abandoned) {
      MarginRec m;
      static_cast<RecBase&>(m) = base;
      m.team = w, m.vs = l, m.margin = target - 1 - second.runs;
      add(rec.bigWinsRuns, m, [](const MarginRec& a, const MarginRec& c) { return c.margin - a.margin; });
    } else if (*r->winner == second.batting && second.target && second.runs >= target) {
      MarginRec m;
      static_cast<RecBase&>(m) = base;
      m.team = w, m.vs = l, m.margin = 10 - second.wickets, m.ballsLeft = second.maxBalls - second.legalBalls;
      add(rec.bigWinsWkts, m, [](const MarginRec& a, const MarginRec& c) {
        const int d = c.margin - a.margin;
        return d ? d : *c.ballsLeft - *a.ballsLeft;
      });
    }
  }
}

std::vector<HallEntry> hallOfFame(const World& world, const OrderedMap<PlayerId, CareerStats>& careerPlayers) {
  static const CareerStats empty;
  std::vector<HallEntry> out;
  for (const auto& [id, p] : world.players) {
    if (!p.retired || p.retired->quiet) continue;
    const CareerStats* s = careerPlayers.find(id);
    if (!s) s = &empty;
    double peak = 0;
    for (const auto& h : p.history) {
      const double v = p.role == Role::BOWL ? h.bowl : p.role == Role::AR ? (h.bat + h.bowl) / 2 : h.bat;
      peak = std::max(peak, v);
    }
    HallEntry e;
    if (s->bat.runs >= 1200) e.reasons.push_back(cat(s->bat.runs, " runs"));
    if (s->bowl.wickets >= 50) e.reasons.push_back(cat(s->bowl.wickets, " wickets"));
    if (s->bat.runs >= 700 && s->bowl.wickets >= 30) e.reasons.push_back("all-round career");
    if (s->potm >= 8) e.reasons.push_back(cat(s->potm, " player of the match awards"));
    e.legend = peak >= 85;
    if (e.reasons.empty() && !e.legend) continue;
    e.id = id;
    e.team = p.retired->team;
    e.year = p.retired->year;
    e.age = p.retired->age;
    e.peak = js_round(peak);
    e.stats = s;
    out.push_back(std::move(e));
  }
  std::stable_sort(out.begin(), out.end(), [](const HallEntry& a, const HallEntry& b) {
    const double d = static_cast<double>(b.reasons.size()) - static_cast<double>(a.reasons.size());
    return (d ? d : b.peak - a.peak) < 0;
  });
  return out;
}

}  // namespace t20
