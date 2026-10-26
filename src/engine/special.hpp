#pragma once
// Special deliveries (special.js): every bowler has one signature ball. Once his rhythm is high
// enough he can produce it; what happens depends on the batter.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/battle.hpp"
#include "engine/match_types.hpp"

namespace t20 {

struct SpecialDef {
  SpecialKey key;
  std::string_view label;
  BowlK skill;  // the bowling skill that powers it
  std::vector<std::pair<Out, double>> out;  // how it shifts the way the wicket falls
};
const SpecialDef& specialDef(SpecialKey k);

// What each bowling type can bowl, in the order used to break ties
const std::vector<SpecialKey>& specialsFor(std::optional<BowlType> bowlType);
// The bowler's special: the one chosen in the editor, or the one his best skill suits
std::optional<SpecialKey> specialOf(const Player& p);
std::optional<std::string_view> specialLabel(const Player& p);

inline constexpr double SPECIAL_RHYTHM = 70;
inline constexpr int SPECIAL_GAP = 6;

bool specialReady(const BowlingFigures* fig);
inline double specialChance(const BowlingFigures& fig) { return 0.1 + 0.006 * (fig.rhythm - SPECIAL_RHYTHM); }

struct SpecialEffect {
  double W, o0, o1, o4, o6, edge;
};
SpecialEffect specialEffect(const Player& bowler, SpecialKey key, double rhythm, const BatterNow& B, double conf);
std::vector<std::pair<Out, double>> specialDismissals(SpecialKey key, std::vector<std::pair<Out, double>> weights);
// Commentary for a special delivery that did not take a wicket
std::string specialText(const std::string& lead, SpecialKey key, const std::string& striker, int runs);

}  // namespace t20
