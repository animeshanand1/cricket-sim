#include "engine/ageing.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "engine/rng.hpp"
#include "engine/skills.hpp"

namespace t20 {

double mainRating(const Player& p) {
  if (p.role == Role::BOWL) return p.bowl;
  if (p.role == Role::AR) return js_round((p.bat + p.bowl) / 2);
  return p.bat;
}

double growthRoom(const Player& p) {
  if (!p.potential) return 1;
  return std::max(0.0, std::min(1.4, (*p.potential - mainRating(p)) / 6));
}

namespace {

// Per-season change by age band (the age he is turning). Keys a player doesn't have are ignored.
struct Curve {
  int upTo;
  std::vector<std::pair<BatK, double>> bat;
  std::vector<std::pair<BowlK, double>> bowl;
  std::vector<std::pair<FieldK, double>> field;
};

using B = BatK;
using W = BowlK;
using F = FieldK;

const std::vector<Curve>& CURVE() {
  static const std::vector<Curve> c = {
      {21,
       {{B::defence, 2}, {B::vsPace, 2}, {B::vsSpin, 2}, {B::grounded, 2.5}, {B::lofted, 2.5}, {B::rotation, 2},
        {B::running, 0.5}, {B::awareness, 2.5}, {B::temperament, 2}},
       {{W::pace, 1.5}, {W::accuracy, 2}, {W::swing, 1.5}, {W::seam, 1.5}, {W::variations, 2}, {W::death, 2},
        {W::turn, 1.5}, {W::flight, 2}},
       {{F::catching, 1}, {F::ground, 0.5}, {F::throwing, 1}, {F::keeping, 1.5}}},
      {24,
       {{B::defence, 1.5}, {B::vsPace, 1}, {B::vsSpin, 1.5}, {B::grounded, 1.5}, {B::lofted, 1.5}, {B::rotation, 1.5},
        {B::awareness, 2}, {B::temperament, 1.5}},
       {{W::pace, 0.5}, {W::accuracy, 1.5}, {W::swing, 1}, {W::seam, 1}, {W::variations, 1.5}, {W::death, 1.5},
        {W::turn, 1}, {W::flight, 1.5}},
       {{F::catching, 0.5}, {F::throwing, 0.5}, {F::keeping, 1}}},
      {29,
       {{B::defence, 0.5}, {B::vsSpin, 0.5}, {B::rotation, 0.5}, {B::awareness, 1}, {B::temperament, 1}},
       {{W::accuracy, 0.5}, {W::variations, 0.5}, {W::death, 0.5}, {W::flight, 0.5}},
       {}},
      {32,
       {{B::vsPace, -0.5}, {B::lofted, -0.5}, {B::running, -1.5}, {B::awareness, 0.5}, {B::temperament, 0.5}},
       {{W::pace, -1.5}, {W::swing, -0.5}, {W::seam, -0.5}, {W::death, -0.5}, {W::accuracy, 0.5}, {W::turn, -0.3}},
       {{F::ground, -1.5}, {F::throwing, -1}, {F::catching, -0.5}, {F::keeping, -0.5}}},
      {35,
       {{B::defence, -0.5}, {B::vsPace, -1.5}, {B::grounded, -1}, {B::lofted, -1.5}, {B::rotation, -1},
        {B::running, -3}, {B::awareness, 0.3}},
       {{W::pace, -3}, {W::accuracy, -0.5}, {W::swing, -1.5}, {W::seam, -1.5}, {W::variations, -0.5}, {W::death, -1.5},
        {W::turn, -0.5}, {W::flight, -0.3}},
       {{F::ground, -2.5}, {F::throwing, -2}, {F::catching, -1.5}, {F::keeping, -1.5}}},
      {99,
       {{B::defence, -1.5}, {B::vsPace, -3}, {B::vsSpin, -1}, {B::grounded, -2}, {B::lofted, -2.5}, {B::rotation, -2},
        {B::running, -4}, {B::awareness, -0.5}, {B::temperament, -0.5}},
       {{W::pace, -4.5}, {W::accuracy, -1.5}, {W::swing, -2}, {W::seam, -2}, {W::variations, -1}, {W::death, -2.5},
        {W::turn, -1}, {W::flight, -1}},
       {{F::ground, -3.5}, {F::throwing, -3}, {F::catching, -2.5}, {F::keeping, -2.5}}},
  };
  return c;
}

const Curve& curveFor(int age) {
  for (const auto& c : CURVE())
    if (age <= c.upTo) return c;
  return CURVE().back();
}

Rng seasonRng(const std::string& id, int season) {
  int32_t h = 0x0a9e ^ js_imul(season, 7919);
  for (unsigned char ch : id) h = js_imul(h ^ static_cast<int32_t>(ch), kGolden);
  return Rng(h);
}

bool isMental(BatK k) { return k == BatK::awareness || k == BatK::temperament; }
template <class K>
bool isMental(K) {
  return false;
}

Overalls overallsOf(const Player& p, const Skills& s) {
  return {battingOverall(s.bat), bowlingOverall(s.bowl, p.bowlType), fieldingOverall(s.field, p.role == Role::WK)};
}

}  // namespace

AgePhase agePhase(int age) {
  if (age <= 23) return {"rising", "Rising", "still improving every season"};
  if (age <= 29) return {"prime", "Prime", "at his peak; reads the game better each year"};
  if (age <= 32) return {"experienced", "Experienced", "wiser, but pace and legs start to fade"};
  if (age <= 35) return {"veteran", "Veteran", "declining: pace, running and fielding drop each season"};
  return {"twilight", "Twilight", "declining fast; every skill slips each season"};
}

AgeResult ageOneYear(const Player& p, int season) {
  AgeResult r;
  r.id = p.id;
  r.age = p.age + 1;
  const Curve& curve = curveFor(r.age);
  Rng rng = seasonRng(p.id, season);
  const double rate = 0.6 + rng() * 0.8;
  const bool pace = isPaceType(p.bowlType);
  const double room = growthRoom(p);
  Skills after = p.skills;

  auto run = [&](auto& afterSet, auto& deltaSet, const auto& entries, int area) {
    for (const auto& [k, base] : entries) {
      if (!afterSet.contains(k)) continue;
      double v = base * rate;
      if (area == 1 && v < 0) v *= pace ? 1.15 : 0.7;
      if (v > 0) {
        v *= std::max(0.2, (99 - afterSet[k]) / 40);
        v *= isMental(k) ? std::max(0.5, room) : room;
        if (area == 0 && p.role == Role::BOWL) v *= 0.35;
        if (area == 1 && p.bowl < 40) v *= 0.3;
      }
      const double next = clamp(afterSet[k] + v, 1, 99);
      const double d = js_round((next - afterSet[k]) * 10) / 10;
      if (d != 0) {
        deltaSet.set(k, d);
        afterSet.set(k, afterSet[k] + d);
      }
    }
  };
  run(after.bat, r.deltas.bat, curve.bat, 0);
  run(after.bowl, r.deltas.bowl, curve.bowl, 1);
  run(after.field, r.deltas.field, curve.field, 2);
  r.before = overallsOf(p, p.skills);
  r.after = overallsOf(p, after);
  return r;
}

MainChange mainChange(const Overalls& before, const Overalls& after, Role role) {
  const double bat = after.bat - before.bat;
  const double bowl = after.bowl - before.bowl;
  const double field = after.field - before.field;
  const double main =
      role == Role::BOWL ? bowl : role == Role::AR ? (bat + bowl) / 2 : role == Role::WK ? (bat + field) / 2 : bat;
  return {bat, bowl, field, main};
}

}  // namespace t20
