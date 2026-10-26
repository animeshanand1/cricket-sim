#include "engine/season.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "core/str.hpp"
#include "engine/pitch.hpp"
#include "engine/rng.hpp"

namespace t20 {

std::string_view stageLabel(Stage s) {
  switch (s) {
    case Stage::league: return "League";
    case Stage::q1: return "Qualifier 1";
    case Stage::elim: return "Eliminator";
    case Stage::q2: return "Qualifier 2";
    case Stage::final_: return "Final";
  }
  return "";
}

namespace {

template <class T, class Cmp>
void jsSort(std::vector<T>& v, Cmp cmp) {
  std::stable_sort(v.begin(), v.end(), [&](const T& a, const T& b) { return cmp(a, b) < 0; });
}

double strength(const Team& t) {
  auto top = [](std::vector<double> arr, int n) {
    std::stable_sort(arr.begin(), arr.end(), [](double a, double b) { return b - a < 0; });
    double s = 0;
    for (int i = 0; i < n && i < static_cast<int>(arr.size()); ++i) s += arr[static_cast<size_t>(i)];
    return s / n;
  };
  std::vector<double> bats, bowls;
  for (const auto& p : t.squad) {
    bats.push_back(p.bat);
    bowls.push_back(p.bowl);
  }
  return top(bats, 7) + top(bowls, 5);
}

using Pair = std::pair<TeamId, TeamId>;

std::vector<Pair> scheduleOrder(const std::vector<Pair>& pairs, Rng& rng) {
  std::vector<Pair> left = pairs;
  for (int i = static_cast<int>(left.size()) - 1; i > 0; --i) {
    const int j = static_cast<int>(std::floor(rng() * (i + 1)));
    std::swap(left[static_cast<size_t>(i)], left[static_cast<size_t>(j)]);
  }
  std::vector<Pair> out;
  std::unordered_map<TeamId, int> played, lastDay;
  auto get = [](const std::unordered_map<TeamId, int>& m, const TeamId& k, int dflt) {
    auto it = m.find(k);
    return it == m.end() ? dflt : it->second;
  };
  while (!left.empty()) {
    size_t best = 0;
    double bestScore = -std::numeric_limits<double>::infinity();
    const int day = static_cast<int>(out.size());
    for (size_t k = 0; k < left.size(); ++k) {
      const auto& [a, b] = left[k];
      const int rest = std::min(day - get(lastDay, a, -9), day - get(lastDay, b, -9));
      const double score = std::min(rest, 4) * 10 - (get(played, a, 0) + get(played, b, 0)) * 3 + rng();
      if (score > bestScore) {
        bestScore = score;
        best = k;
      }
    }
    Pair pr = left[best];
    left.erase(left.begin() + static_cast<std::ptrdiff_t>(best));
    for (const TeamId& t : {pr.first, pr.second}) {
      played[t] = get(played, t, 0) + 1;
      lastDay[t] = day;
    }
    out.push_back(pr);
  }
  return out;
}

int32_t seedFrom(Rng& rng) { return static_cast<int32_t>(std::floor(rng() * 2147483648.0)); }

void addFixture(Season& season, Stage stage, const TeamId& home, const TeamId& away) {
  Rng rng(season.seed ^ js_imul(static_cast<int32_t>(season.fixtures.size()), 7919));
  const Venue* v = season.venues.find(home);
  Fixture fx;
  fx.no = static_cast<int>(season.fixtures.size()) + 1;
  fx.stage = stage;
  fx.home = home;
  fx.away = away;
  fx.venue = v ? v->ground : "Neutral venue";
  fx.pitch = v ? v->pitch : Pitch::balanced;
  fx.seed = seedFrom(rng);
  season.fixtures.push_back(std::move(fx));
}

const Fixture* stageFixture(const Season& s, Stage st) {
  for (const auto& f : s.fixtures)
    if (f.stage == st) return &f;
  return nullptr;
}
std::optional<TeamId> winnerOf(const Fixture* fx) {
  if (!fx || !fx->result) return std::nullopt;
  return fx->result->winner;
}
std::optional<TeamId> loserOf(const Fixture* fx) {
  if (!fx || !fx->result) return std::nullopt;
  return fx->result->winner == fx->home ? fx->away : fx->home;
}

std::string oversShort(int balls) { return balls % 6 ? cat(balls / 6, '.', balls % 6) : cat(balls / 6); }

}  // namespace

Season createSeason(int32_t seed, int number, const TeamId& userTeam, const std::vector<Team>& league) {
  Rng rng(seed);
  std::vector<const Team*> seeded;
  for (const auto& t : league) seeded.push_back(&t);
  std::vector<double> str;
  std::stable_sort(seeded.begin(), seeded.end(),
                   [](const Team* a, const Team* b) { return strength(*b) - strength(*a) < 0; });
  Season s;
  s.id = cat('S', number, '-', seed);
  s.number = number;
  s.seed = seed;
  s.userTeam = userTeam;
  static constexpr char snake[] = {'A', 'B', 'B', 'A'};
  for (size_t i = 0; i < seeded.size(); ++i) (snake[i % 4] == 'A' ? s.groupA : s.groupB).push_back(seeded[i]->id);

  std::vector<Pair> pairs;
  for (const auto* g : {&s.groupA, &s.groupB}) {
    const auto& ids = *g;
    for (size_t i = 0; i < ids.size(); ++i)
      for (size_t j = i + 1; j < ids.size(); ++j) pairs.push_back(rng() < 0.5 ? Pair{ids[i], ids[j]} : Pair{ids[j], ids[i]});
  }
  for (const auto& a : s.groupA)
    for (const auto& b : s.groupB) {
      pairs.push_back({a, b});
      pairs.push_back({b, a});
    }
  const auto ordered = scheduleOrder(pairs, rng);
  auto ground = [&](const TeamId& id) -> const Team& {
    for (const auto& t : league)
      if (t.id == id) return t;
    return league[0];
  };
  for (size_t i = 0; i < ordered.size(); ++i) {
    const auto& [home, away] = ordered[i];
    Fixture fx;
    fx.no = static_cast<int>(i) + 1;
    fx.stage = Stage::league;
    fx.home = home;
    fx.away = away;
    const Team& g = ground(home);
    fx.venue = g.ground;
    fx.pitch = rng() < 0.7 ? g.pitch : PITCH_KEYS[static_cast<size_t>(std::floor(rng() * PITCH_KEYS.size()))];
    fx.seed = seedFrom(rng);
    s.fixtures.push_back(std::move(fx));
  }
  for (const auto& t : league) s.venues.set(t.id, Venue{t.ground, t.pitch});
  return s;
}

MatchSummary summarizeMatch(const Match& match) {
  MatchSummary s;
  auto teamId = [&](int side) { return match.teams[static_cast<size_t>(side)].id; };
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const auto& [bid, b] : inn.batters) {
      const auto& d = b.dismissal;
      if (!d || !d->fielder) continue;
      FieldingLine& f = s.fielding[*d->fielder];
      if (d->type == Out::stumped)
        f.st++;
      else if (d->type == Out::runout)
        f.ro++;
      else
        f.ct++;
    }
    for (const auto& d : inn.drops) s.fielding[d.fielder].drops++;
    InningsSummary is;
    is.team = teamId(inn.batting);
    is.runs = inn.runs;
    is.wickets = inn.wickets;
    is.balls = inn.legalBalls;
    is.allOut = inn.wickets >= inn.maxWickets;
    is.maxBalls = inn.maxBalls;
    is.extras = inn.extras.wd + inn.extras.nb + inn.extras.lb + inn.extras.b;
    for (const auto& o : inn.overs) is.overs.push_back({o.runs, o.wickets});
    for (const auto& id : inn.battedOrder) {
      const auto& b = inn.batters.at(id);
      is.batting.push_back({id, b.runs, b.balls, b.fours, b.sixes, b.out ? 1 : 0, b.dots});
    }
    for (const auto& id : inn.bowlingOrder) {
      const auto& x = inn.bowling.at(id);
      is.bowling.push_back({id, x.balls, x.runs, x.wickets, x.maidens, x.dots, x.fours, x.sixes, x.wides, x.noballs});
    }
    s.innings.push_back(std::move(is));
  }
  std::vector<SuperOverLine> so;
  for (const Innings& i : match.innings)
    if (i.superOver) so.push_back({teamId(i.batting), i.runs, i.wickets});
  s.matchId = match.id;
  const Result& r = *match.result;
  if (r.winner) s.winner = teamId(*r.winner);
  s.text = r.text;
  s.noResult = r.noResult;
  s.dls = r.dls || (match.dls && match.dls->reduced);
  if (!so.empty()) s.superOver = so;
  s.potm = match.potm;
  if (match.toss) s.toss = TossLine{teamId(match.toss->winner), match.toss->decision};
  s.dew = match.dew;
  for (const auto& t : match.teams)
    if (t.impact) s.impact.push_back({t.id, t.impact->out, t.impact->in});
  return s;
}

bool recordResult(Season& season, int fixtureNo, MatchSummary summary) {
  Fixture* fx = nullptr;
  for (auto& f : season.fixtures)
    if (f.no == fixtureNo) {
      fx = &f;
      break;
    }
  if (!fx || fx->result) return false;
  if (fx->stage != Stage::league && !summary.winner) {
    summary.winner = fx->home;
    summary.text = cat(summary.text, ". ", fx->home, " go through as the higher-placed side.");
  }
  fx->result = std::move(summary);
  advancePlayoffs(season);
  return true;
}

std::vector<TableRow> pointsTable(const Season& season, int upTo) {
  OrderedMap<TeamId, TableRow> rows;
  std::vector<TeamId> all = season.groupA;
  all.insert(all.end(), season.groupB.begin(), season.groupB.end());
  for (const auto& id : all) {
    TableRow r;
    r.team = id;
    r.group = std::find(season.groupA.begin(), season.groupA.end(), id) != season.groupA.end() ? "A" : "B";
    rows.set(id, r);
  }
  for (const auto& fx : season.fixtures) {
    if (fx.stage != Stage::league || !fx.result || fx.no > upTo) continue;
    const MatchSummary& r = *fx.result;
    rows[fx.home].p++;
    rows[fx.away].p++;
    if (r.winner) {
      const TeamId loser = *r.winner == fx.home ? fx.away : fx.home;
      rows[*r.winner].w++;
      rows[*r.winner].pts += 2;
      rows[loser].l++;
      rows[*r.winner].form.push_back("W");
      rows[loser].form.push_back("L");
    } else {
      for (const TeamId& id : {fx.home, fx.away}) {
        rows[id].nr++;
        rows[id].pts += 1;
        rows[id].form.push_back("N");
      }
    }
    if (r.noResult) continue;
    for (const auto& inn : r.innings) {
      const TeamId& bat = inn.team;
      const TeamId bowl = bat == fx.home ? fx.away : fx.home;
      const int balls = inn.allOut ? inn.maxBalls : inn.balls;
      rows[bat].rf += inn.runs;
      rows[bat].bf += balls;
      rows[bowl].ra += inn.runs;
      rows[bowl].ba += balls;
      rows[bowl].wkts += inn.wickets;
      rows[bowl].ballsBowled += inn.balls;
    }
  }
  std::vector<TableRow> list;
  for (auto& [id, x] : rows) {
    TableRow r = x;
    r.nrr = (r.bf ? (r.rf * 6) / r.bf : 0) - (r.ba ? (r.ra * 6) / r.ba : 0);
    r.wpb = r.ballsBowled ? r.wkts / r.ballsBowled : 0;
    if (r.form.size() > 5) r.form.erase(r.form.begin(), r.form.end() - 5);
    list.push_back(std::move(r));
  }
  Rng lots(season.seed ^ 0x10775);
  std::unordered_map<TeamId, double> lot;
  for (const auto& id : all) lot[id] = lots();
  jsSort(list, [&](const TableRow& a, const TableRow& b) {
    if (double d = b.pts - a.pts) return d;
    if (double d = b.w - a.w) return d;
    if (double d = b.nrr - a.nrr) return d;
    if (double d = b.wpb - a.wpb) return d;
    return lot[a.team] - lot[b.team];
  });
  for (size_t i = 0; i < list.size(); ++i) list[i].pos = static_cast<int>(i) + 1;
  return list;
}

bool leagueComplete(const Season& season) {
  int n = 0;
  for (const auto& f : season.fixtures)
    if (f.stage == Stage::league && f.result) ++n;
  return n == LEAGUE_MATCHES;
}

void advancePlayoffs(Season& season) {
  if (!leagueComplete(season)) return;
  if (!stageFixture(season, Stage::q1)) {
    const auto t = pointsTable(season);
    season.top4 = std::vector<TeamId>{t[0].team, t[1].team, t[2].team, t[3].team};
    addFixture(season, Stage::q1, t[0].team, t[1].team);
    addFixture(season, Stage::elim, t[2].team, t[3].team);
    return;
  }
  // Re-find after each push: addFixture may move the vector
  if (winnerOf(stageFixture(season, Stage::q1)) && winnerOf(stageFixture(season, Stage::elim)) &&
      !stageFixture(season, Stage::q2)) {
    const TeamId a = *loserOf(stageFixture(season, Stage::q1));
    const TeamId b = *winnerOf(stageFixture(season, Stage::elim));
    auto seedOf = [&](const TeamId& id) {
      const auto& t4 = *season.top4;
      auto it = std::find(t4.begin(), t4.end(), id);
      return it == t4.end() ? -1 : static_cast<int>(it - t4.begin());
    };
    const bool aFirst = seedOf(a) < seedOf(b);
    addFixture(season, Stage::q2, aFirst ? a : b, aFirst ? b : a);
  }
  if (winnerOf(stageFixture(season, Stage::q1)) && winnerOf(stageFixture(season, Stage::q2)) &&
      !stageFixture(season, Stage::final_))
    addFixture(season, Stage::final_, *winnerOf(stageFixture(season, Stage::q1)),
               *winnerOf(stageFixture(season, Stage::q2)));
  const Fixture* fin = stageFixture(season, Stage::final_);
  if (winnerOf(fin) && !season.champion) {
    season.champion = winnerOf(fin);
    season.runnerUp = loserOf(fin);
  }
}

const Fixture* nextFixture(const Season& season) {
  for (const auto& f : season.fixtures)
    if (!f.result) return &f;
  return nullptr;
}

const Fixture* nextUserFixture(const Season& season) {
  for (const auto& f : season.fixtures)
    if (!f.result && (f.home == season.userTeam || f.away == season.userTeam)) return &f;
  return nullptr;
}

std::optional<std::string> userPlayoffStatus(const Season& season) {
  const TeamId& me = season.userTeam;
  if (!season.top4 || std::find(season.top4->begin(), season.top4->end(), me) == season.top4->end())
    return std::nullopt;
  const bool lostKnockout = std::any_of(season.fixtures.begin(), season.fixtures.end(), [&](const Fixture& f) {
    return (f.stage == Stage::elim || f.stage == Stage::q2 || f.stage == Stage::final_) && f.result &&
           (f.home == me || f.away == me) && f.result->winner != me;
  });
  if (lostKnockout) return "eliminated";
  if (season.champion) return std::nullopt;
  return nextUserFixture(season) ? "playing" : "waiting";
}

SeasonStats seasonStats(const Season& season, const TeamOf& teamOf) {
  OrderedMap<PlayerId, BatStat> bat;
  OrderedMap<PlayerId, BowlStat> bowl;
  OrderedMap<PlayerId, FieldStat> field;
  std::unordered_map<PlayerId, int> apps;
  auto B = [&](const PlayerId& id) -> BatStat& {
    if (BatStat* s = bat.find(id)) return *s;
    BatStat s;
    s.id = id;
    s.team = teamOf(id);
    return bat.set(id, s);
  };
  auto W = [&](const PlayerId& id) -> BowlStat& {
    if (BowlStat* s = bowl.find(id)) return *s;
    BowlStat s;
    s.id = id;
    s.team = teamOf(id);
    return bowl.set(id, s);
  };
  auto F = [&](const PlayerId& id) -> FieldStat& {
    if (FieldStat* s = field.find(id)) return *s;
    FieldStat s;
    s.id = id;
    s.team = teamOf(id);
    return field.set(id, s);
  };
  int done = 0;
  for (const auto& fx : season.fixtures) {
    if (!fx.result) continue;
    ++done;
    const MatchSummary& r = *fx.result;
    std::unordered_set<PlayerId> seen;
    for (const auto& inn : r.innings) {
      for (const auto& row : inn.batting) {
        seen.insert(row.id);
        BatStat& s = B(row.id);
        s.inns++, s.runs += row.runs, s.balls += row.balls, s.fours += row.fours, s.sixes += row.sixes,
            s.dots += row.dots;
        if (!row.out) s.no++;
        if (row.out && row.runs == 0) s.ducks++;
        if (row.runs >= 100)
          s.hundreds++;
        else if (row.runs >= 50)
          s.fifties++;
        if (row.runs > s.hs || (row.runs == s.hs && !row.out)) s.hs = row.runs, s.hsNO = !row.out;
      }
      for (const auto& row : inn.bowling) {
        seen.insert(row.id);
        BowlStat& s = W(row.id);
        s.inns++, s.balls += row.balls, s.runs += row.runs, s.wkts += row.wkts, s.maidens += row.maidens,
            s.dots += row.dots;
        s.fours += row.fours, s.sixes += row.sixes, s.wides += row.wides, s.noballs += row.noballs;
        if (row.wkts >= 5)
          s.fiveW++;
        else if (row.wkts >= 4)
          s.fourW++;
        if (s.inns == 1 || row.wkts > s.bestW || (row.wkts == s.bestW && row.runs < s.bestR))
          s.bestW = row.wkts, s.bestR = row.runs;
      }
    }
    for (const auto& [id, x] : r.fielding) {
      seen.insert(id);
      FieldStat& s = F(id);
      s.ct += x.ct, s.st += x.st, s.ro += x.ro, s.drops += x.drops;
    }
    for (const auto& id : seen) apps[id] += 1;
  }
  auto appsOf = [&](const PlayerId& id) {
    auto it = apps.find(id);
    return it == apps.end() ? 0 : it->second;
  };
  SeasonStats out;
  out.matches = done;
  for (auto& [id, s0] : bat) {
    BatStat s = s0;
    s.m = appsOf(id);
    if (s.inns - s.no > 0) s.avg = static_cast<double>(s.runs) / (s.inns - s.no);
    if (s.balls) s.sr = (s.runs * 100.0) / s.balls;
    s.hsText = cat(s.hs, s.hsNO ? "*" : "");
    if (s.runs) s.boundaryPct = ((s.fours * 4 + s.sixes * 6) * 100.0) / s.runs;
    out.batting.push_back(std::move(s));
  }
  for (auto& [id, s0] : bowl) {
    BowlStat s = s0;
    s.m = appsOf(id);
    s.overs = s.balls / 6.0;
    s.oversText = oversShort(s.balls);
    if (s.wkts) s.avg = static_cast<double>(s.runs) / s.wkts;
    if (s.balls) s.econ = (s.runs * 6.0) / s.balls;
    if (s.wkts) s.sr = static_cast<double>(s.balls) / s.wkts;
    if (s.balls) s.dotPct = (s.dots * 100.0) / s.balls;
    s.bestText = cat(s.bestW, '/', s.bestR);
    out.bowling.push_back(std::move(s));
  }
  for (auto& [id, s0] : field) {
    FieldStat s = s0;
    s.m = appsOf(id);
    s.dismissals = s.ct + s.st + s.ro;
    out.fielding.push_back(std::move(s));
  }
  OrderedMap<PlayerId, MvpStat> mvp;
  auto M = [&](const PlayerId& id) -> MvpStat& {
    if (MvpStat* s = mvp.find(id)) return *s;
    MvpStat s;
    s.id = id;
    s.team = teamOf(id);
    return mvp.set(id, s);
  };
  for (const auto& s : out.batting) {
    MvpStat& x = M(s.id);
    x.fours += s.fours;
    x.sixes += s.sixes;
  }
  for (const auto& s : out.bowling) {
    MvpStat& x = M(s.id);
    x.wkts += s.wkts;
    x.dots += s.dots;
  }
  for (const auto& s : out.fielding) M(s.id).catches += s.ct + s.st;
  for (auto& [id, x0] : mvp) {
    MvpStat x = x0;
    x.m = appsOf(id);
    x.pts = x.wkts * 3.5 + x.dots * 1.0 + x.fours * 2.5 + x.sixes * 3.5 + x.catches * 2.5;
    out.mvp.push_back(std::move(x));
  }
  return out;
}

std::vector<TeamStat> teamAnalytics(const Season& season) {
  OrderedMap<TeamId, TeamStat> t;
  auto T = [&](const TeamId& id) -> TeamStat& {
    if (TeamStat* s = t.find(id)) return *s;
    TeamStat s;
    s.team = id;
    return t.set(id, s);
  };
  for (const auto& fx : season.fixtures) {
    if (!fx.result) continue;
    const MatchSummary& r = *fx.result;
    for (const TeamId& id : {fx.home, fx.away}) {
      TeamStat& x = T(id);
      x.p++;
      if (r.winner == id)
        x.w++;
      else if (r.winner)
        x.l++;
      if (r.superOver) x.superOvers++;
    }
    if (!r.innings.empty()) {
      const auto& first = r.innings[0];
      T(first.team).batFirstP++;
      if (r.winner == first.team) T(first.team).batFirstW++;
      T(first.team).firstInnsTotals.push_back(first.runs);
    }
    if (r.innings.size() > 1) {
      const auto& second = r.innings[1];
      T(second.team).chaseP++;
      if (r.winner == second.team) T(second.team).chaseW++;
    }
    for (const auto& inn : r.innings) {
      const TeamId oppId = inn.team == fx.home ? fx.away : fx.home;
      T(inn.team);
      T(oppId);
      TeamStat& x = *t.find(inn.team);
      TeamStat& opp = *t.find(oppId);
      x.runs += inn.runs, x.balls += inn.balls;
      opp.conceded += inn.runs, opp.ballsBowled += inn.balls, opp.wkts += inn.wickets;
      const TotalLine total{inn.runs, inn.wickets, oppId, fx.no};
      if (!x.highest || inn.runs > x.highest->runs) x.highest = total;
      if (inn.wickets == 10 || inn.balls >= 120)
        if (!x.lowest || inn.runs < x.lowest->runs) x.lowest = total;
      for (size_t k = 0; k < inn.overs.size(); ++k) {
        const int runs = inn.overs[k][0];
        if (k < 6)
          x.ppRuns += runs, x.ppOvers++, opp.ppConc += runs, opp.ppConcOvers++;
        else if (k < 15)
          x.midRuns += runs, x.midOvers++;
        else
          x.deathRuns += runs, x.deathOvers++, opp.deathConc += runs, opp.deathConcOvers++;
      }
      for (const auto& row : inn.batting) x.fours += row.fours, x.sixes += row.sixes, x.dots += row.dots;
    }
  }
  auto rate = [](int r, int o) -> std::optional<double> { return o ? std::optional<double>(static_cast<double>(r) / o) : std::nullopt; };
  std::vector<TeamStat> out;
  for (auto& [id, x0] : t) {
    TeamStat x = x0;
    if (!x.firstInnsTotals.empty()) {
      double s = 0;
      for (int v : x.firstInnsTotals) s += v;
      x.avgFirst = s / static_cast<double>(x.firstInnsTotals.size());
    }
    if (x.balls) x.rr = (x.runs * 6.0) / x.balls;
    if (x.ballsBowled) x.econ = (x.conceded * 6.0) / x.ballsBowled;
    x.ppRR = rate(x.ppRuns, x.ppOvers);
    x.midRR = rate(x.midRuns, x.midOvers);
    x.deathRR = rate(x.deathRuns, x.deathOvers);
    x.ppEcon = rate(x.ppConc, x.ppConcOvers);
    x.deathEcon = rate(x.deathConc, x.deathConcOvers);
    if (x.balls) x.dotPct = (x.dots * 100.0) / x.balls;
    if (x.p) x.winPct = (x.w * 100.0) / x.p;
    out.push_back(std::move(x));
  }
  return out;
}

std::vector<PositionDay> positionHistory(const Season& season) {
  std::vector<PositionDay> out;
  for (const auto& fx : season.fixtures) {
    if (fx.stage != Stage::league || !fx.result) continue;
    PositionDay day;
    day.no = fx.no;
    for (const auto& r : pointsTable(season, fx.no)) day.table.push_back({r.team, r.pos, r.pts});
    out.push_back(std::move(day));
  }
  return out;
}

SeasonRecords seasonRecords(const Season& season, const TeamOf& teamOf) {
  SeasonRecords rec;
  std::vector<SeasonTotal> totals;
  std::vector<SeasonScore> scores;
  std::vector<SeasonFigure> figures;
  for (const auto& fx : season.fixtures) {
    if (!fx.result) continue;
    for (const auto& inn : fx.result->innings) {
      const TeamId vs = inn.team == fx.home ? fx.away : fx.home;
      totals.push_back({inn.team, vs, inn.runs, inn.wickets, fx.no, fx.stage});
      for (const auto& b : inn.batting) scores.push_back({b.id, inn.team, vs, b.runs, b.balls, b.out, fx.no});
      for (const auto& w : inn.bowling) figures.push_back({w.id, teamOf(w.id), inn.team, w.wkts, w.runs, w.balls, fx.no});
    }
  }
  auto top5 = [](auto v) {
    if (v.size() > 5) v.resize(5);
    return v;
  };
  auto hi = totals;
  jsSort(hi, [](const SeasonTotal& a, const SeasonTotal& b) { return b.runs - a.runs; });
  rec.highestTotals = top5(hi);
  std::vector<SeasonTotal> lo;
  for (const auto& x : totals)
    if (x.wickets == 10) lo.push_back(x);
  jsSort(lo, [](const SeasonTotal& a, const SeasonTotal& b) { return a.runs - b.runs; });
  rec.lowestTotals = top5(lo);
  jsSort(scores, [](const SeasonScore& a, const SeasonScore& b) {
    const int d = b.runs - a.runs;
    return d ? d : a.balls - b.balls;
  });
  rec.highestScores = top5(scores);
  jsSort(figures, [](const SeasonFigure& a, const SeasonFigure& b) {
    const int d = b.wkts - a.wkts;
    return d ? d : a.runs - b.runs;
  });
  rec.bestFigures = top5(figures);
  return rec;
}

SeasonAwards seasonAwards(const Season& season, const TeamOf& teamOf,
                          const std::function<std::optional<int>(const PlayerId&)>& ageOf) {
  const SeasonStats st = seasonStats(season, teamOf);
  auto by = [](const auto& arr, auto f, auto filter) {
    using T = typename std::decay_t<decltype(arr)>::value_type;
    std::vector<T> v;
    for (const auto& x : arr)
      if (filter(x)) v.push_back(x);
    std::stable_sort(v.begin(), v.end(), [&](const T& a, const T& b) { return f(b) - f(a) < 0; });
    return v.empty() ? std::optional<T>() : std::optional<T>(v[0]);
  };
  auto all = [](const auto&) { return true; };
  SeasonAwards a;
  a.orangeCap = by(st.batting, [](const BatStat& s) { return s.runs + s.sr.value_or(0) / 1000; }, all);
  a.purpleCap = by(st.bowling, [](const BowlStat& s) { return s.wkts - s.econ.value_or(99) / 100; }, all);
  a.mvp = by(st.mvp, [](const MvpStat& s) { return s.pts; }, all);
  a.mostSixes = by(st.batting, [](const BatStat& s) { return static_cast<double>(s.sixes); }, all);
  a.mostFours = by(st.batting, [](const BatStat& s) { return static_cast<double>(s.fours); }, all);
  a.bestStrikeRate = by(st.batting, [](const BatStat& s) { return s.sr.value_or(0); },
                        [](const BatStat& s) { return s.balls >= 100; });
  a.bestEconomy = by(st.bowling, [](const BowlStat& s) { return -s.econ.value_or(99); },
                     [](const BowlStat& s) { return s.balls >= 120; });
  a.mostDots = by(st.bowling, [](const BowlStat& s) { return static_cast<double>(s.dots); }, all);
  a.emerging = by(st.mvp, [](const MvpStat& s) { return s.pts; },
                  [&](const MvpStat& s) { return ageOf(s.id).value_or(99) <= 23; });
  a.safestHands = by(st.fielding, [](const FieldStat& s) { return static_cast<double>(s.ct + s.st); }, all);
  return a;
}

}  // namespace t20
