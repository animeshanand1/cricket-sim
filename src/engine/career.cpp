#include "engine/career.hpp"

#include <algorithm>
#include <cstdlib>

#include "core/str.hpp"
#include "engine/achievements.hpp"
#include "engine/matchups.hpp"
#include "engine/milestones.hpp"
#include "engine/popularity.hpp"
#include "engine/records.hpp"

namespace t20 {

namespace {
bool better(int w, int r, int bestW, int bestR) { return w > bestW || (w == bestW && r < bestR); }
}  // namespace

bool recordMatch(Career& career, const Match& match, const BaseOf* baseOf) {
  if (match.status != MatchStatus::complete) return false;
  if (std::find(career.recorded.begin(), career.recorded.end(), match.id) != career.recorded.end()) return false;
  career.recorded.push_back(match.id);
  if (career.recorded.size() > RECORDED_LIMIT)
    career.recorded.erase(career.recorded.begin(),
                          career.recorded.begin() + static_cast<std::ptrdiff_t>(career.recorded.size() - RECORDED_LIMIT));
  const std::string when =
      match.fixture && match.fixture->seasonNo ? cat("Season ", match.fixture->seasonNo) : std::string("Friendly");
  updateRecords(career.records, match, when);

  std::vector<PlayerId> everyone;
  for (const auto& t : match.teams) {
    everyone.insert(everyone.end(), t.players.begin(), t.players.end());
    if (t.impact) everyone.push_back(t.impact->out);
  }
  const auto before = snapshot(career.players, everyone);

  auto get = [&](const PlayerId& id) -> CareerStats& { return career.players[id]; };
  struct PerMatch {
    int side;
    std::optional<std::string> bat, bowl;
  };
  OrderedMap<PlayerId, PerMatch> perMatch;

  for (int side = 0; side < 2; ++side) {
    const MatchTeam& team = match.teams[static_cast<size_t>(side)];
    std::vector<PlayerId> ids = team.players;
    if (team.impact) ids.push_back(team.impact->out);
    for (const auto& id : ids) {
      CareerStats& s = get(id);
      s.matches += 1;
      if (match.result && match.result->winner == side) s.wins += 1;
      perMatch.set(id, PerMatch{side, std::nullopt, std::nullopt});
    }
  }

  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings& b = inn.batters.at(id);
      BatCareer& s = get(id).bat;
      s.inns += 1;
      s.runs += b.runs;
      s.balls += b.balls;
      s.fours += b.fours;
      s.sixes += b.sixes;
      if (!b.out) s.notOuts += 1;
      if (b.out && b.runs == 0) s.ducks += 1;
      if (b.runs >= 100)
        s.hundreds += 1;
      else if (b.runs >= 50)
        s.fifties += 1;
      if (b.runs > s.hs || (b.runs == s.hs && !b.out)) {
        s.hs = b.runs;
        s.hsNotOut = !b.out;
      }
      perMatch[id].bat = cat(b.runs, b.out ? "" : "*", " (", b.balls, ")");
      const auto& d = b.dismissal;
      if (d && d->fielder) {
        FieldCareer& f = get(*d->fielder).field;
        if (d->type == Out::stumped)
          f.stumpings += 1;
        else if (d->type == Out::runout)
          f.runOuts += 1;
        else
          f.catches += 1;
      }
    }
    for (const DropRecord& d : inn.drops) get(d.fielder).field.drops += 1;
    for (const PlayerId& id : inn.bowlingOrder) {
      const BowlingFigures& fig = inn.bowling.at(id);
      if (!fig.balls && !fig.runs) continue;
      BowlCareer& s = get(id).bowl;
      s.inns += 1;
      s.balls += fig.balls;
      s.runs += fig.runs;
      s.wickets += fig.wickets;
      s.maidens += fig.maidens;
      s.dots += fig.dots;
      if (fig.wickets >= 4) s.fourW += 1;
      if (s.inns == 1 || better(fig.wickets, fig.runs, s.bestW, s.bestR)) {
        s.bestW = fig.wickets;
        s.bestR = fig.runs;
      }
      perMatch[id].bowl = cat(fig.wickets, '/', fig.runs);
    }
  }

  recordMatchups(career.matchups, match);
  if (match.potm) get(*match.potm).potm += 1;
  updatePopularity(career, match);

  if (baseOf) {
    auto reports = developFromMatch(career.progress, match, *baseOf);
    career.lastReport = LastReport{match.id, std::move(reports)};
  }

  for (const auto& [id, pm] : perMatch) {
    const MatchTeam& opp = match.teams[static_cast<size_t>(1 - pm.side)];
    const std::string result = !match.result || !match.result->winner ? "T"
                               : *match.result->winner == pm.side       ? "W"
                                                                        : "L";
    CareerStats& s = get(id);
    s.recent.insert(s.recent.begin(), RecentLine{opp.id, pm.bat, pm.bowl, result});
    if (s.recent.size() > RECENT_LIMIT) s.recent.resize(RECENT_LIMIT);
  }

  auto reached = milestonesReached(before, career.players, match);
  career.lastMilestones = LastMilestones{match.id, reached};
  std::vector<MilestoneHit> log;
  for (auto x : reached) {
    x.when = when;
    log.push_back(std::move(x));
  }
  log.insert(log.end(), career.milestones.begin(), career.milestones.end());
  if (log.size() > MILESTONE_LOG) log.resize(MILESTONE_LOG);
  career.milestones = std::move(log);
  career.lastUnlocked = LastUnlocked{match.id, updateManager(career, match, when)};
  return true;
}

CareerSummary summarize(const CareerStats* s) {
  static const CareerStats empty;
  const CareerStats& st = s ? *s : empty;
  CareerSummary out;
  out.s = &st;
  const auto& bat = st.bat;
  const auto& bowl = st.bowl;
  const int outs = bat.inns - bat.notOuts;
  if (outs > 0) out.batAvg = static_cast<double>(bat.runs) / outs;
  if (bat.balls > 0) out.batSR = (bat.runs * 100.0) / bat.balls;
  out.hsText = bat.inns ? cat(bat.hs, bat.hsNotOut ? "*" : "") : "–";
  out.overs = bowl.balls % 6 ? cat(bowl.balls / 6, '.', bowl.balls % 6) : cat(bowl.balls / 6);
  if (bowl.wickets > 0) out.bowlAvg = static_cast<double>(bowl.runs) / bowl.wickets;
  if (bowl.balls > 0) out.econ = (bowl.runs * 6.0) / bowl.balls;
  if (bowl.wickets > 0) out.bowlSR = static_cast<double>(bowl.balls) / bowl.wickets;
  out.bestText = bowl.inns ? cat(bowl.bestW, '/', bowl.bestR) : "–";
  return out;
}

std::string fmt(std::optional<double> v, int digits) { return v ? js_fixed(*v, digits) : "–"; }

Form formOf(const CareerStats* stats) {
  std::vector<int> scores;
  if (stats)
    for (const auto& r : stats->recent) {
      if (!r.bat || r.bat->empty()) continue;
      scores.push_back(std::atoi(r.bat->c_str()));  // parseInt: leading digits
      if (scores.size() == 3) break;
    }
  if (scores.empty()) return {0, "No recent innings", {}};
  double sum = 0;
  for (int s : scores) sum += s;
  const double avg = sum / static_cast<double>(scores.size());
  const double bonus = std::max(-8.0, std::min(12.0, js_round((avg - 18) / 2)));
  const char* label = bonus >= 8 ? "Red hot" : bonus >= 3 ? "In form" : bonus > -3 ? "Steady" : "Out of form";
  return {bonus, label, scores};
}

}  // namespace t20
