#include "engine/weather.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/rng.hpp"

namespace t20 {

const SkyDef& skyDef(Sky s) {
  static const SkyDef defs[] = {
      {"Clear skies", "No help in the air."},
      {"Overcast", "Heavy cloud: the new ball will swing for longer."},
      {"Humid", "Muggy evening: a little swing, and dew is more likely."},
      {"Hot and dry", "The pitch will wear faster; spinners come into it late."},
  };
  return defs[static_cast<size_t>(s)];
}

// Rain: 'pre' (a delayed start, fewer overs each), 'first' (the first innings is cut short and
// the chase is reduced), 'second' (the chase is interrupted, with a revised target or a result).
Weather forecast(int32_t seed, Pitch pitch) {
  int32_t h = seed ^ 0x3ea7;
  h = js_imul(static_cast<int32_t>(static_cast<uint32_t>(h) ^ js_ushr(h, 13)), kGolden);
  Rng rng(h);
  const double x = rng();
  Sky sky;
  if (pitch == Pitch::green)
    sky = x < 0.4 ? Sky::overcast : x < 0.65 ? Sky::humid : x < 0.85 ? Sky::clear : Sky::hot;
  else if (pitch == Pitch::dusty || pitch == Pitch::slow)
    sky = x < 0.1 ? Sky::overcast : x < 0.3 ? Sky::humid : x < 0.55 ? Sky::clear : Sky::hot;
  else
    sky = x < 0.18 ? Sky::overcast : x < 0.4 ? Sky::humid : x < 0.75 ? Sky::clear : Sky::hot;
  const double rainChance = sky == Sky::overcast ? 0.22 : sky == Sky::humid ? 0.12 : sky == Sky::clear ? 0.03 : 0.01;
  Weather w;
  w.sky = sky;
  w.rainChance = rainChance;
  if (rng() < rainChance) {
    const double kind = rng();
    Rain rain;
    if (kind < 0.35) {
      rain.when = RainWhen::pre;
      rain.overs = 5 + static_cast<int>(std::floor(rng() * 12));  // 5-16 overs a side
    } else if (kind < 0.65) {
      rain.when = RainWhen::first;
      rain.atOver = 6 + static_cast<int>(std::floor(rng() * 11));
      rain.lost = 2 + static_cast<int>(std::floor(rng() * 8));
    } else {
      rain.when = RainWhen::second;
      rain.atOver = 3 + static_cast<int>(std::floor(rng() * 14));
      rain.lost = 2 + static_cast<int>(std::floor(rng() * 9));
      rain.abandon = rng() < 0.3;
    }
    w.rain = rain;
  }
  return w;
}

SkyEffect skyEffect(const Weather* weather, int over, bool spin) {
  SkyEffect e;
  if (!weather) return e;
  if (weather->sky == Sky::overcast && !spin) {
    const double early = over < 10 ? 1 : 0.5;
    e.wicket = 1 + 0.08 * early;
    e.boundary = 1 - 0.04 * early;
    e.wide = 1 + 0.12 * early;
  }
  if (weather->sky == Sky::humid && !spin && over < 6) {
    e.wicket = 1.04;
    e.wide = 1.06;
  }
  if (weather->sky == Sky::hot) e.wear = 1.25;
  return e;
}

// ---------------------------------------------------------------------------
// DLS-style resources
// ---------------------------------------------------------------------------

namespace {
constexpr double F[] = {1, 0.93, 0.85, 0.74, 0.62, 0.49, 0.37, 0.27, 0.16, 0.07};
constexpr double B = 0.028;
double Z(double u, double w) {
  if (w >= 10) return 0;
  const double f = F[static_cast<size_t>(w)];
  return f * (1 - js_exp((-B * u) / f));
}
double full() {
  static const double value = Z(20, 0);
  return value;
}
}  // namespace

double resources(double oversLeft, double wickets) {
  return (100 * Z(std::max(0.0, oversLeft), std::min(10.0, wickets))) / full();
}

double firstInningsResources(double from, std::optional<double> to, double atOvers, double wickets) {
  if (!to || *to >= from) return resources(from, 0);
  return resources(from, 0) - resources(from - atOvers, wickets) + resources(*to - atOvers, wickets);
}

std::string oversLabel(int balls) {
  if (balls % 6) return cat(balls / 6, '.', balls % 6);
  return std::to_string(balls / 6);
}

}  // namespace t20
