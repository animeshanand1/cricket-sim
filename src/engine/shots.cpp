#include "engine/shots.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "engine/tactics.hpp"

namespace t20 {

namespace {
struct ZoneArc {
  double lo, hi, w;
};
// clang-format off
const std::vector<ZoneArc> Z4 = {{-165, -135, 1}, {-110, -75, 2}, {-60, -25, 1.5}, {-12, 12, 1}, {30, 75, 2.5}, {80, 110, 1.5}, {135, 160, 1}};
const std::vector<ZoneArc> Z6 = {{-80, -40, 3}, {-35, 20, 3}, {25, 60, 1.2}, {-110, -85, 0.8}};
const std::vector<ZoneArc> ZRUN = {{-170, -120, 1}, {-110, -60, 1.5}, {-55, -20, 1.5}, {-15, 15, 1}, {20, 60, 1.5}, {65, 110, 1.5}, {120, 170, 1}};
// clang-format on

const ZoneArc& pickZone(const std::vector<ZoneArc>& zones, double u) {
  double total = 0;
  for (const auto& z : zones) total += z.w;
  double x = u * total;
  for (const auto& z : zones) {
    x -= z.w;
    if (x <= 0) return z;
  }
  return zones.back();
}
}  // namespace

Placement shotFor(int32_t key, int runs, std::optional<Out> wicketType, Hand hand) {
  Rng r(key);
  r();
  const double mirror = hand == Hand::L ? -1 : 1;
  double dir = 0, dist = 0;
  if (wicketType) {
    switch (*wicketType) {
      case Out::bowled:
      case Out::lbw:
      case Out::stumped:
      case Out::runout: return {0, 0};
      case Out::caughtKeeper: return {180, 0.12};
      case Out::caughtBowled: return {0, 0.25};
      default: break;
    }
    const ZoneArc& z = pickZone(ZRUN, r());
    dir = z.lo + r() * (z.hi - z.lo);
    dist = 0.55 + r() * 0.4;
  } else if (runs == 6) {
    const ZoneArc& z = pickZone(Z6, r());
    dir = z.lo + r() * (z.hi - z.lo);
    dist = 1.08 + r() * 0.15;
  } else if (runs == 4) {
    const ZoneArc& z = pickZone(Z4, r());
    dir = z.lo + r() * (z.hi - z.lo);
    dist = 1;
  } else if (runs > 0) {
    const ZoneArc& z = pickZone(ZRUN, r());
    dir = z.lo + r() * (z.hi - z.lo);
    dist = runs == 1 ? 0.3 + r() * 0.35 : runs == 2 ? 0.6 + r() * 0.3 : 0.85 + r() * 0.1;
  } else {
    dir = (r() - 0.5) * 120;
    dist = 0.08 + r() * 0.15;
  }
  return {js_round(dir * mirror), js_round(dist * 100) / 100};
}

Region regionOf(double dir) {
  struct R {
    Region key;
    double lo, hi;
  };
  static constexpr R regions[] = {
      {Region::straight, -20, 20}, {Region::offDrive, 20, 50},    {Region::cover, 50, 80},
      {Region::point, 80, 120},    {Region::third, 120, 181},     {Region::midwicket, -70, -20},
      {Region::square, -115, -70}, {Region::fine, -181, -115},
  };
  for (const auto& r : regions)
    if (dir >= r.lo && dir < r.hi) return r.key;
  return Region::straight;
}

double planLean(Plan plan, Region region) {
  using R = Region;
  switch (plan) {
    case Plan::wide:
      switch (region) {
        case R::cover: return 1.6;
        case R::point: return 1.8;
        case R::third: return 1.5;
        case R::offDrive: return 1.2;
        case R::midwicket: return 0.6;
        case R::square: return 0.6;
        case R::fine: return 0.6;
        default: return 1;
      }
    case Plan::bouncers:
      switch (region) {
        case R::square: return 1.9;
        case R::fine: return 1.6;
        case R::third: return 1.4;
        case R::midwicket: return 1.2;
        case R::cover: return 0.6;
        case R::offDrive: return 0.6;
        case R::straight: return 0.7;
        default: return 1;
      }
    case Plan::yorkers:
      switch (region) {
        case R::straight: return 1.6;
        case R::fine: return 1.4;
        case R::third: return 1.4;
        case R::cover: return 0.7;
        case R::point: return 0.7;
        default: return 1;
      }
    case Plan::attack:
      switch (region) {
        case R::straight: return 1.4;
        case R::midwicket: return 1.3;
        case R::offDrive: return 1.2;
        default: return 1;
      }
    case Plan::slower:
      switch (region) {
        case R::midwicket: return 1.4;
        case R::straight: return 1.2;
        case R::offDrive: return 1.1;
        default: return 1;
      }
    case Plan::flight:
      switch (region) {
        case R::straight: return 1.6;
        case R::midwicket: return 1.5;
        case R::offDrive: return 1.3;
        case R::cover: return 1.1;
        default: return 1;
      }
    case Plan::darts:
      switch (region) {
        case R::midwicket: return 1.4;
        case R::square: return 1.4;
        case R::fine: return 1.2;
        case R::point: return 0.7;
        case R::cover: return 0.8;
        default: return 1;
      }
    case Plan::mystery:
      switch (region) {
        case R::midwicket: return 1.2;
        case R::cover: return 1.1;
        default: return 1;
      }
    default: return 1;
  }
}

BatterLean batterLean(const Player& p) {
  const auto& s = p.skills.bat;
  int32_t h = 0;
  for (unsigned char ch : p.id) h = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(h) * 31 + ch));
  const double quirk = static_cast<double>(static_cast<uint32_t>(h) % 1000) / 1000 - 0.5;
  const double off =
      std::max(0.3, std::min(0.7, 0.5 + (s[BatK::grounded] - s[BatK::lofted]) / 160 + quirk * 0.16));
  return {off, hasStrength(p, Strength::puller)};
}

double shotDirection(RandFn r, int runs, const Player& batter, Plan plan, bool spin) {
  const auto& zones = runs == 6 ? Z6 : runs == 4 ? Z4 : ZRUN;
  const BatterLean lean = batterLean(batter);
  std::vector<ZoneArc> weighted;
  weighted.reserve(zones.size());
  for (const auto& z : zones) {
    const double mid = (z.lo + z.hi) / 2;
    const Region reg = regionOf(mid);
    double k = z.w * (mid > 5 ? lean.off * 2 : mid < -5 ? (1 - lean.off) * 2 : 1) * planLean(plan, reg);
    if (lean.puller && (reg == Region::square || reg == Region::midwicket)) k *= 1.3;
    if (spin && (reg == Region::square || reg == Region::fine)) k *= 1.15;  // sweeps
    weighted.push_back({z.lo, z.hi, k});
  }
  const ZoneArc z = pickZone(weighted, r());
  return js_round(z.lo + r() * (z.hi - z.lo));
}

int32_t ballKey(int32_t seed, int innings, int legalBalls, int idxInOver) {
  return js_imul(seed ^ (innings * 7919), 31) ^ js_imul(legalBalls + 1, kGolden) ^ js_imul(idxInOver + 3, 40503);
}

}  // namespace t20
