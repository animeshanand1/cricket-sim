#include "engine/ai.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "engine/battle.hpp"
#include "engine/league.hpp"
#include "engine/matchups.hpp"
#include "engine/mindset.hpp"
#include "engine/rng.hpp"
#include "engine/rules.hpp"
#include "engine/tactics.hpp"

namespace t20 {

double matchupBonus(const Match& match, const Innings& inn, const PlayerId& bowlerId) {
  auto edge = [&](const std::optional<PlayerId>& batId) -> double {
    if (!batId) return 0;
    const double* e = match.h2h.find(matchupKey(*batId, bowlerId));
    return e ? *e : 0;
  };
  const double k = match.teams[static_cast<size_t>(inn.bowlingSide)].captain == Captain::shrewd ? 1.5 : 1;
  return -(edge(inn.striker) * 8 + edge(inn.nonStriker) * 4) * k;
}

PlayerId aiChooseBowler(Match& match) {
  const Innings& inn = inningsOf(match);
  const int over = overIndex(inn);
  const Phase phase = phaseAt(&inn, over);
  std::vector<BowlOption> options;
  for (auto& o : bowlingOptions(match))
    if (o.ok) options.push_back(o);
  if (options.empty()) throw std::runtime_error("No legal bowler available");

  // Rank the attack so the best two can be saved for the death.
  const MatchTeam& team = match.teams[static_cast<size_t>(inn.bowlingSide)];
  std::vector<const Player*> attack;
  for (const PlayerId& id : team.players) attack.push_back(&match.players.at(id));
  std::stable_sort(attack.begin(), attack.end(), [](const Player* a, const Player* b) { return b->bowl - a->bowl < 0; });
  std::vector<PlayerId> strike;
  for (size_t i = 0; i < attack.size() && i < 2; ++i) strike.push_back(attack[i]->id);

  const double wear = matchWear(match);
  const bool dew = dewActive(match, over);
  StateStream stream{&match.rngState};
  PlayerId best;
  bool haveBest = false;
  double bestScore = -std::numeric_limits<double>::infinity();
  for (const BowlOption& o : options) {
    const Player& p = match.players.at(o.id);
    const bool spin = isSpinner(p);
    const BowlingFigures* fig = inn.bowling.find(o.id);
    double score = p.bowl + styleBowlerBonus(team.captain, p, phase);
    if (p.bowl < 40) score -= 25;  // part-timers only when needed
    if (phase == Phase::pp) score += spin ? -6 : 5;
    if (phase == Phase::mid) score += spin ? 7 : -2;
    if (phase == Phase::death) score += spin ? -4 : 6;
    // Right bowler for the moment: new-ball swing, seam on a green top, turn on a dry pitch, death skills
    const Edge edge = specialistEdge(p, {over, phase, match.pitch, wear, dew});
    score += (edge.wicket - 1) * 60 - (edge.boundary - 1) * 40;
    // Match-ups: bring on the bowler who has the batters' number, keep away the one they enjoy
    score += matchupBonus(match, inn, o.id);
    const int left = bowlerQuota(&inn) - o.bowled;
    if (std::find(strike.begin(), strike.end(), o.id) != strike.end() && phase != Phase::death && left <= 2)
      score -= 14;  // keep overs for the death
    if (fig && fig->balls >= 6) {
      const double econ = (fig->runs * 6.0) / fig->balls;
      if (econ > 11) score -= 6;
      if (econ < 7) score += 3;
      score += fig->wickets * 2;
    }
    score += stream() * 6;
    if (score > bestScore) {
      bestScore = score;
      best = o.id;
      haveBest = true;
    }
  }
  if (!haveBest) best = options[0].id;  // (only if every score was NaN; the JS would return null)
  return best;
}

double aiAggression(const Match& match) {
  const Innings& inn = inningsOf(match);
  const int over = overIndex(inn);
  const int wk = inn.wickets;
  const int ballsLeft = inn.maxBalls - inn.legalBalls;
  const double oversAll = std::ceil(inn.maxBalls / 6.0);
  if (inn.target) {
    const double need = *inn.target - inn.runs;
    const double rrr = (need * 6) / std::max(1, ballsLeft);
    double a = rrr > 13 ? 5 : rrr > 10.5 ? 4 : rrr > 7.2 ? 3 : 2;
    if (ballsLeft <= 12 && need / ballsLeft > 1.1) a = 5;
    return clamp(a, 1, 5);
  }
  double a;
  const double o20 = std::floor((over * 20) / oversAll);  // on the 20-over scale
  if (o20 < 6)
    a = wk <= 1 ? 4 : 3;
  else if (o20 < 15)
    a = 3;
  else if (o20 < 18)
    a = 4;
  else
    a = 5;
  // Wickets, collapses, conditions and confidence are handled by each batter's own game
  // awareness (mindset); the captain only sets the plan for the phase.
  return clamp(a, 1, 5);
}

NextBatterCall nextBatterCall(const Match& match) {
  const Innings& inn = inningsOf(match);
  const auto& queue = inn.yetToBat;
  const PlayerId next = queue.empty() ? PlayerId{} : queue[0];
  if (queue.size() < 2) return {next, ""};
  auto P = [&](const PlayerId& id) -> const Player& { return match.players.at(id); };
  std::vector<PlayerId> options;
  for (size_t i = 0; i < queue.size() && i < 3; ++i)
    if (P(queue[i]).bat >= 40 || queue[i] == next) options.push_back(queue[i]);
  const int over = over20(&inn, overIndex(inn));
  const int ballsLeft = inn.maxBalls - inn.legalBalls;
  auto best = [&](auto score) {
    PlayerId a = next;
    for (const PlayerId& b : options)
      if (score(b) > score(a)) a = b;
    return a;
  };
  // The last few overs, wickets in hand: send in the hitter
  if ((over >= 15 || ballsLeft <= 30) && inn.wickets <= 6) {
    auto hit = [&](const PlayerId& id) {
      const auto& s = P(id).skills.bat;
      return s[BatK::lofted] * 0.7 + s[BatK::vsPace] * 0.15 + P(id).power * 0.15;
    };
    const PlayerId pick = best(hit);
    if (pick != next && hit(pick) >= hit(next) + 8) return {pick, "Promoted: a hitter for the last overs"};
  }
  // A collapse with plenty of the innings left: someone to stop the rot
  if (wicketsInLast(inn, 18) >= 2 && over < 12) {
    auto anchor = [&](const PlayerId& id) {
      const auto& s = P(id).skills.bat;
      return s[BatK::defence] * 0.5 + s[BatK::temperament] * 0.3 + s[BatK::rotation] * 0.2;
    };
    const PlayerId pick = best(anchor);
    if (pick != next && anchor(pick) >= anchor(next) + 8) return {pick, "Promoted: a steady head to stop the collapse"};
  }
  return {next, ""};
}

PlayerId aiNextBatter(const Match& match) { return nextBatterCall(match).id; }

TossDecision aiTossDecision(Match& match) {
  if (match.dew) return TossDecision::bowl;                          // chase under dew
  if (match.weather.sky == Sky::overcast) return TossDecision::bowl;  // use the swing
  if (match.weather.rain && match.weather.rainChance > 0.1) return TossDecision::bowl;  // know the target if rain comes
  switch (match.pitch) {
    case Pitch::green:
    case Pitch::flat: return TossDecision::bowl;
    case Pitch::dusty:
    case Pitch::slow: return TossDecision::bat;
    default: {
      StateStream stream{&match.rngState};
      return stream() < 0.5 ? TossDecision::bat : TossDecision::bowl;
    }
  }
}

}  // namespace t20
