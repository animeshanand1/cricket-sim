#include "engine/fans.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/pitch.hpp"

namespace t20 {

double groundCapacity(const TeamId& id) {
  static const std::pair<std::string_view, double> table[] = {
      {"CHK", 38000}, {"MUI", 33000}, {"RGC", 35000}, {"KNR", 66000}, {"DLC", 41000},
      {"PBK", 26000}, {"RJR", 30000}, {"SNH", 55000}, {"GUT", 100000}, {"LKG", 50000},
  };
  for (const auto& [k, v] : table)
    if (k == id) return v;
  return 40000;
}

namespace {
// Two steady numbers in [0, 1) from the match seed
double seeded(int32_t seed, int32_t salt) {
  int32_t h = js_imul(seed ^ salt, kGolden);
  h = static_cast<int32_t>(static_cast<uint32_t>(h) ^ js_ushr(h, 15));
  h = js_imul(h, static_cast<int32_t>(2246822519u));
  h = static_cast<int32_t>(static_cast<uint32_t>(h) ^ js_ushr(h, 13));
  return static_cast<double>(static_cast<uint32_t>(h) % 10000) / 10000;
}

// How the batting side is doing, 0 (hopeless) to 100 (cruising)
double outlook(const Match& match, const Innings& inn) {
  const double left = inn.maxBalls - inn.legalBalls;
  if (inn.target) {
    const double need = *inn.target - inn.runs;
    if (need <= 0) return 97;
    if (left <= 0 || inn.wickets >= inn.maxWickets) return 3;
    const double inHand = inn.maxWickets - inn.wickets;
    const double can = 6.4 + 0.42 * inHand + (left < 30 ? (30 - left) * 0.06 : 0);  // a rate this side can keep up
    const double p = 1 / (1 + js_exp(((need * 6) / left - can) * 0.7));
    return 8 + 84 * p;
  }
  if (inn.superOver || inn.legalBalls < 6) return 50;
  const double par = pitchDef(match.pitch).par * (inn.maxBalls / 120.0);
  const double parRate = (par * 6) / inn.maxBalls;
  const double rate = 0.5 * ((inn.runs * 6.0) / inn.legalBalls) + 0.5 * parRate;
  const double behind = std::max(0.0, inn.wickets - (static_cast<double>(inn.legalBalls) / inn.maxBalls) * 6);
  const double projected = inn.runs + (left / 6) * rate * (1 - 0.045 * behind);
  return 50 + clamp(((projected - par) / par) * 220, -38, 38);
}
}  // namespace

Fans createFans(const Match& match, const GroundInfo& ground) {
  const std::optional<TeamId> home = ground.home ? *ground.home : std::optional<TeamId>(match.teams[0].id);
  int homeIdx = -1;
  if (home)
    for (int i = 0; i < 2; ++i)
      if (match.teams[static_cast<size_t>(i)].id == *home) {
        homeIdx = i;
        break;
      }
  const double seats = ground.capacity ? *ground.capacity : home ? groundCapacity(*home) : 40000;
  const double fill = ground.big ? 0.93 + 0.06 * seeded(match.seed, 0xfa) : 0.74 + 0.2 * seeded(match.seed, 0xfa);
  const double total = js_round((seats * fill) / 10) * 10;
  const double u = seeded(match.seed, 0xb5);
  // share of the crowd backing the first-named or home side
  const double share = homeIdx < 0 ? 0.42 + 0.16 * u : 0.66 + 0.14 * u;
  const size_t first = homeIdx < 0 ? 0 : static_cast<size_t>(homeIdx);
  Fans f;
  if (homeIdx >= 0) f.home = homeIdx;
  f.capacity = seats;
  f.base[first] = js_round((total * share) / 10) * 10;
  f.base[1 - first] = total - f.base[first];
  f.now = f.base;
  f.overStart = f.base;
  f.mood = {50, 50};
  return f;
}

std::vector<std::string> updateFans(Match& match, int batRuns, bool wicket, bool overDone) {
  Fans& f = match.fans;
  const Innings& inn = match.innings[static_cast<size_t>(match.current)];
  const size_t bat = static_cast<size_t>(inn.batting);
  const size_t bowl = static_cast<size_t>(inn.bowlingSide);
  const double t = outlook(match, inn);
  const double batJolt = wicket ? -5 : batRuns == 6 ? 3 : batRuns == 4 ? 2 : 0;
  const double bowlJolt = wicket ? 6 : batRuns == 6 ? -1.5 : batRuns == 4 ? -1 : 0;
  f.mood[bat] = clamp(f.mood[bat] + (t - f.mood[bat]) * 0.1 + batJolt, 3, 97);
  f.mood[bowl] = clamp(f.mood[bowl] + (100 - t - f.mood[bowl]) * 0.1 + bowlJolt, 3, 97);

  // Each side's fans in the ground; late arrivals can't push it past its capacity
  const double a = f.base[0] * (0.6 + 0.008 * f.mood[0]);
  const double b = f.base[1] * (0.6 + 0.008 * f.mood[1]);
  const double squeeze = std::max(1.0, (a + b) / f.capacity);
  f.now[0] = js_round(a / squeeze / 10) * 10;
  f.now[1] = js_round(b / squeeze / 10) * 10;
  if (overDone) f.overStart = f.now;

  std::vector<std::string> notes;
  for (size_t s = 0; s < 2; ++s) {
    const double ratio = f.now[s] / f.base[s];
    if (ratio > 0.8 && ratio < 1.2) continue;
    const std::string key = cat(ratio <= 0.8 ? "out" : "in", s, match.current);
    if (std::find(f.told.begin(), f.told.end(), key) != f.told.end()) continue;
    f.told.push_back(key);
    const std::string& name = match.teams[s].name;
    const std::string n = fmt_indian(f.now[s]);
    notes.push_back(ratio <= 0.8 ? cat(name, " fans are heading for the exits. ", n, " of them are left in the ground.")
                                 : cat("The ", name, " stands are filling up and bouncing: ", n, " fans in full voice."));
  }
  return notes;
}

std::string_view fansMoodLabel(double mood) {
  if (mood >= 78) return "Roaring";
  if (mood >= 60) return "In full voice";
  if (mood >= 42) return "Watching closely";
  if (mood >= 25) return "Gone quiet";
  return "Heading home";
}

}  // namespace t20
