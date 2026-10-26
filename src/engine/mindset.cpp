#include "engine/mindset.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/commentary.hpp"
#include "engine/league.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "engine/weather.hpp"

namespace t20 {

// ---------------------------------------------------------------------------
// Confidence (batters) and rhythm (bowlers)
// ---------------------------------------------------------------------------

std::string_view confidenceLabel(double c) {
  if (c >= 80) return "In the zone";
  if (c >= 65) return "Confident";
  if (c >= 45) return "Settled";
  if (c >= 30) return "Tentative";
  return "Nervy";
}

std::string_view rhythmLabel(double r) {
  if (r >= 70) return "On fire";
  if (r >= 55) return "In rhythm";
  if (r >= 40) return "Steady";
  return "Rattled";
}

void batterFacedBall(BatterInnings& st, int runs, bool boundary, double temperament) {
  double d = 0.15;  // simply surviving settles you
  if (runs == 0)
    d -= 1.0 * (1.3 - temperament / 100);  // dot balls get to the nervous ones
  else if (runs == 4 && boundary)
    d += 3;
  else if (runs == 6)
    d += 4;
  else
    d += 1.0;
  st.conf = clamp(st.conf + d, 5, 100);
}

void partnerDismissed(BatterInnings& st) { st.conf = clamp(st.conf - (st.balls >= 30 ? 1 : 2), 5, 100); }

void milestoneBoost(BatterInnings& st, int m) { st.conf = clamp(st.conf + (m >= 100 ? 8 : 6), 5, 100); }

void bowlerEvent(BowlingFigures& fig, BowlerEvent kind) {
  double d = 0;
  switch (kind) {
    case BowlerEvent::wicket: d = 9; break;
    case BowlerEvent::dot: d = 1.2; break;
    case BowlerEvent::run: d = 0; break;
    case BowlerEvent::four: d = -3; break;
    case BowlerEvent::six: d = -4; break;
    case BowlerEvent::wide: d = -2; break;
    case BowlerEvent::noball: d = -3; break;
    case BowlerEvent::expensiveOver: d = -4; break;
    case BowlerEvent::maiden: d = 5; break;
    case BowlerEvent::drop: d = -2; break;
  }
  fig.rhythm = clamp(fig.rhythm + d, 10, 95);
}

// ---------------------------------------------------------------------------
// Live pitch conditions
// ---------------------------------------------------------------------------

double matchWear(const Match& match) {
  double played = 0;
  for (const Innings& i : match.innings) played += i.legalBalls;
  const double first = match.innings.size() > 0 ? match.innings[0].maxBalls : BALLS;
  const double second = match.innings.size() > 1 ? match.innings[1].maxBalls : first;
  const double total = first + second;
  // A hot day wears the pitch faster; a shortened game wears it less
  return std::min(1.0, (played / total) * (match.weather.sky == Sky::hot ? 1.25 : 1) * std::sqrt(total / (2 * BALLS)));
}

bool dewActive(const Match& match, int over) { return match.dew && match.current == 1 && over >= 8; }

Conditions pitchConditions(const Match& match, int over, bool spin) {
  const PitchDef& p = pitchDef(match.pitch);
  const double wear = matchWear(match);
  const PitchWear& wr = p.wear;
  double boundary = p.boundary * (1 + wr.boundary * wear);
  const double paceW = 1 + (p.paceWicket - 1) * (1 - wr.paceDecay * wear);
  const double spinW = p.spinWicket * (1 + wr.spin * wear);
  double wicket = p.wicket * (spin ? spinW : paceW);
  double wide = 1;
  if (!spin && over < 3) wicket *= 1.1;  // new ball swings
  const bool dew = dewActive(match, over);
  if (dew) {
    // Two-ball rule: a drier second ball comes in after the 10th over, halving the dew's effect
    const double k = over >= 10 ? 0.5 : 1;
    if (spin) {
      wicket *= 1 - 0.22 * k;
      boundary *= 1 + 0.14 * k;
    } else {
      boundary *= 1 + 0.04 * k;
    }
    wide *= 1 + 0.2 * k;
  }
  const SkyEffect sky = skyEffect(&match.weather, over, spin);
  wicket *= sky.wicket;
  boundary *= sky.boundary;
  wide *= sky.wide;
  return {boundary, p.dot, wicket, wide, wear, dew};
}

ConditionsSummary conditionsSummary(const Match& match) {
  const Innings* inn = match.innings.empty() ? nullptr : &inningsOf(match);
  const int over = inn ? inn->legalBalls / 6 : 0;
  const double wear = matchWear(match);
  const PitchDef& p = pitchDef(match.pitch);
  std::vector<std::string> notes;
  if (match.pitch == Pitch::dusty && wear > 0.35)
    notes.emplace_back("Turning square");
  else if (match.pitch == Pitch::dusty)
    notes.emplace_back("Gripping for spinners");
  if (match.pitch == Pitch::green && wear < 0.3)
    notes.emplace_back("Seaming around");
  else if (match.pitch == Pitch::green)
    notes.emplace_back("Grass drying out");
  if (match.pitch == Pitch::slow && wear > 0.4) notes.emplace_back("Getting slower");
  if (over < 3 && inn && !inn->complete) notes.emplace_back("New ball swinging");
  if (dewActive(match, over)) notes.emplace_back("Heavy dew");
  if (match.weather.sky == Sky::overcast && over < 10) notes.emplace_back("Overcast: the ball is swinging");
  if (match.dls && match.dls->target && inn && inn->target)
    notes.push_back(cat("DLS target ", *inn->target, " from ", static_cast<int>(std::ceil(inn->maxBalls / 6.0)), " overs"));
  return {p.label, js_round(wear * 100), notes};
}

// ---------------------------------------------------------------------------
// Game awareness
// ---------------------------------------------------------------------------

int wicketsInLast(const Innings& inn, int balls) {
  int n = 0;
  for (const auto& f : inn.fow)
    if (f.ball > inn.legalBalls - balls) ++n;
  return n;
}

Mindset computeMindset(const Match& match, const Innings& inn, const PlayerId& batterId, double instructed) {
  const Player& p = match.players.at(batterId);
  const BatterInnings& st = inn.batters.at(batterId);
  const int over = over20(&inn, inn.legalBalls / 6);  // thresholds below are on the 20-over scale
  const int ballsLeft = inn.maxBalls - inn.legalBalls;
  const int wk = inn.wickets;
  const bool chase = inn.target.has_value();
  const double need = chase ? *inn.target - inn.runs : 0;
  const double rrr = chase ? (need * 6) / std::max(1, ballsLeft) : 0;
  const bool desperate = chase && rrr > 12.5;

  struct Part {
    double v;
    std::string_view label;
  };
  std::vector<Part> parts;
  parts.reserve(12);
  std::string seeingOff;

  const int recent = wicketsInLast(inn, 18);
  if (recent >= 2 && !desperate)
    parts.push_back({-1.4, "Rebuilding after a collapse"});
  else if (wicketsInLast(inn, 6) >= 1 && st.balls < 6 && !desperate)
    parts.push_back({-0.6, "Steadying things after a wicket"});
  if (st.balls < 6 && over < 16 && !desperate) parts.push_back({-0.5, "Playing himself in"});
  if (wk >= 7 && over < 17 && !desperate)
    parts.push_back({-0.9, "Protecting the tail"});
  else if (wk >= 5 && over < 13 && !desperate)
    parts.push_back({-0.6, "Too many down to take risks"});
  if (over >= 16 && wk <= 5) parts.push_back({0.6, "Going big at the death"});

  if (chase) {
    if (desperate)
      parts.push_back({0.8, "Rate climbing, has to go for it"});
    else if (rrr < 7 && 10 - wk <= 5)
      parts.push_back({-0.7, "Knocking it around, target in reach"});
  } else if (over >= 8 && over < 16 && wk <= 3 && inn.legalBalls > 0) {
    const double projected = inn.runs + ((inn.runs * 6.0) / inn.legalBalls) * (ballsLeft / 6.0);
    // a shorter innings has a lower par
    const double par = pitchDef(match.pitch).par * (resources(inn.maxBalls / 6.0, 0) / 100);
    if (projected < par * 0.88) parts.push_back({0.5, "Behind par, needs to accelerate"});
  }

  if (inn.currentBowler) {
    const Player& bowler = match.players.at(*inn.currentBowler);
    const bool spin = isSpinner(bowler);
    if (match.pitch == Pitch::dusty && spin) parts.push_back({-0.4, "Respecting the turn"});
    if (match.pitch == Pitch::green && !spin && over < 6) parts.push_back({-0.4, "Seeing off the new ball"});
    if (match.pitch == Pitch::slow) parts.push_back({-0.25, "Pitch too slow to force it"});
    // A bowler running through the side: see him off and score at the other end
    const BowlingFigures* f = inn.bowling.find(bowler.id);
    if (f && f->wickets >= 2 && !desperate) {
      seeingOff = cat("Seeing off ", surname(bowler.name));
      parts.push_back({-std::min(1.1, 0.4 * (f->wickets - 1)), seeingOff});
    }
  }

  // How well he reads the game decides how much of this he acts on. Poor readers can even do
  // the opposite under pressure: a swing for the fences when the team needs a steady head.
  const double awareness = p.awareness;
  const double aw = clamp((awareness - 30) / 60, -0.3, 1.1);
  double sum = 0;
  for (const auto& part : parts) sum += part.v;
  const double raw = clamp(sum, -1.8, 1.2);
  const double situational = raw * aw;
  if (aw < 0.1 && raw <= -0.5) parts.push_back({-raw + 0.01, "Panicking under pressure"});

  // Confidence lets a batter express himself (or makes him freeze).
  const double c = (st.conf - 50) / 50;
  double confPart = 0;
  if (st.conf >= 70)
    confPart = 0.5 * c;
  else if (st.conf < 30)
    confPart = 0.6 * c;
  if (confPart > 0.15) parts.push_back({confPart, "Taking the bowlers on"});
  if (confPart < -0.15) parts.push_back({confPart, "Low on confidence"});

  const double adjust = situational + confPart;
  const double tempo = clamp(instructed + adjust, 1, 5);
  std::string_view label = "Following the plan";
  double best = 0.2;
  for (const auto& part : parts) {
    if (std::abs(part.v) > best) {
      best = std::abs(part.v);
      label = part.label;
    }
  }
  return {tempo, std::string(label), adjust};
}

double startConfidence(double formBonus) { return clamp(START_CONFIDENCE + formBonus, 20, 70); }

}  // namespace t20
