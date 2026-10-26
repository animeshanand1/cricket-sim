#include "engine/tactics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engine/league.hpp"

namespace t20 {

// clang-format off
const std::vector<PlanDef> PACE_PLANS = {
  {Plan::stock, "Stock", std::nullopt, std::nullopt, "Good length, mixing it up. No particular risk or reward."},
  {Plan::yorkers, "Yorkers", BowlK::death, Weak::yorker, "Full and straight: hard to hit for six, bowls people. Missed yorkers are full tosses."},
  {Plan::bouncers, "Short ball", BowlK::pace, Weak::shortBall, "Bouncers and back of a length: gets batters caught hooking. Good pullers cash in."},
  {Plan::wide, "Wide line", BowlK::accuracy, Weak::outsideOff, "Outside off, packed off side: dries up boundaries, fewer wickets, more wides."},
  {Plan::slower, "Slower balls", BowlK::variations, Weak::slower, "Cutters and slower bouncers: deceive the big hitters."},
  {Plan::attack, "Attack the stumps", BowlK::accuracy, Weak::yorker, "Full, straight, wicket-to-wicket: more wickets, more runs."},
};

const std::vector<PlanDef> SPIN_PLANS = {
  {Plan::stock, "Stock", std::nullopt, std::nullopt, "Normal pace and flight."},
  {Plan::flight, "Toss it up", BowlK::flight, Weak::flight, "Flight and loop: invites the big shot, gets stumpings and catches, goes for sixes."},
  {Plan::darts, "Fire it in", BowlK::accuracy, std::nullopt, "Flat and quick into the pads: hard to score off, fewer wickets."},
  {Plan::wide, "Wide of off", BowlK::accuracy, Weak::outsideOff, "Away from the hitting arc: saves runs."},
  {Plan::mystery, "Variations", BowlK::variations, Weak::spin, "Googlies, carrom balls, arm balls: wickets against poor players of spin, loose balls if it goes wrong."},
};
// clang-format on

const std::vector<PlanDef>& plansFor(const Player& bowler) { return isSpinner(bowler) ? SPIN_PLANS : PACE_PLANS; }

const PlanDef& planByKey(const Player& bowler, Plan key) {
  const auto& plans = plansFor(bowler);
  for (const auto& p : plans)
    if (p.key == key) return p;
  return plans[0];
}

namespace {
using Mask = uint8_t;
inline Mask bit(Weak w) { return static_cast<Mask>(1u << static_cast<unsigned>(w)); }
inline Mask bit(Strength s) { return static_cast<Mask>(1u << static_cast<unsigned>(s)); }

// A batter's weak spots, read from his skills
Mask weakMask(const Player& p) {
  const auto& s = p.skills.bat;
  Mask m = 0;
  if (s[BatK::vsPace] <= 64) m |= bit(Weak::shortBall);
  if (s[BatK::defence] <= 62) m |= bit(Weak::yorker);
  if ((s[BatK::defence] + s[BatK::grounded]) / 2 <= 64 || s[BatK::temperament] <= 56) m |= bit(Weak::outsideOff);
  if (s[BatK::lofted] >= 76 && s[BatK::awareness] <= 70) m |= bit(Weak::slower);
  if (s[BatK::vsSpin] <= 64) m |= bit(Weak::spin);
  if (s[BatK::lofted] >= 74 && s[BatK::vsSpin] <= 72) m |= bit(Weak::flight);
  return m;
}

Mask strengthMask(const Player& p) {
  const auto& s = p.skills.bat;
  Mask m = 0;
  if (s[BatK::lofted] >= 80 && s[BatK::vsPace] >= 74) m |= bit(Strength::puller);
  if (s[BatK::vsSpin] >= 82) m |= bit(Strength::spinPlayer);
  if (s[BatK::rotation] >= 82) m |= bit(Strength::rotator);
  return m;
}
}  // namespace

std::vector<Weak> weaknesses(const Player& p) {
  const Mask m = weakMask(p);
  std::vector<Weak> out;
  for (Weak w : {Weak::shortBall, Weak::yorker, Weak::outsideOff, Weak::slower, Weak::spin, Weak::flight})
    if (m & bit(w)) out.push_back(w);
  return out;
}

std::vector<Strength> strengths(const Player& p) {
  const Mask m = strengthMask(p);
  std::vector<Strength> out;
  for (Strength s : {Strength::puller, Strength::spinPlayer, Strength::rotator})
    if (m & bit(s)) out.push_back(s);
  return out;
}

bool hasStrength(const Player& p, Strength s) { return (strengthMask(p) & bit(s)) != 0; }

std::string_view weakLabel(Weak w) {
  switch (w) {
    case Weak::shortBall: return "Short ball";
    case Weak::yorker: return "Yorkers";
    case Weak::outsideOff: return "Outside off";
    case Weak::slower: return "Slower balls";
    case Weak::spin: return "Spin";
    default: return "Tossed-up spin";
  }
}

std::string_view strengthLabel(Strength s) {
  switch (s) {
    case Strength::puller: return "Pulls the short ball";
    case Strength::spinPlayer: return "Plays spin well";
    default: return "Rotates the strike";
  }
}

PlanEffect planEffect(Plan planKey, const Player& bowler, const Player* batter) {
  PlanEffect m;
  if (planKey == Plan::stock || !batter) return m;
  const PlanDef& plan = planByKey(bowler, planKey);
  const auto& b = bowler.skills.bowl;
  const double q = plan.skill ? (b.get_or(*plan.skill, 60) - 65) / 30 : 0;  // -1 (poor) .. +1 (elite)
  const Mask weak = weakMask(*batter);
  const Mask strong = strengthMask(*batter);
  const bool fits = plan.exploits ? (weak & bit(*plan.exploits)) != 0 : false;
  m.fits = fits;
  const double miss = q < 0 ? 1 + 0.35 * -q : 1;  // poor execution leaks boundaries
  const bool puller = strong & bit(Strength::puller);
  const bool spinPlayer = strong & bit(Strength::spinPlayer);
  switch (planKey) {
    case Plan::yorkers:
      m.W = 0.98 + 0.1 * q + (fits ? 0.18 : 0);
      m.o6 = (0.9 - 0.1 * q) * miss;
      m.o4 = (0.97 - 0.06 * q) * miss;
      m.o1 = 1.03;
      m.wide = 1.1;
      break;
    case Plan::bouncers:
      m.W = 0.97 + 0.08 * q + (fits ? 0.22 : 0) - (puller ? 0.15 : 0);
      m.o6 = (1.05 - 0.1 * q + (puller ? 0.3 : 0)) * miss;
      m.o0 = 1.08;
      m.o1 = 0.88;
      m.wide = 1.25;
      break;
    case Plan::wide:
      m.W = 0.82 + (fits ? 0.18 : 0);
      m.o4 = 0.9;
      m.o6 = 0.84;
      m.o0 = 1.03;
      m.o1 = 1.06;
      m.wide = 1.35 - 0.3 * q;
      break;
    case Plan::slower:
      m.W = 0.97 + 0.08 * q + (fits ? 0.2 : 0);
      m.o6 = (0.98 - 0.08 * q) * miss;
      m.o4 = 1.04;
      m.o0 = 1.03;
      if (strong & bit(Strength::rotator)) m.o1 = 1.1;
      break;
    case Plan::attack:
      m.W = 1.07 + 0.06 * q + (fits ? 0.08 : 0);
      m.o4 = 1.12 * miss;
      m.o6 = 1.08;
      m.o0 = 0.95;
      m.wide = 0.9;
      break;
    case Plan::flight:
      m.W = 1.05 + 0.08 * q + (fits ? 0.12 : 0) + ((weak & bit(Weak::spin)) ? 0.06 : 0) - (spinPlayer ? 0.12 : 0);
      m.o6 = (1.18 - 0.1 * q + (spinPlayer ? 0.15 : 0)) * miss;
      m.o4 = 1.05;
      m.o0 = 0.98;
      break;
    case Plan::darts:
      m.W = 0.88;
      m.o4 = 0.9 * miss;
      m.o6 = 0.9 * miss;
      m.o0 = 1.02;
      m.o1 = 1.07;
      break;
    case Plan::mystery:
      m.W = 0.98 + 0.15 * q + (fits ? 0.18 : 0) - (spinPlayer ? 0.1 : 0);
      m.o4 = (1.05 - 0.1 * q) * miss;
      m.o6 = 1 - 0.05 * q;
      m.o0 = 1.02;
      m.wide = 1.15;
      break;
    default: break;
  }
  // Batters adjust to any plan they see coming, so no plan is free
  m.W *= 0.96;
  return m;
}

namespace {
constexpr int FULLY_READ = 6;  // further balls until he has it completely worked out
}

double readLevel(Plan plan, int n) {
  if (plan == Plan::stock || n <= READ_AFTER) return 0;
  return std::min(1.0, static_cast<double>(n - READ_AFTER) / FULLY_READ);
}

PlanEffect readByBatter(const PlanEffect& eff, Plan plan, int n) {
  const double k = readLevel(plan, n);
  PlanEffect out = eff;
  if (!k) {
    out.read = 0;
    return out;
  }
  out.W = 1 + (eff.W - 1) * (1 - k) - 0.1 * k;
  out.o4 = eff.o4 * (1 + 0.15 * k);
  out.o6 = eff.o6 * (1 + 0.2 * k);
  out.fits = eff.fits && k < 0.5;
  out.read = k;
  return out;
}

void notePlanFaced(Innings& inn, const PlayerId& batterId, Plan plan) {
  auto it = inn.planStreak.find(batterId);
  if (it != inn.planStreak.end() && it->second.plan == plan)
    it->second.n += 1;
  else
    inn.planStreak[batterId] = {plan, 1};
}

std::vector<std::pair<Out, double>> planDismissals(Plan planKey, std::vector<std::pair<Out, double>> w) {
  auto mul = [&](Out k, double f) {
    for (auto& kv : w)
      if (kv.first == k) kv.second *= f;
  };
  switch (planKey) {
    case Plan::yorkers: mul(Out::bowled, 1.8); mul(Out::lbw, 1.4); mul(Out::caught, 0.8); break;
    case Plan::attack: mul(Out::bowled, 1.5); mul(Out::lbw, 1.5); break;
    case Plan::bouncers: mul(Out::caught, 1.5); mul(Out::bowled, 0.5); mul(Out::lbw, 0.4); break;
    case Plan::wide: mul(Out::caughtKeeper, 1.8); mul(Out::caught, 1.1); mul(Out::bowled, 0.5); mul(Out::lbw, 0.5); break;
    case Plan::slower: mul(Out::caught, 1.3); break;
    case Plan::flight: mul(Out::stumped, 1.8); mul(Out::caught, 1.2); break;
    case Plan::darts: mul(Out::lbw, 1.4); mul(Out::bowled, 1.2); break;
    case Plan::mystery: mul(Out::bowled, 1.5); mul(Out::lbw, 1.2); break;
    default: break;
  }
  return w;
}

// ---------------------------------------------------------------------------
// Captains
// ---------------------------------------------------------------------------

const CaptainStyleDef& captainStyle(Captain c) {
  static const CaptainStyleDef defs[] = {
      {"Balanced", "Sensible plans by phase."},
      {"Aggressive", "Bats with intent and bowls to take wickets."},
      {"Cautious", "Protects wickets and dries up runs."},
      {"Shrewd", "Reads every batter and bowls to his weakness."},
      {"Spin first", "Turns to spin early and often."},
      {"Pace first", "Trusts the quicks, even in the middle overs."},
  };
  return defs[static_cast<size_t>(c)];
}

double styleTempo(Captain style) {
  return style == Captain::aggressive ? 0.45 : style == Captain::cautious ? -0.45 : 0;
}

double styleBowlerBonus(Captain style, const Player& bowler, Phase phase) {
  const bool spin = isSpinner(bowler);
  if (style == Captain::spin) return spin ? (phase == Phase::death ? 2 : 7) : -2;
  if (style == Captain::pace) return spin ? -5 : 4;
  return 0;
}

namespace {

// What a plan is worth against this batter right now: wickets, minus boundaries, minus what he
// has already read of it.
double planValue(Plan key, const Player& bowler, const Player& batter, const PlanStreak* streak,
                 double wicketWeight = 3, double boundaryWeight = 1) {
  const PlanEffect e =
      readByBatter(planEffect(key, bowler, &batter), key, streak && streak->plan == key ? streak->n + 1 : 1);
  return (e.W - 1) * wicketWeight - ((e.o4 - 1) * 0.6 + (e.o6 - 1) * 0.8) * boundaryWeight;
}

Plan preferredPlan(Captain style, const Player& bowler, const Player& batter, Phase phase,
                   const std::optional<ChaseState>& chase) {
  const auto& plans = plansFor(bowler);
  const bool spin = isSpinner(bowler);
  const auto& b = bowler.skills.bowl;
  const Mask weak = weakMask(batter);
  if (chase && chase->ballsLeft > 0) {
    const double perBall = chase->need / chase->ballsLeft;
    // Defending: they need big hits, so deny the boundaries
    if (perBall > 1.6 && phase == Phase::death) return spin ? Plan::wide : b[BowlK::death] >= 65 ? Plan::yorkers : Plan::wide;
    // They are cruising: only wickets will do
    if (perBall < 0.85 && chase->ballsLeft > 18) {
      Plan best = Plan::stock;
      double bestScore = -std::numeric_limits<double>::infinity();
      for (const auto& p : plans) {
        const double v = planValue(p.key, bowler, batter, nullptr, 5, 0.4);
        if (v > bestScore) {
          bestScore = v;
          best = p.key;
        }
      }
      return best;
    }
  }
  // Shrewd captains (and everyone, sometimes) pick the plan that hits a weakness the bowler can exploit
  const PlanDef* targeted = nullptr;
  for (const auto& p : plans) {
    if (p.exploits && (weak & bit(*p.exploits)) && b.get_or(*p.skill, 0) >= 62) {
      targeted = &p;
      break;
    }
  }
  if (targeted && (style == Captain::shrewd || style == Captain::aggressive)) return targeted->key;
  if (style == Captain::shrewd) {
    // No clear weakness: pick the plan with the best expected return
    Plan best = Plan::stock;
    double bestScore = 0;
    for (const auto& p : plans) {
      const PlanEffect e = planEffect(p.key, bowler, &batter);
      const double score = (e.W - 1) * 3 - ((e.o4 - 1) * 0.6 + (e.o6 - 1) * 0.8);
      if (score > bestScore) {
        bestScore = score;
        best = p.key;
      }
    }
    return best;
  }
  if (style == Captain::cautious)
    return spin ? Plan::darts : phase == Phase::death && b[BowlK::death] >= 65 ? Plan::yorkers : Plan::wide;
  if (style == Captain::aggressive) return spin ? Plan::flight : phase == Phase::death ? Plan::yorkers : Plan::attack;
  if (phase == Phase::death && !spin && b[BowlK::death] >= 68) return Plan::yorkers;
  return Plan::stock;
}

}  // namespace

Plan aiPlan(Captain style, const Player& bowler, const Player& batter, Phase phase, const AiPlanContext& ctx) {
  const Plan preferred = preferredPlan(style, bowler, batter, phase, ctx.chase);
  if (!ctx.r) return preferred;
  const RandFn& r = *ctx.r;
  const PlanStreak* streak = ctx.streak;
  // Mix it up: every captain varies it now and then; nobody keeps bowling a plan the batter has read
  const int seen = streak && streak->plan == preferred ? streak->n : 0;
  const double changeP = 0.08 + (style == Captain::shrewd ? 0.1 : style == Captain::cautious ? -0.04 : 0) +
                         (seen >= READ_AFTER + 1 ? 0.55 : seen >= READ_AFTER - 1 ? 0.25 : 0);
  if (r() >= changeP) return preferred;
  // Switch to one of the next best plans (stock always an option), weighted by their value
  struct Option {
    Plan key;
    double v;
  };
  std::vector<Option> others;
  for (const auto& p : plansFor(bowler))
    if (p.key != preferred) others.push_back({p.key, planValue(p.key, bowler, batter, streak)});
  std::stable_sort(others.begin(), others.end(), [](const Option& a, const Option& b) { return b.v - a.v < 0; });
  if (others.size() > 3) others.resize(3);
  double total = 0;
  for (size_t i = 0; i < others.size(); ++i) total += 3 - static_cast<double>(i);
  double x = r() * total;
  for (size_t i = 0; i < others.size(); ++i) {
    x -= 3 - static_cast<double>(i);
    if (x <= 0) return others[i].key;
  }
  return Plan::stock;
}

}  // namespace t20
