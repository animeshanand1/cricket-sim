#include "engine/special.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"

namespace t20 {

using SK = SpecialKey;

const SpecialDef& specialDef(SpecialKey k) {
  // clang-format off
  static const SpecialDef defs[] = {
    {SK::yorker, "Toe-crushing yorker", BowlK::death, {{Out::bowled, 2.2}, {Out::lbw, 1.6}, {Out::caught, 0.6}}},
    {SK::outswinger, "Late outswinger", BowlK::swing, {{Out::caughtKeeper, 3}, {Out::caught, 1.2}, {Out::lbw, 0.6}}},
    {SK::inswinger, "Booming inswinger", BowlK::swing, {{Out::lbw, 2.2}, {Out::bowled, 1.8}, {Out::caughtKeeper, 0.6}}},
    {SK::bouncer, "Searing bouncer", BowlK::pace, {{Out::caught, 1.6}, {Out::caughtKeeper, 1.4}, {Out::bowled, 0.4}, {Out::lbw, 0.3}}},
    {SK::seamer, "Seaming jaffa", BowlK::seam, {{Out::caughtKeeper, 2.5}, {Out::bowled, 1.5}}},
    {SK::knuckle, "Knuckle ball", BowlK::variations, {{Out::caught, 1.5}, {Out::bowled, 1.4}}},
    {SK::cutter, "Off-cutter", BowlK::variations, {{Out::bowled, 1.5}, {Out::lbw, 1.4}, {Out::caught, 1.2}}},
    {SK::googly, "Googly", BowlK::variations, {{Out::bowled, 2}, {Out::lbw, 1.6}, {Out::stumped, 1.3}}},
    {SK::doosra, "Doosra", BowlK::variations, {{Out::caughtKeeper, 2.5}, {Out::stumped, 1.6}, {Out::caught, 1.1}}},
    {SK::carrom, "Carrom ball", BowlK::variations, {{Out::bowled, 1.8}, {Out::lbw, 1.6}}},
    {SK::armball, "Arm ball", BowlK::accuracy, {{Out::lbw, 2.2}, {Out::bowled, 1.6}, {Out::stumped, 0.6}}},
    {SK::flipper, "Flipper", BowlK::accuracy, {{Out::lbw, 2}, {Out::bowled, 1.8}, {Out::stumped, 0.5}}},
    {SK::ripper, "Big-turning ripper", BowlK::turn, {{Out::bowled, 1.6}, {Out::stumped, 1.5}, {Out::caughtKeeper, 2}}},
    {SK::drifter, "Drifting dipper", BowlK::flight, {{Out::stumped, 2.2}, {Out::caught, 1.3}}},
  };
  // clang-format on
  return defs[static_cast<size_t>(k)];
}

const std::vector<SpecialKey>& specialsFor(std::optional<BowlType> bowlType) {
  // clang-format off
  static const std::vector<SpecialKey> none;
  static const std::vector<SpecialKey> byType[] = {
    /* fast */     {SK::yorker, SK::outswinger, SK::bouncer, SK::seamer, SK::knuckle, SK::inswinger, SK::cutter},
    /* fastmed */  {SK::yorker, SK::outswinger, SK::seamer, SK::bouncer, SK::knuckle, SK::inswinger, SK::cutter},
    /* medium */   {SK::outswinger, SK::seamer, SK::cutter, SK::yorker, SK::knuckle, SK::inswinger},
    /* lfast */    {SK::yorker, SK::inswinger, SK::bouncer, SK::seamer, SK::cutter, SK::outswinger, SK::knuckle},
    /* lfastmed */ {SK::yorker, SK::inswinger, SK::seamer, SK::bouncer, SK::cutter, SK::outswinger, SK::knuckle},
    /* lmedium */  {SK::inswinger, SK::seamer, SK::cutter, SK::yorker, SK::outswinger, SK::knuckle},
    /* off */      {SK::doosra, SK::armball, SK::ripper, SK::drifter, SK::carrom},
    /* leg */      {SK::googly, SK::flipper, SK::ripper, SK::drifter},
    /* sla */      {SK::armball, SK::ripper, SK::drifter, SK::carrom},
    /* lws */      {SK::googly, SK::ripper, SK::drifter, SK::flipper},
  };
  // clang-format on
  if (!bowlType) return none;
  return byType[static_cast<size_t>(*bowlType)];
}

std::optional<SpecialKey> specialOf(const Player& p) {
  const auto& keys = specialsFor(p.bowlType);
  if (keys.empty()) return std::nullopt;
  if (p.special && std::find(keys.begin(), keys.end(), *p.special) != keys.end()) return p.special;
  const auto& b = p.skills.bowl;
  SpecialKey best = keys[0];
  for (SpecialKey k : keys)
    if (b.get_or(specialDef(k).skill, 0) > b.get_or(specialDef(best).skill, 0)) best = k;
  return best;
}

std::optional<std::string_view> specialLabel(const Player& p) {
  const auto k = specialOf(p);
  if (!k) return std::nullopt;
  return specialDef(*k).label;
}

bool specialReady(const BowlingFigures* fig) {
  return fig && fig->rhythm >= SPECIAL_RHYTHM && fig->balls - fig->specialAt.value_or(-SPECIAL_GAP) >= SPECIAL_GAP;
}

SpecialEffect specialEffect(const Player& bowler, SpecialKey key, double rhythm, const BatterNow& B, double conf) {
  const double skill = bowler.skills.bowl.get_or(specialDef(key).skill, bowler.bowl);
  const double potency = (skill - 65) / 25 + (rhythm - SPECIAL_RHYTHM) / 60;
  const double resist = (B.survive - 60) / 22 + (conf - 50) / 45;
  const double edge = clamp(potency - resist, -2, 2.5);
  const double boundary = clamp(0.6 * js_exp(-0.3 * edge), 0.3, 1.15);
  return {clamp(1.7 * js_exp(0.5 * edge), 1.1, 4.5), 1.3, 0.9, boundary, boundary, edge};
}

std::vector<std::pair<Out, double>> specialDismissals(SpecialKey key, std::vector<std::pair<Out, double>> w) {
  for (const auto& [k, f] : specialDef(key).out)
    for (auto& kv : w)
      if (kv.first == k) kv.second *= f;
  return w;
}

std::string specialText(const std::string& lead, SpecialKey key, const std::string& striker, int runs) {
  const std::string ball = lower_ascii(std::string(specialDef(key).label));
  if (runs == 0) return cat(lead, ", the ", ball, "! ", striker, " is beaten all ends up but survives.");
  if (runs == 4) return cat(lead, ", the ", ball, ", but ", striker, " reads it and finds the rope. FOUR!");
  if (runs == 6) return cat(lead, ", the ", ball, ", and ", striker, " has picked it! SIX!");
  return cat(lead, ", the ", ball, "! ", striker, " gets something on it and they run ", runs, ".");
}

}  // namespace t20
