#pragma once
// Weather: the sky changes how the ball behaves, and rain can shorten a match, with targets
// revised by a DLS-style method (weather.js). The forecast comes from a hash of the match seed,
// so it never disturbs the ball-by-ball random stream.

#include <optional>
#include <string>
#include <string_view>

#include "engine/match_types.hpp"

namespace t20 {

struct SkyDef {
  std::string_view label, note;
};
const SkyDef& skyDef(Sky s);

Weather forecast(int32_t seed, Pitch pitch = Pitch::balanced);

struct SkyEffect {
  double wicket = 1, boundary = 1, wide = 1, wear = 1;
};
SkyEffect skyEffect(const Weather* weather, int over, bool spin);

// Share (0-100) of a 20-over innings' resources left with `oversLeft` overs and `wickets` down.
double resources(double oversLeft, double wickets);
// Resources the first innings actually had, given an interruption at `atOvers` with `wickets`
// down that cut it from `from` to `to` overs.
double firstInningsResources(double from, std::optional<double> to = std::nullopt, double atOvers = 0,
                             double wickets = 0);
inline int dlsTarget(double runs, double r1, double r2) { return static_cast<int>(std::floor((runs * r2) / r1)) + 1; }
inline int dlsPar(double runs, double r1, double r2used) { return static_cast<int>(std::floor((runs * r2used) / r1)); }

std::string oversLabel(int balls);

}  // namespace t20
