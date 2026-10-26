#pragma once
// Ageing (ageing.js): every off-season each player gets a year older and his skills move with his
// age. Young players grow, players in their prime hold steady, and from the early thirties the
// body goes first. Spinners age more gracefully than fast bowlers.

#include <string_view>

#include "engine/career_types.hpp"
#include "engine/types.hpp"

namespace t20 {

// A player's rating in his main discipline (what potential is measured against)
double mainRating(const Player& p);
// How much room a young player has left to grow: 0 at his ceiling, up to 1.4 far below it
double growthRoom(const Player& p);

struct AgePhase {
  std::string_view key, label, note;
};
AgePhase agePhase(int age);

struct SkillDeltas {
  SkillSet<BatK> bat;
  SkillSet<BowlK> bowl;
  SkillSet<FieldK> field;
};
struct AgeResult {
  PlayerId id;
  int age = 0;
  SkillDeltas deltas;
  Overalls before, after;
};
// One off-season for one player (p as he is now, before this birthday)
AgeResult ageOneYear(const Player& p, int season);

struct MainChange {
  double bat, bowl, field, main;
};
MainChange mainChange(const Overalls& before, const Overalls& after, Role role);

}  // namespace t20
