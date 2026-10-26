#include "engine/skills.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "engine/rng.hpp"

namespace t20 {

// ---------------------------------------------------------------------------
// Definitions
// ---------------------------------------------------------------------------

// clang-format off
const std::vector<SkillDef<BatK>> BAT_SKILLS = {
  {BatK::defence, "Defence", "Technique", 0.17, "Solid technique. Keeps good balls out, especially early on."},
  {BatK::vsPace, "Against pace", "Technique", 0.1, "Handles speed, bounce and movement."},
  {BatK::vsSpin, "Against spin", "Technique", 0.1, "Footwork and reading turn and flight."},
  {BatK::grounded, "Grounded shots", "Scoring", 0.16, "Timing and placement along the ground: drives, cuts, pulls for four."},
  {BatK::lofted, "Lofted shots", "Scoring", 0.15, "Power and range hitting over the top for six."},
  {BatK::rotation, "Strike rotation", "Scoring", 0.13, "Finding singles and keeping the scoreboard moving; fewer dot balls."},
  {BatK::running, "Running", "Scoring", 0.04, "Speed and judgement between the wickets: twos, threes, fewer run outs."},
  {BatK::awareness, "Game awareness", "Mind", 0.09, "Reads the match: rebuilds after a collapse, times the acceleration."},
  {BatK::temperament, "Temperament", "Mind", 0.06, "Composure under pressure: chases, collapses, the nineties."},
};

const std::vector<SkillDef<BowlK>> PACE_SKILLS = {
  {BowlK::pace, "Pace", "", 0.2, "Raw speed. Hurries batters and beats them for pace."},
  {BowlK::accuracy, "Line and length", "", 0.26, "Hits the right areas: fewer loose balls and wides."},
  {BowlK::swing, "Swing", "", 0.15, "Movement in the air. Deadly with the new ball."},
  {BowlK::seam, "Seam movement", "", 0.15, "Movement off the pitch, best on a green top."},
  {BowlK::variations, "Variations", "", 0.12, "Slower balls, cutters and bouncers."},
  {BowlK::death, "Death bowling", "", 0.12, "Yorkers and nerve in the last five overs."},
};

const std::vector<SkillDef<BowlK>> SPIN_SKILLS = {
  {BowlK::turn, "Turn", "", 0.3, "How much it spins, especially on a dry, wearing pitch."},
  {BowlK::flight, "Flight and drift", "", 0.2, "Deceives in the air and draws batters out of the crease."},
  {BowlK::accuracy, "Accuracy", "", 0.3, "Relentless line and length; hard to get away."},
  {BowlK::variations, "Variations", "", 0.2, "Googly, doosra, arm ball, carrom ball: the mystery."},
};

const std::vector<SkillDef<FieldK>> FIELD_SKILLS = {
  {FieldK::catching, "Catching", "", 0, "Safe hands; fewer dropped chances."},
  {FieldK::ground, "Ground fielding", "", 0, "Cuts off boundaries and turns twos into ones."},
  {FieldK::throwing, "Throwing", "", 0, "Arm strength and accuracy: run outs."},
  {FieldK::keeping, "Wicketkeeping", "", 0, "Glovework: catches behind and stumpings."},
};
// clang-format on

bool isPaceType(std::optional<BowlType> t) {
  if (!t) return false;
  switch (*t) {
    case BowlType::fast:
    case BowlType::fastmed:
    case BowlType::medium:
    case BowlType::lfast:
    case BowlType::lfastmed:
    case BowlType::lmedium: return true;
    default: return false;
  }
}

namespace {

template <class K>
double weightedMix(const std::vector<SkillDef<K>>& defs, const SkillSet<K>& s) {
  double sum = 0;
  for (const auto& d : defs) sum += d.weight * s[d.key];
  return sum;
}

template <class K>
struct StyleMod {
  std::string_view name;
  std::vector<std::pair<K, double>> mods;
  double get(K k) const {
    for (const auto& m : mods)
      if (m.first == k) return m.second;
    return 0;
  }
};

// clang-format off
const std::vector<StyleMod<BatK>> BAT_STYLES = {
  {"Power hitter", {{BatK::lofted, 0}, {BatK::defence, -8}, {BatK::rotation, -5}, {BatK::grounded, 0}, {BatK::temperament, -4}, {BatK::running, -2}}},
  {"Anchor", {{BatK::defence, 9}, {BatK::rotation, 5}, {BatK::grounded, 4}, {BatK::temperament, 7}, {BatK::running, 2}}},
  {"Accumulator", {{BatK::rotation, 10}, {BatK::running, 9}, {BatK::grounded, 3}, {BatK::defence, 2}}},
  {"Stroke-maker", {{BatK::grounded, 9}, {BatK::vsPace, 5}, {BatK::defence, -2}}},
  {"Finisher", {{BatK::temperament, 8}, {BatK::rotation, 4}, {BatK::running, 3}, {BatK::defence, -4}}},
  {"Spin specialist", {{BatK::vsSpin, 12}, {BatK::rotation, 4}, {BatK::vsPace, -5}}},
  {"Pace specialist", {{BatK::vsPace, 12}, {BatK::grounded, 3}, {BatK::vsSpin, -6}}},
  {"Tailender", {}},
};

const std::vector<StyleMod<BowlK>> PACE_STYLES = {
  {"Swing bowler", {{BowlK::swing, 14}, {BowlK::seam, -3}, {BowlK::pace, -3}}},
  {"Hit-the-deck seamer", {{BowlK::seam, 12}, {BowlK::pace, 4}, {BowlK::swing, -5}}},
  {"Enforcer", {{BowlK::pace, 12}, {BowlK::accuracy, -7}, {BowlK::variations, 2}}},
  {"Death specialist", {{BowlK::death, 14}, {BowlK::variations, 8}, {BowlK::swing, -6}}},
  {"Line-and-length", {{BowlK::accuracy, 11}, {BowlK::pace, -6}, {BowlK::variations, -2}}},
};

const std::vector<StyleMod<BowlK>> SPIN_STYLES = {
  {"Big turner", {{BowlK::turn, 13}, {BowlK::accuracy, -5}}},
  {"Mystery spinner", {{BowlK::variations, 15}, {BowlK::accuracy, -4}, {BowlK::flight, 2}}},
  {"Control spinner", {{BowlK::accuracy, 11}, {BowlK::turn, -5}, {BowlK::variations, -3}}},
  {"Flight and drift", {{BowlK::flight, 13}, {BowlK::accuracy, -3}}},
};
// clang-format on

template <class K>
const StyleMod<K>* findStyle(const std::vector<StyleMod<K>>& styles, std::string_view name) {
  for (const auto& s : styles)
    if (s.name == name) return &s;
  return nullptr;
}

// Nudge a set of sub-skills so their weighted mix lands on `target`, keeping `fixed` keys as they are.
template <class K>
void fitToTarget(const std::vector<SkillDef<K>>& defs, SkillSet<K>& s, double target, std::initializer_list<K> fixed) {
  for (int pass = 0; pass < 4; ++pass) {
    std::vector<const SkillDef<K>*> free;
    for (const auto& d : defs) {
      const bool isFixed = std::find(fixed.begin(), fixed.end(), d.key) != fixed.end();
      if (!isFixed && s[d.key] > 3 && s[d.key] < 99) free.push_back(&d);
    }
    double wFree = 0;
    for (const auto* d : free) wFree += d->weight;
    const double diff = target - weightedMix(defs, s);
    if (std::abs(diff) < 0.3 || wFree == 0) break;
    for (const auto* d : free) s.set(d->key, clamp(js_round(s[d->key] + diff / wFree), 3, 99));
  }
}

int32_t hashSeed(std::string_view id, int32_t seed, int32_t salt) {
  int32_t h = seed ^ salt;
  for (unsigned char ch : id) h = js_imul(h ^ static_cast<int32_t>(ch), kGolden);
  return h;
}

}  // namespace

std::optional<std::string> bowlStyleFor(const SkillSet<BowlK>& bowl, std::optional<BowlType> type) {
  if (!type || bowlingOverall(bowl, type) < 40) return std::nullopt;
  static const std::vector<std::pair<BowlK, std::string_view>> PACE_SIGNATURE = {
      {BowlK::swing, "Swing bowler"}, {BowlK::seam, "Hit-the-deck seamer"}, {BowlK::pace, "Enforcer"},
      {BowlK::death, "Death specialist"}, {BowlK::accuracy, "Line-and-length"}};
  static const std::vector<std::pair<BowlK, std::string_view>> SPIN_SIGNATURE = {
      {BowlK::turn, "Big turner"}, {BowlK::variations, "Mystery spinner"}, {BowlK::accuracy, "Control spinner"},
      {BowlK::flight, "Flight and drift"}};
  const auto& sig = isPaceType(type) ? PACE_SIGNATURE : SPIN_SIGNATURE;
  size_t best = 0;
  for (size_t i = 1; i < sig.size(); ++i)
    if (bowl.get_or(sig[i].first, 0) > bowl.get_or(sig[best].first, 0)) best = i;
  return std::string(sig[best].second);
}

double battingOverall(const SkillSet<BatK>& bat) { return js_round(weightedMix(BAT_SKILLS, bat)); }

double bowlingOverall(const SkillSet<BowlK>& bowl, std::optional<BowlType> type) {
  return js_round(weightedMix(isPaceType(type) ? PACE_SKILLS : SPIN_SKILLS, bowl));
}

double fieldingOverall(const SkillSet<FieldK>& f, bool isKeeper) {
  const double base = (f[FieldK::catching] + f[FieldK::ground] + f[FieldK::throwing]) / 3;
  return js_round(isKeeper ? 0.55 * f[FieldK::keeping] + 0.45 * base : base);
}

std::optional<double> speedKph(const Player& p) {
  if (!p.bowlType) return std::nullopt;
  const auto& b = p.skills.bowl;
  if (isPaceType(p.bowlType)) return js_round(112 + b[BowlK::pace] * 0.42);
  return js_round(78 + (b[BowlK::accuracy] + b[BowlK::flight]) * 0.06);
}

// ---------------------------------------------------------------------------
// Generation (deterministic per player)
// ---------------------------------------------------------------------------

Player deriveSkills(const RawPlayer& p, int32_t seed) {
  Rng rngState(hashSeed(p.id, seed, 0x5eed));
  RandFn rng(rngState);
  auto n = [&](double spread) { return js_round((rng() - 0.5) * 2 * spread); };

  // ---- Batting
  std::string_view batStyle;
  if (p.bat < 45) {
    batStyle = "Tailender";
  } else if (p.power - p.bat > 8) {
    batStyle = "Power hitter";
  } else if (p.bat - p.power > 14) {
    batStyle = "Anchor";
  } else {
    static const std::vector<std::string_view> choices = {"Accumulator", "Stroke-maker", "Finisher",
                                                          "Spin specialist", "Pace specialist", "Anchor"};
    batStyle = pick(rng, choices);
  }
  if (!p.batHint.empty() && findStyle(BAT_STYLES, p.batHint) && batStyle != "Tailender") batStyle = p.batHint;
  const StyleMod<BatK>& mod = *findStyle(BAT_STYLES, batStyle);
  SkillSet<BatK> bat;
  for (const auto& d : BAT_SKILLS) bat.set(d.key, clamp(p.bat + mod.get(d.key) + n(7), 3, 99));
  bat.set(BatK::lofted, clamp(p.power, 3, 99));
  bat.set(BatK::awareness, p.awareness);
  const bool tail = batStyle == "Tailender";
  if (tail) bat.set(BatK::running, clamp(bell(rng, 25, 70), 3, 99));
  // Tailenders get everything scaled down together; proper batters keep their hitting and awareness
  if (tail)
    fitToTarget(BAT_SKILLS, bat, p.bat, {});
  else
    fitToTarget(BAT_SKILLS, bat, p.bat, {BatK::lofted, BatK::awareness});

  // ---- Bowling
  const bool pace = isPaceType(p.bowlType);
  const auto& defs = pace ? PACE_SKILLS : SPIN_SKILLS;
  const auto& styles = pace ? PACE_STYLES : SPIN_STYLES;
  const StyleMod<BowlK>* bowlStyle = nullptr;
  if (p.bowl >= 40) {
    bowlStyle = &styles[static_cast<size_t>(std::floor(rng() * static_cast<double>(styles.size())))];
    if (pace && p.bowlType == BowlType::lfast && rng() < 0.5) bowlStyle = findStyle(PACE_STYLES, "Swing bowler");
  }
  if (bowlStyle && !p.bowlHint.empty()) {
    if (const auto* hinted = findStyle(styles, p.bowlHint)) bowlStyle = hinted;
  }
  SkillSet<BowlK> bowl;
  for (const auto& d : defs) bowl.set(d.key, clamp(p.bowl + (bowlStyle ? bowlStyle->get(d.key) : 0) + n(7), 3, 99));
  if (pace) {
    // Genuine quicks are quick; medium pacers rely on skill
    const double speedBase = p.bowlType == BowlType::medium ? bell(rng, 25, 55) : bell(rng, 62, 92);
    bowl.set(BowlK::pace, clamp(speedBase + (bowlStyle ? bowlStyle->get(BowlK::pace) : 0), 5, 99));
    if (p.bowlType == BowlType::lfast) bowl.set(BowlK::swing, clamp(bowl[BowlK::swing] + 4, 3, 99));
  }
  if (pace)
    fitToTarget(defs, bowl, p.bowl, {BowlK::pace});
  else
    fitToTarget(defs, bowl, p.bowl, {});

  // ---- Fielding
  const bool isKeeper = p.role == Role::WK;
  SkillSet<FieldK> field;
  field.set(FieldK::catching, clamp(p.field + n(9), 5, 99));
  field.set(FieldK::ground, clamp(p.field + n(9) - std::max(0, p.age - 31) * 2, 5, 99));
  field.set(FieldK::throwing, clamp(p.field + n(9), 5, 99));
  if (isKeeper)
    field.set(FieldK::keeping, clamp(p.field + 4 + n(6), 5, 99));
  else
    field.set(FieldK::keeping, clamp(bell(rng, 10, 40), 5, 99));
  const double fTarget = p.field;
  for (int pass = 0; pass < 3; ++pass) {
    const double diff = fTarget - fieldingOverall(field, isKeeper);
    if (std::abs(diff) < 1) break;
    static const FieldK keeperOrder[] = {FieldK::keeping, FieldK::catching, FieldK::ground, FieldK::throwing};
    static const FieldK fielderOrder[] = {FieldK::catching, FieldK::ground, FieldK::throwing};
    if (isKeeper) {
      for (FieldK k : keeperOrder) field.set(k, clamp(field[k] + diff, 5, 99));
    } else {
      for (FieldK k : fielderOrder) field.set(k, clamp(field[k] + diff, 5, 99));
    }
  }

  Player out;
  out.id = p.id;
  out.name = p.name;
  out.role = p.role;
  out.hand = p.hand;
  out.age = p.age;
  out.bowlType = p.bowlType;
  out.skills = {bat, bowl, field};
  out.batStyle = std::string(batStyle);
  if (bowlStyle) out.bowlStyle = std::string(bowlStyle->name);
  // Headline ratings are exactly the weighted mix of the sub-skills
  out.bat = battingOverall(bat);
  out.bowl = bowlingOverall(bowl, p.bowlType);
  out.field = fieldingOverall(field, isKeeper);
  out.power = bat[BatK::lofted];
  out.awareness = bat[BatK::awareness];
  return out;
}

}  // namespace t20
