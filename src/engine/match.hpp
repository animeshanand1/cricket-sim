#pragma once
// The T20 match engine (match.js). Everything mutates a plain Match value, so the same code
// runs live, in sims, and in win-probability copies.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/battle.hpp"
#include "engine/fans.hpp"
#include "engine/matchups.hpp"
#include "engine/match_types.hpp"
#include "engine/mindset.hpp"
#include "engine/rng.hpp"

namespace t20 {

// ---------------------------------------------------------------------------
// Probability model
// ---------------------------------------------------------------------------

// Outcome weights for one legal ball, in the order JS iterates them: 0,1,2,3,4,6,W.
struct OutcomeWeights {
  std::array<double, 7> w{};
  bool hasW = true;
  double& operator[](Outcome o) { return w[static_cast<size_t>(o)]; }
  double operator[](Outcome o) const { return w[static_cast<size_t>(o)]; }
};
Outcome weightedOutcome(RandFn rng, const OutcomeWeights& w);

struct OutcomeInputs {
  const Player* bowler = nullptr;
  BatterNow B{};
  BowlerNow T{};
  std::optional<Edge> edge;
  std::optional<FieldingNow> field;
  int ballsFaced = 0;
  int runsScored = 0;
  Phase phase = Phase::mid;
  double tempo = 3;
  Conditions cond{1, 1, 1, 1, 0, false};
  double conf = START_CONFIDENCE;
  double rhythm = 50;
  bool freeHit = false;
};
OutcomeWeights outcomeWeights(const OutcomeInputs& in);

struct AggressionLevel {
  int level;
  std::string_view label;
};
extern const std::vector<AggressionLevel> AGGRESSION;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

struct SideInput {
  TeamId id;
  std::string name, color;
  PlayerId keeperId;
  Captain captain = Captain::balanced;
  std::optional<PlayerId> captainId;
  std::vector<Player> players;  // the XI in batting order
  std::vector<Player> bench;
};

struct MatchSetup {
  int32_t seed = 0;
  std::optional<std::string> id;
  std::array<SideInput, 2> teams;
  std::optional<int> userTeam = 0;
  Pitch pitch = Pitch::balanced;
  MatchSettings settings;
  std::optional<Weather> weather;  // none = forecast from the seed
  GroundInfo ground;
  const MatchupBook* matchups = nullptr;
};

Match createMatch(const MatchSetup& setup);

// The human calls the toss ("heads" / "tails")
const Toss& callToss(Match& match, const std::string& call);
void chooseTossDecision(Match& match, TossDecision decision);
// AI vs AI (and scripts): random toss, AI decision, start
void autoToss(Match& match);
void startSecondInnings(Match& match);

// ---------------------------------------------------------------------------
// Decisions
// ---------------------------------------------------------------------------

bool userIsBatting(const Match& match);
bool userIsBowling(const Match& match);
bool setNextBatter(Match& match, const PlayerId& id);
bool setBowler(Match& match, const PlayerId& id);
// The user's plan for the current bowler (nullopt = let the AI captain decide each ball)
bool setBowlingPlan(Match& match, std::optional<Plan> plan);
// The user's field: a preset (auto = the AI's) and, if customised, the positions
bool setField(Match& match, FieldPreset preset, std::optional<std::vector<Pos>> positions = std::nullopt);
bool setBatterTarget(Match& match, const PlayerId& bowlerId, std::optional<TargetMode> mode);
bool setBatterTempo(Match& match, const PlayerId& batterId, Tempo level);
Tempo batterTempoOf(const Match& match, const PlayerId& batterId);
bool canChangeBowler(const Match& match);
bool changeBowler(Match& match);
// The AI makes whatever call the user is being asked for (used when simming ahead)
void resolvePending(Match& match);
// Refresh the displayed mindset without playing a ball
void refreshMindset(Match& match);

// ---------------------------------------------------------------------------
// Ball by ball
// ---------------------------------------------------------------------------

inline constexpr int REVIEWS = 2;

// Returns what happened ("W", "wd", "nb", "review", "0".."7"), or empty if no ball was bowled
std::string playBall(Match& match);
// The user's call on a review (match.awaiting == review)
bool reviewDecision(Match& match, bool review);

std::optional<PlayerId> playerOfTheMatch(const Match& match);

// Batch play
void playOver(Match& match);
void simInnings(Match& match);
void simMatch(Match& match);

// ---------------------------------------------------------------------------
// Impact Player
// ---------------------------------------------------------------------------

bool impactWindowOpen(const Match& match);
struct ImpactIn {
  PlayerId id;
  bool ok = true;
  std::string reason;
};
struct ImpactOptions {
  bool allowed = false;
  std::string reason;
  std::vector<PlayerId> outs;
  std::vector<ImpactIn> ins;
};
ImpactOptions impactOptions(const Match& match, int side);
bool useImpactPlayer(Match& match, int side, const PlayerId& outId, const PlayerId& inId, bool byAi = false);
struct ImpactPlan {
  PlayerId out, in;
  double gain;
};
std::optional<ImpactPlan> aiImpactPlan(const Match& match, int side);
void aiUseImpact(Match& match, int side);

}  // namespace t20
