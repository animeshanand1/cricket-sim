#include "engine/fitness.hpp"

#include <algorithm>
#include <cmath>

#include "engine/rng.hpp"
#include "engine/skills.hpp"

namespace t20 {

namespace {

struct InjuryKind {
  std::string_view type, label;
  int min, max;
  double w;
};
const std::vector<InjuryKind> PACE_INJURIES = {
    {"side", "Side strain", 3, 6, 3},           {"hamstring", "Hamstring", 2, 5, 3},
    {"ankle", "Ankle sprain", 1, 3, 2},          {"back", "Back stress fracture", 10, 18, 0.6},
    {"shoulder", "Shoulder", 2, 6, 1},
};
const std::vector<InjuryKind> OTHER_INJURIES = {
    {"hamstring", "Hamstring", 2, 5, 3}, {"finger", "Broken finger", 2, 5, 2}, {"knee", "Knee", 3, 8, 1},
    {"concussion", "Concussion", 1, 2, 1}, {"groin", "Groin strain", 1, 4, 2},
};

const InjuryKind& pickInjury(Rng& rng, const Player& p) {
  const auto& list = isPaceType(p.bowlType) && p.bowl >= 45 ? PACE_INJURIES : OTHER_INJURIES;
  double total = 0;
  for (const auto& i : list) total += i.w;
  double x = rng() * total;
  for (const auto& i : list)
    if ((x -= i.w) < 0) return i;
  return list[0];
}

double perOver(const std::optional<BowlType>& t) {
  if (!t) return 2;
  switch (*t) {
    case BowlType::fast:
    case BowlType::lfast: return 4;
    case BowlType::fastmed:
    case BowlType::lfastmed: return 3.5;
    case BowlType::medium:
    case BowlType::lmedium: return 3;
    default: return 2;
  }
}

}  // namespace

std::string_view fitnessLabel(double f) {
  if (f >= 90) return "Fresh";
  if (f >= 75) return "Fit";
  if (f >= 60) return "Tired";
  return "Exhausted";
}

Workload matchWorkload(const Match& match) {
  Workload w;
  w.seed = match.seed;
  for (int side = 0; side < 2; ++side) {
    const MatchTeam& t = match.teams[static_cast<size_t>(side)];
    w.teams.push_back(t.id);
    std::vector<PlayerId> ids;
    auto addUnique = [&](const PlayerId& id) {
      if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    };
    for (const auto& id : t.players) addUnique(id);
    if (t.impact) addUnique(t.impact->in);
    for (const auto& id : ids) {
      const Player& p = match.players.at(id);
      double load = 5;
      if (id == t.keeperId) load += 7;
      for (const Innings& inn : match.innings) {
        if (inn.superOver) continue;
        const BatterInnings* b = inn.batters.find(id);
        if (inn.batting == side && b && b->balls)
          load += b->balls * 0.09 + std::max(0, b->runs - 4 * b->fours - 6 * b->sixes) * 0.12;
        const BowlingFigures* f = inn.bowling.find(id);
        if (inn.bowlingSide == side && f && f->balls) load += (f->balls / 6.0) * perOver(p.bowlType);
      }
      const double ageK = 1 + std::max(0, p.age - 30) * 0.04;
      w.loads.set(id, js_round(load * ageK * 10) / 10);
    }
  }
  return w;
}

double injuryChance(const Player& p, double load) {
  const double f = fitnessOf(p);
  const bool pace = isPaceType(p.bowlType) && p.bowl >= 45;
  double c = 0.006 + load * 0.0004;
  if (pace) c *= 1.7;
  if (f < 80) c *= 1 + (80 - f) / 18;
  c *= 1 + std::max(0, p.age - 30) * 0.06;
  return std::min(0.25, c);
}

std::vector<InjuryNews> applyMatchFitness(World& world, const Workload& workload) {
  return applyMatchFitness(world, workload, world.year);
}

std::vector<InjuryNews> applyMatchFitness(World& world, const Workload& workload, int year) {
  Rng rng(workload.seed ^ 0x1a7e);
  std::vector<InjuryNews> news;
  for (const TeamId& teamId : workload.teams) {
    WorldTeam* team = world.teams.find(teamId);
    if (!team) continue;
    for (const PlayerId& id : team->squad) {
      Player* pp = world.players.find(id);
      if (!pp) continue;
      Player& p = *pp;
      if (p.injury) {
        p.injury->matches -= 1;
        if (p.injury->matches <= 0) {
          p.injury.reset();
          p.fitness = 80;
          continue;
        }
      }
      const double recovery = RECOVERY_PER_GAP - std::max(0, p.age - 31) * 0.8;
      p.fitness = clamp(fitnessOf(p) + recovery, 0, 100);
      const double* load = workload.loads.find(id);
      if (!load) continue;
      p.fitness = clamp(js_round((*p.fitness - *load) * 10) / 10, 5, 100);
      if (rng() < injuryChance(p, *load)) {
        const InjuryKind& kind = pickInjury(rng, p);
        const int matches = kind.min + static_cast<int>(std::floor(rng() * (kind.max - kind.min + 1)));
        p.injury = Injury{std::string(kind.type), std::string(kind.label), matches, year, std::nullopt};
        news.push_back({id, teamId, std::string(kind.label), matches});
      }
    }
  }
  std::vector<InjuryNews> all = news;
  all.insert(all.end(), world.lastInjuries.begin(), world.lastInjuries.end());
  if (all.size() > 20) all.resize(20);
  world.lastInjuries = std::move(all);
  return news;
}

void offseasonRecovery(World& world) {
  for (auto& [id, p] : world.players) {
    p.fitness = 100;
    if (p.injury) {
      // matches > 14: carried over, 14 fewer; otherwise healed
      if (p.injury->matches > 14 && p.injury->matches - 14 > 0)
        p.injury->matches -= 14;
      else
        p.injury.reset();
    }
  }
}

Player applyFatigue(const Player& p) {
  const double f = fitnessOf(p);
  if (f >= 70) return p;
  const double k = (70 - f) / 10;
  Player q = p;
  auto& bat = q.skills.bat;
  auto& bowl = q.skills.bowl;
  auto& field = q.skills.field;
  bat.set(BatK::running, std::max(1.0, bat[BatK::running] - 3 * k));
  bat.set(BatK::defence, std::max(1.0, bat[BatK::defence] - 0.8 * k));
  bat.set(BatK::awareness, std::max(1.0, bat[BatK::awareness] - 0.8 * k));
  if (bowl.contains(BowlK::pace)) bowl.set(BowlK::pace, std::max(1.0, bowl[BowlK::pace] - 2 * k));
  bowl.set(BowlK::accuracy, std::max(1.0, bowl[BowlK::accuracy] - 1.2 * k));
  if (bowl.contains(BowlK::death)) bowl.set(BowlK::death, std::max(1.0, bowl[BowlK::death] - 1.2 * k));
  field.set(FieldK::ground, std::max(1.0, field[FieldK::ground] - 2 * k));
  field.set(FieldK::catching, std::max(1.0, field[FieldK::catching] - 1 * k));
  q.tired = true;
  return q;
}

}  // namespace t20
