#include "engine/field.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/league.hpp"
#include "engine/tactics.hpp"

namespace t20 {

const PositionDef& position(Pos p) {
  // clang-format off
  static const PositionDef defs[kPosCount] = {
    {"First slip", 162, 0.26, Zone::close},
    {"Second slip", 152, 0.28, Zone::close},
    {"Gully", 128, 0.3, Zone::close},
    {"Leg slip", -162, 0.26, Zone::close},
    {"Short leg", -85, 0.17, Zone::close},
    {"Silly point", 88, 0.17, Zone::close},
    {"Point", 95, 0.46, Zone::ring},
    {"Cover", 62, 0.46, Zone::ring},
    {"Extra cover", 40, 0.46, Zone::ring},
    {"Mid-off", 14, 0.44, Zone::ring},
    {"Mid-on", -14, 0.44, Zone::ring},
    {"Midwicket", -58, 0.46, Zone::ring},
    {"Square leg", -92, 0.46, Zone::ring},
    {"Short fine leg", -150, 0.42, Zone::ring},
    {"Short third man", 148, 0.46, Zone::ring},
    {"Third man", 150, 0.93, Zone::deep},
    {"Deep point", 95, 0.93, Zone::deep},
    {"Deep cover", 55, 0.93, Zone::deep},
    {"Long-off", 14, 0.93, Zone::deep},
    {"Long-on", -14, 0.93, Zone::deep},
    {"Deep midwicket", -55, 0.93, Zone::deep},
    {"Deep square leg", -95, 0.93, Zone::deep},
    {"Fine leg", -155, 0.93, Zone::deep},
  };
  // clang-format on
  return defs[static_cast<size_t>(p)];
}

using enum Pos;

// clang-format off
const std::vector<PresetDef> PRESETS = {
  {FieldPreset::attacking, "Attacking", "Two slips and a gully: edges carry to hand. Gaps in the ring.",
   {slip1, slip2, gully, point, cover, midOff, midOn, squareLeg, fineLeg}},
  {FieldPreset::balanced, "Balanced", "A slip, a full ring and two men back.",
   {slip1, point, cover, midOff, midOn, midWicket, squareLeg, thirdMan, fineLeg}},
  {FieldPreset::defensive, "Defensive", "Sweepers out: fewer boundaries, easy singles.",
   {point, cover, midOff, midOn, midWicket, shortFineLeg, deepCover, longOn, deepMidWicket}},
  {FieldPreset::death, "Death field", "Five on the rope for the last overs.",
   {point, extraCover, midWicket, shortFineLeg, longOff, longOn, deepMidWicket, deepSquareLeg, deepPoint}},
  {FieldPreset::spin, "Spin attack", "Short leg and silly point for the bat-pad catch.",
   {slip1, shortLeg, sillyPoint, point, cover, midOff, midOn, midWicket, deepMidWicket}},
  {FieldPreset::offside, "Off-side field", "Six on the off side, for bowling wide outside off.",
   {slip1, point, cover, extraCover, midOff, deepPoint, deepCover, midOn, fineLeg}},
};
// clang-format on

const PresetDef& presetByKey(FieldPreset k) {
  for (const auto& p : PRESETS)
    if (p.key == k) return p;
  return PRESETS[1];
}

namespace {
bool has(const std::vector<Pos>& v, Pos p) { return std::find(v.begin(), v.end(), p) != v.end(); }
int countOf(const std::vector<Pos>& v, std::initializer_list<Pos> set) {
  int n = 0;
  for (Pos p : v)
    if (std::find(set.begin(), set.end(), p) != set.end()) ++n;
  return n;
}
}  // namespace

FieldShape describeField(const std::vector<Pos>& positions) {
  FieldShape d;
  for (Pos k : positions) {
    const PositionDef& p = position(k);
    if (p.zone == Zone::close) ++d.close;
    if (p.zone == Zone::ring) ++d.ring;
    if (p.zone == Zone::deep) ++d.deep;
    if (p.angle > 0) ++d.off;
    if (p.angle < -95) ++d.legBehind;
  }
  d.leg = static_cast<int>(positions.size()) - d.off;
  return d;
}

std::vector<std::string> fieldProblems(const std::vector<Pos>& positions, Phase phase) {
  const FieldShape d = describeField(positions);
  std::vector<std::string> out;
  if (positions.size() != 9) out.push_back(cat("Place 9 fielders (", positions.size(), " placed)."));
  if (d.deep > maxDeep(phase))
    out.push_back(cat(d.deep, " outside the circle; the limit is ", maxDeep(phase),
                      phase == Phase::pp ? " in the powerplay" : "", "."));
  if (d.legBehind > 2) out.emplace_back("No more than 2 fielders behind square on the leg side.");
  return out;
}

namespace {
Pos insideOf(Pos k) {
  switch (k) {
    case thirdMan: return shortThirdMan;
    case deepPoint: return point;
    case deepCover: return extraCover;
    case longOff: return midOff;
    case longOn: return midOn;
    case deepMidWicket: return midWicket;
    case deepSquareLeg: return squareLeg;
    case fineLeg: return shortFineLeg;
    default: return k;
  }
}
constexpr Pos kFill[] = {cover, midWicket, point, midOff, midOn, squareLeg, extraCover, gully, slip1};

std::optional<Pos> firstFillMissing(const std::vector<Pos>& pos) {
  for (Pos f : kFill)
    if (!has(pos, f)) return f;
  return std::nullopt;
}
}  // namespace

std::vector<Pos> legalize(const std::vector<Pos>& positions, Phase phase) {
  std::vector<Pos> pos;
  for (Pos k : positions)
    if (!has(pos, k)) pos.push_back(k);
  while (pos.size() < 9) {
    const auto f = firstFillMissing(pos);
    if (!f) break;
    pos.push_back(*f);
  }
  if (pos.size() > 9) pos.resize(9);
  while (describeField(pos).deep > maxDeep(phase)) {
    Pos k = pos[0];
    for (Pos x : pos)
      if (position(x).zone == Zone::deep) {
        k = x;
        break;
      }
    const Pos inside = insideOf(k);
    Pos repl = inside;
    if (has(pos, inside)) repl = firstFillMissing(pos).value_or(inside);
    for (Pos& x : pos)
      if (x == k) x = repl;
  }
  while (describeField(pos).legBehind > 2) {
    Pos k = pos[0];
    for (Pos x : pos)
      if (position(x).angle < -95) {
        k = x;
        break;
      }
    // (the JS can leave a hole here if every fill position is taken, which never happens: the
    // fill list has nobody behind square on the leg side)
    const Pos repl = firstFillMissing(pos).value_or(k);
    for (Pos& x : pos)
      if (x == k) x = repl;
  }
  return pos;
}

FieldEffect fieldEffect(const std::vector<Pos>& positions, const Player& bowler, const Player* batter, Plan plan) {
  const FieldShape d = describeField(positions);
  const bool spin = isSpinner(bowler);
  const int bat = countOf(positions, {shortLeg, sillyPoint});
  const int slips = countOf(positions, {slip1, slip2, gully, legSlip});
  // Close catchers only pay off for the right bowler: slips for pace, bat-pad for spin
  const double catchers = spin ? bat * 1.2 + slips * 0.5 : slips + bat * 0.4;
  const double c = catchers - 1;
  const double dp = d.deep - 2;
  FieldEffect m{
      0.98 + 0.032 * c + 0.01 * dp,  //
      1 - 0.015 * dp + 0.012 * c,    //
      1 + 0.035 * dp - 0.02 * c,     //
      1 + 0.02 * dp,                 //
      1.02 + 0.03 * c,               //
      1 - 0.01 * dp,
  };
  // Working with the plan
  if (plan == Plan::wide) m.o4 *= d.off >= 6 ? 0.9 : d.off <= 4 ? 1.12 : 1;
  if (plan == Plan::bouncers && (has(positions, deepSquareLeg) || has(positions, fineLeg))) {
    m.W *= 1.06;
    m.o6 *= 0.94;
  }
  if (plan == Plan::yorkers && (has(positions, longOn) || has(positions, longOff))) m.o6 *= 0.95;
  if (plan == Plan::flight && (has(positions, longOn) || has(positions, deepMidWicket) || has(positions, longOff))) {
    m.W *= 1.04;
    m.o6 *= 0.92;
  }
  // A big puller with nobody back on the leg side
  if (batter && hasStrength(*batter, Strength::puller) && !has(positions, deepSquareLeg) &&
      !has(positions, deepMidWicket))
    m.o6 *= 1.12;
  for (double* v : {&m.W, &m.o0, &m.o1, &m.o2, &m.o4, &m.o6}) *v = std::max(0.6, *v);
  return m;
}

std::vector<std::pair<Out, double>> fieldDismissals(const std::vector<Pos>& positions,
                                                    std::vector<std::pair<Out, double>> w) {
  const int slips = countOf(positions, {slip1, slip2, gully});
  const int bat = countOf(positions, {shortLeg, sillyPoint, legSlip});
  const int deep = describeField(positions).deep;
  for (auto& kv : w) {
    if (kv.first == Out::caughtKeeper) kv.second *= 1 + 0.15 * slips;
    if (kv.first == Out::caught) kv.second *= 1 + 0.06 * slips + 0.1 * bat + 0.03 * (deep - 2);
  }
  return w;
}

// ---------------------------------------------------------------------------
// Who stands where, and who gets to the ball
// ---------------------------------------------------------------------------

namespace {
double angDiff(double a, double b) {
  const double d = std::fmod(std::abs(a - b), 360.0);
  return d > 180 ? 360 - d : d;
}

double fieldScore(Zone z, const SkillSet<FieldK>& f) {
  switch (z) {
    case Zone::close: return f[FieldK::catching];  // safest hands in the slips and at short leg
    case Zone::deep: return f[FieldK::catching] * 0.5 + f[FieldK::throwing] * 0.3 + f[FieldK::ground] * 0.2;
    default: return f[FieldK::ground] * 0.6 + f[FieldK::throwing] * 0.4;  // stops and run outs
  }
}
int zoneOrder(Zone z) { return z == Zone::close ? 0 : z == Zone::deep ? 1 : 2; }

// A fielder's ground work against the league average
double groundK(const SkillSet<FieldK>& f) {
  return std::min(1.35, std::max(0.7, js_exp((f[FieldK::ground] - 65) / 50)));
}
}  // namespace

std::array<PlayerId, kPosCount> assignFielders(const std::vector<Pos>& positions, const std::vector<PlayerId>& ids,
                                               const OrderedMap<PlayerId, Player>& P) {
  std::vector<PlayerId> left = ids;
  std::array<PlayerId, kPosCount> out{};
  std::vector<Pos> order = positions;
  std::stable_sort(order.begin(), order.end(),
                   [](Pos a, Pos b) { return zoneOrder(position(a).zone) - zoneOrder(position(b).zone) < 0; });
  for (Pos k : order) {
    if (left.empty()) break;
    const Zone z = position(k).zone;
    size_t bi = 0;
    for (size_t i = 1; i < left.size(); ++i)
      if (fieldScore(z, P.at(left[i]).skills.field) > fieldScore(z, P.at(left[bi]).skills.field)) bi = i;
    out[static_cast<size_t>(k)] = left[bi];
    left.erase(left.begin() + static_cast<std::ptrdiff_t>(bi));
  }
  return out;
}

std::optional<Nearest> nearestFielder(const std::vector<Pos>& positions, double dir, std::initializer_list<Zone> zones) {
  std::optional<Nearest> best;
  for (Pos k : positions) {
    const PositionDef& p = position(k);
    if (std::find(zones.begin(), zones.end(), p.zone) == zones.end()) continue;
    const double reach = p.zone == Zone::deep ? 16 : 12;  // degrees a sweeper covers; a ring fielder less
    const double x = angDiff(dir, p.angle) / reach;
    const double cover = js_exp(-(x * x));
    if (!best || cover > best->cover) best = Nearest{k, cover};
  }
  return best;
}

ShotResolution resolveShot(RandFn r, int runs, double dir, const std::vector<Pos>& positions, const FielderAt& fielderAt,
                           bool freeHit) {
  if (runs == 4) {
    const auto deep = nearestFielder(positions, dir, {Zone::deep});
    const auto ring = nearestFielder(positions, dir, {Zone::ring, Zone::close});
    const double dp = deep ? 0.42 * deep->cover : 0;
    const double rp = ring ? 0.2 * ring->cover : 0;
    const bool byDeep = dp >= rp;
    const std::optional<Nearest>& by = byDeep ? deep : ring;
    const Player* f = by ? fielderAt(by->pos) : nullptr;
    if (f && r() < std::max(dp, rp) * groundK(f->skills.field)) {
      const double x = r();
      // A sweeper on the rope keeps it to one or two; a diving stop in the ring to a dot or one
      const int saved = byDeep ? (x < 0.55 ? 2 : x < 0.9 ? 1 : 3) : x < 0.4 ? 0 : 1;
      return {saved, by->pos, true, false};
    }
  } else if (runs == 6 && !freeHit) {
    const auto deep = nearestFielder(positions, dir, {Zone::deep});
    const Player* f = deep && deep->cover > 0.5 ? fielderAt(deep->pos) : nullptr;
    // Not quite enough on it: a catch on the rope, if he holds it
    if (f && r() < 0.05 * deep->cover * js_exp((f->skills.field[FieldK::catching] - 65) / 40))
      return {0, deep->pos, false, true};
  } else if (runs == 2) {
    const auto deep = nearestFielder(positions, dir, {Zone::deep});
    const Player* f = deep ? fielderAt(deep->pos) : nullptr;
    if (f && r() < 0.3 * deep->cover * groundK(f->skills.field)) return {1, deep->pos, true, false};
  } else if (runs == 1) {
    const auto ring = nearestFielder(positions, dir, {Zone::ring, Zone::close});
    const Player* f = ring ? fielderAt(ring->pos) : nullptr;
    if (f && r() < 0.1 * ring->cover * groundK(f->skills.field)) return {0, ring->pos, true, false};
  }
  return {runs, std::nullopt, false, false};
}

namespace {
double catchClose(bool spin, Pos k) {
  if (!spin) {
    switch (k) {
      case slip1: return 3;
      case slip2: return 2.2;
      case gully: return 1.8;
      case legSlip: return 0.8;
      case shortLeg: return 0.6;
      case sillyPoint: return 0.4;
      default: return 0.5;
    }
  }
  switch (k) {
    case slip1: return 1.4;
    case slip2: return 0.6;
    case gully: return 0.8;
    case legSlip: return 1;
    case shortLeg: return 2.4;
    case sillyPoint: return 2;
    default: return 0.5;
  }
}
}  // namespace

std::optional<Pos> catchPosition(RandFn r, const std::vector<Pos>& positions, bool spin,
                                 const std::function<double(double)>& lean) {
  std::vector<std::pair<Pos, double>> w;
  w.reserve(positions.size());
  for (Pos k : positions) {
    const PositionDef& p = position(k);
    const double base = p.zone == Zone::close ? catchClose(spin, k) : p.zone == Zone::deep ? 1.3 : 1;
    w.emplace_back(k, base * (p.zone == Zone::close ? 1 : lean(p.angle)));
  }
  double total = 0;
  for (const auto& kv : w) total += kv.second;
  double x = r() * total;
  for (const auto& kv : w) {
    x -= kv.second;
    if (x <= 0) return kv.first;
  }
  if (w.empty()) return std::nullopt;
  return w.back().first;
}

FieldSet aiField(Captain style, Phase phase, Plan plan, const Player& bowler) {
  const bool spin = isSpinner(bowler);
  FieldPreset preset;
  if (phase == Phase::death)
    preset = FieldPreset::death;
  else if (plan == Plan::wide)
    preset = FieldPreset::offside;
  else if (phase == Phase::pp)
    preset = style == Captain::cautious ? FieldPreset::balanced : spin ? FieldPreset::balanced : FieldPreset::attacking;
  else if (spin && (style == Captain::aggressive || style == Captain::shrewd || style == Captain::spin))
    preset = FieldPreset::spin;
  else
    preset = style == Captain::aggressive ? FieldPreset::balanced : FieldPreset::defensive;
  return {preset, legalize(presetByKey(preset).positions, phase)};
}

}  // namespace t20
