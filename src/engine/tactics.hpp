#pragma once
// Captaincy tactics (tactics.js): bowling plans against each batter's weaknesses, and the AI
// captains' own styles. A plan changes the odds of every outcome for the balls it is used on.

#include <optional>
#include <string_view>
#include <vector>

#include "engine/match_types.hpp"
#include "engine/rng.hpp"

namespace t20 {

enum class Weak : uint8_t { shortBall, yorker, outsideOff, slower, spin, flight };
enum class Strength : uint8_t { puller, spinPlayer, rotator };

struct PlanDef {
  Plan key;
  std::string_view label;
  std::optional<BowlK> skill;
  std::optional<Weak> exploits;
  std::string_view help;
};

extern const std::vector<PlanDef> PACE_PLANS;
extern const std::vector<PlanDef> SPIN_PLANS;

const std::vector<PlanDef>& plansFor(const Player& bowler);
const PlanDef& planByKey(const Player& bowler, Plan key);

std::vector<Weak> weaknesses(const Player& p);
std::vector<Strength> strengths(const Player& p);
std::string_view weakLabel(Weak w);
std::string_view strengthLabel(Strength s);
bool hasStrength(const Player& p, Strength s);

// Multipliers on the outcome weights (and on the wide chance) for this plan, bowler and batter
struct PlanEffect {
  double W = 1, o0 = 1, o1 = 1, o4 = 1, o6 = 1, wide = 1;
  bool fits = false;
  double read = 0;
};
PlanEffect planEffect(Plan planKey, const Player& bowler, const Player* batter);

// Predictability: bowl the same plan ball after ball and the batter starts waiting for it
inline constexpr int READ_AFTER = 4;
double readLevel(Plan plan, int n);
PlanEffect readByBatter(const PlanEffect& eff, Plan plan, int n);
void notePlanFaced(Innings& inn, const PlayerId& batterId, Plan plan);

// How the plan shifts the way wickets fall (same key order as the input)
std::vector<std::pair<Out, double>> planDismissals(Plan planKey, std::vector<std::pair<Out, double>> weights);

struct CaptainStyleDef {
  std::string_view label, note;
};
const CaptainStyleDef& captainStyle(Captain c);
double styleTempo(Captain style);
double styleBowlerBonus(Captain style, const Player& bowler, Phase phase);

struct ChaseState {
  double need;
  double ballsLeft;
};
struct AiPlanContext {
  const RandFn* r = nullptr;  // the match stream, for variety; none = the preferred plan
  const PlanStreak* streak = nullptr;
  std::optional<ChaseState> chase;
};
// The AI captain's plan for this bowler against this batter
Plan aiPlan(Captain style, const Player& bowler, const Player& batter, Phase phase, const AiPlanContext& ctx = {});

}  // namespace t20
