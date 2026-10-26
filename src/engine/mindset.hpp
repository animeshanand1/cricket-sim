#pragma once
// The "human" layer of the engine (mindset.js): batter confidence, bowler rhythm, game
// awareness, and live conditions (pitch wear, the new ball, dew).

#include <string>
#include <vector>

#include "engine/match_types.hpp"

namespace t20 {

inline constexpr double START_CONFIDENCE = 45;

std::string_view confidenceLabel(double c);
std::string_view rhythmLabel(double r);

// Called for the striker after every ball he faces (legal or no-ball).
void batterFacedBall(BatterInnings& st, int runs, bool boundary = false, double temperament = 60);
void partnerDismissed(BatterInnings& st);
void milestoneBoost(BatterInnings& st, int m);

enum class BowlerEvent : uint8_t { wicket, dot, run, four, six, wide, noball, expensiveOver, maiden, drop };
void bowlerEvent(BowlingFigures& fig, BowlerEvent kind);

double matchWear(const Match& match);
bool dewActive(const Match& match, int over);

struct Conditions {
  double boundary, dot, wicket, wide, wear;
  bool dew;
};
// Multipliers for the current delivery
Conditions pitchConditions(const Match& match, int over, bool spin);

struct ConditionsSummary {
  std::string_view label;
  double wear;
  std::vector<std::string> notes;
};
ConditionsSummary conditionsSummary(const Match& match);

int wicketsInLast(const Innings& inn, int balls);

struct Mindset {
  double tempo;
  std::string label;
  double adjust;
};
// The batter's own reading of the game, applied on top of the captain's tempo (1-5).
Mindset computeMindset(const Match& match, const Innings& inn, const PlayerId& batterId, double instructed);

// Starting confidence from recent form (career stats)
double startConfidence(double formBonus = 0);

}  // namespace t20
