#include "engine/development.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/league.hpp"
#include "engine/skills.hpp"

namespace t20 {

using B = BatK;
using W = BowlK;
using F = FieldK;

const std::vector<Drill> DRILLS = {
    {"technique", "Technique nets", "Batting", "", {{B::defence, 5}, {B::vsPace, 4}, {B::vsSpin, 4}}, {}, {},
     "Tighter defence against pace and spin."},
    {"range", "Range hitting", "Batting", "", {{B::lofted, 7}, {B::grounded, 3}}, {}, {}, "Clearing the rope. More sixes."},
    {"rotation", "Running and rotation", "Batting", "", {{B::rotation, 6}, {B::running, 6}}, {}, {},
     "Working the gaps. Fewer dot balls."},
    {"pressure", "Pressure scenarios", "Batting", "", {{B::awareness, 5}, {B::temperament, 6}}, {}, {},
     "Chases and collapses rehearsed."},
    {"newball", "New-ball swing and seam", "Bowling", "pace", {}, {{W::swing, 6}, {W::seam, 6}}, {},
     "Moving it early. Powerplay wickets."},
    {"death", "Yorkers and slower balls", "Bowling", "pace", {}, {{W::death, 7}, {W::variations, 4}}, {},
     "Nailing the blockhole at the death."},
    {"spincraft", "Turn and flight", "Bowling", "spin", {}, {{W::turn, 6}, {W::flight, 5}}, {},
     "More revs on the ball, more drift."},
    {"lines", "Line and length", "Bowling", "", {}, {{W::accuracy, 6}}, {}, "Hitting the top of off. Fewer bad balls."},
    {"fielding", "Catching and fielding", "Fielding", "", {}, {},
     {{F::catching, 7}, {F::ground, 4}, {F::throwing, 3}, {F::keeping, 5}}, "Safe hands and a sharp arm."},
};

const Drill* drillByKey(std::string_view key) {
  for (const auto& d : DRILLS)
    if (d.key == key) return &d;
  return nullptr;
}

std::vector<const Drill*> drillsFor(const Player& p) {
  const bool spin = isSpinner(p);
  std::vector<const Drill*> out;
  for (const auto& d : DRILLS) {
    if (d.area == "Bowling" && (!p.bowlType || p.bowl < 35)) continue;
    if (d.type == "pace" && spin) continue;
    if (d.type == "spin" && !spin) continue;
    out.push_back(&d);
  }
  return out;
}

namespace {
double clampSkill(double v) { return std::max(1.0, std::min(99.0, js_round(v))); }
double clamp1(double v) { return std::max(-1.0, std::min(1.0, v)); }

const double POS_RUNS[] = {35, 33, 28, 24, 19, 16, 12, 11, 10, 9, 8};
const double POS_SR[] = {147, 140, 136, 133, 132, 140, 137, 141, 100, 99, 97};
const double POS_BAT[] = {81, 79, 76, 75, 71, 69, 65, 60, 27, 24, 20};
constexpr double EXPECTED_WKTS_PER_4 = 1.1;

std::string oversShort(int balls) {
  return balls % 6 ? cat(balls / 6, '.', balls % 6) : cat(balls / 6);
}
}  // namespace

void recomputeRatings(Player& p) {
  p.bat = battingOverall(p.skills.bat);
  p.bowl = bowlingOverall(p.skills.bowl, p.bowlType);
  p.field = fieldingOverall(p.skills.field, p.role == Role::WK);
  p.power = p.skills.bat[BatK::lofted];
  p.awareness = p.skills.bat[BatK::awareness];
}

Player applyTraining(const Player& p, std::string_view drillKey) {
  const Drill* d = drillByKey(drillKey);
  if (!d) return p;
  Player q = p;
  for (const auto& [k, v] : d->bat)
    if (q.skills.bat.contains(k)) q.skills.bat.set(k, clampSkill(q.skills.bat[k] + v));
  for (const auto& [k, v] : d->bowl)
    if (q.skills.bowl.contains(k)) q.skills.bowl.set(k, clampSkill(q.skills.bowl[k] + v));
  for (const auto& [k, v] : d->field)
    if (q.skills.field.contains(k)) q.skills.field.set(k, clampSkill(q.skills.field[k] + v));
  recomputeRatings(q);
  q.training = TrainingTag{std::string(d->key), std::string(d->label)};
  q.startConf = p.startConf.value_or(45) + TRAINING_CONFIDENCE;
  return q;
}

std::vector<TrainingSlot> aiTrainingPlan(const std::vector<const Player*>& players, Pitch pitch) {
  std::vector<TrainingSlot> plan;
  std::vector<PlayerId> used;
  auto isUsed = [&](const Player* p) { return std::find(used.begin(), used.end(), p->id) != used.end(); };
  auto add = [&](const Player* p, const char* drill) {
    if (p && !isUsed(p)) {
      used.push_back(p->id);
      plan.push_back({p->id, drill});
    }
  };
  std::vector<const Player*> batters = players;
  std::stable_sort(batters.begin(), batters.end(), [](const Player* a, const Player* b) { return b->bat - a->bat < 0; });
  const Player* top = batters.empty() ? nullptr : batters[0];
  if (top) add(top, top->batStyle == "Power hitter" || top->batStyle == "Finisher" ? "range" : "technique");
  std::vector<const Player*> bowlers;
  for (const Player* p : players)
    if (p->bowl >= 55) bowlers.push_back(p);
  std::stable_sort(bowlers.begin(), bowlers.end(), [](const Player* a, const Player* b) { return b->bowl - a->bowl < 0; });
  std::vector<const Player*> spinners, pacers;
  for (const Player* p : bowlers) (isSpinner(*p) ? spinners : pacers).push_back(p);
  if ((pitch == Pitch::dusty || pitch == Pitch::slow) && !spinners.empty())
    add(spinners[0], "spincraft");
  else if (!pacers.empty())
    add(pacers[0], "newball");
  std::vector<const Player*> byDeath = pacers;
  std::stable_sort(byDeath.begin(), byDeath.end(), [](const Player* a, const Player* b) {
    return b->skills.bowl[BowlK::death] - a->skills.bowl[BowlK::death] < 0;
  });
  const Player* death = nullptr;
  for (const Player* p : byDeath)
    if (!isUsed(p)) {
      death = p;
      break;
    }
  if (death)
    add(death, "death");
  else if (batters.size() > 1)
    add(batters[1], "rotation");
  if (plan.size() > static_cast<size_t>(MAX_TRAINING_SLOTS)) plan.resize(MAX_TRAINING_SLOTS);
  return plan;
}

double expectedRuns(double bat, int k) { return std::max(4.0, POS_RUNS[k] + 0.4 * (bat - POS_BAT[k])); }
double expectedSR(double bat, int k) { return POS_SR[k] + 0.8 * (bat - POS_BAT[k]); }
double expectedEcon(double bowl) { return 8.5 - (bowl - 70) * 0.085; }

std::optional<double> battingPerformance(const BatterInnings* b, int orderIndex, double rating) {
  if (!b || (b->balls == 0 && !b->out)) return std::nullopt;
  if (orderIndex >= 7 && b->balls < 8) return std::nullopt;
  const double sr = b->balls ? (b->runs * 100.0) / b->balls : 0;
  const double tempoWeight = std::min(1.0, b->balls / 12.0);
  const int k = std::min(10, orderIndex);
  return clamp1((b->runs - expectedRuns(rating, k)) / 35 + ((sr - expectedSR(rating, k)) / 200) * tempoWeight + 0.1);
}

std::optional<double> bowlingPerformance(const BowlingFigures* f, double rating) {
  if (!f || (f->balls < 12 && f->wickets == 0)) return std::nullopt;
  const double econ = (f->runs * 6.0) / std::max(1, f->balls);
  const double overs = f->balls / 6.0;
  return clamp1(0.4 * (f->wickets - EXPECTED_WKTS_PER_4 * (overs / 4)) + (expectedEcon(rating) - econ) / 5 + 0.05);
}

std::optional<std::string_view> performanceLabel(std::optional<double> v) {
  if (!v) return std::nullopt;
  if (*v >= 0.6) return "Excellent";
  if (*v >= 0.2) return "Good";
  if (*v > -0.2) return "Par";
  if (*v > -0.6) return "Poor";
  return "Very poor";
}

namespace {

template <class K>
void bump(SkillSet<K>& prog, K key, double delta, double base) {
  const double cur = prog.get_or(key, 0);
  const double cap = std::max(3.0, std::abs(base) * CAP);
  prog.set(key, std::max(-cap, std::min(cap, cur + delta)));
}

template <class K>
SkillSet<K> addProg(const SkillSet<K>& skills, const SkillSet<K>& prog) {
  SkillSet<K> out = skills;
  for (size_t i = 0; i < SkillSet<K>::N; ++i) {
    const K k = static_cast<K>(i);
    if (prog.contains(k) && out.contains(k)) out.set(k, std::max(1.0, std::min(99.0, out[k] + prog[k])));
  }
  return out;
}

// Headline ratings (unrounded to a tenth) for base skills plus a progress record
Overalls overalls(const Skills& skills, Role role, std::optional<BowlType> bowlType, const Progress* prog) {
  const auto bat = prog ? addProg(skills.bat, prog->bat) : skills.bat;
  const auto bowl = prog ? addProg(skills.bowl, prog->bowl) : skills.bowl;
  const auto f = prog ? addProg(skills.field, prog->field) : skills.field;
  double wb = 0;
  for (const auto& d : BAT_SKILLS) wb += d.weight * bat[d.key];
  double ww = 0;
  for (const auto& d : (isPaceType(bowlType) ? PACE_SKILLS : SPIN_SKILLS)) ww += d.weight * bowl[d.key];
  const double fBase = (f[FieldK::catching] + f[FieldK::ground] + f[FieldK::throwing]) / 3;
  const double fv = role == Role::WK ? 0.55 * f[FieldK::keeping] + 0.45 * fBase : fBase;
  return {js_round(wb * 10) / 10, js_round(ww * 10) / 10, js_round(fv * 10) / 10};
}

struct BatPerf {
  double v;
  std::string line;
  const BatterInnings* b;
};
struct BowlPerf {
  double v;
  std::string line;
};
struct FieldPerf {
  int taken = 0, dropped = 0;
};
struct Perf {
  std::optional<BatPerf> bat;
  std::optional<BowlPerf> bowl;
  std::optional<FieldPerf> field;
};

}  // namespace

std::vector<DevReport> developFromMatch(OrderedMap<PlayerId, Progress>& progress, const Match& match,
                                        const BaseOf& baseOf) {
  std::vector<DevReport> reports;
  OrderedMap<PlayerId, Perf> perf;
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (size_t i = 0; i < inn.battedOrder.size(); ++i) {
      const PlayerId& id = inn.battedOrder[i];
      const Player& mp = match.players.at(id);
      const BatterInnings* b = inn.batters.find(id);
      const auto v = battingPerformance(b, static_cast<int>(i), mp.base ? mp.base->bat : mp.bat);
      if (v) perf[id].bat = BatPerf{*v, cat(b->runs, b->out ? "" : "*", " (", b->balls, ")"), b};
    }
    for (const PlayerId& id : inn.bowlingOrder) {
      const Player& mp = match.players.at(id);
      const BowlingFigures* f = inn.bowling.find(id);
      const auto v = bowlingPerformance(f, mp.base ? mp.base->bowl : mp.bowl);
      if (v) perf[id].bowl = BowlPerf{*v, cat(f->wickets, '/', f->runs, " (", oversShort(f->balls), " ov)")};
    }
    for (const auto& [bid, b] : inn.batters) {
      const auto& d = b.dismissal;
      if (d && d->fielder &&
          (d->type == Out::caught || d->type == Out::caughtBowled || d->type == Out::caughtKeeper ||
           d->type == Out::stumped)) {
        auto& x = perf[*d->fielder].field;
        if (!x) x = FieldPerf{};
        x->taken += 1;
      }
    }
    for (const DropRecord& d : inn.drops) {
      auto& x = perf[d.fielder].field;
      if (!x) x = FieldPerf{};
      x->dropped += 1;
    }
  }

  std::vector<PlayerId> captains;
  for (const auto& t : match.teams)
    if (t.captainId && !t.captainId->empty() &&
        std::find(captains.begin(), captains.end(), *t.captainId) == captains.end())
      captains.push_back(*t.captainId);
  for (const auto& id : captains) perf[id];  // ??= {}
  auto isCaptain = [&](const PlayerId& id) { return std::find(captains.begin(), captains.end(), id) != captains.end(); };
  auto gain = [&](const PlayerId& id, double v) { return isCaptain(id) && v > 0 ? CAPTAIN_GAIN : 1.0; };

  for (const auto& [id, p] : perf) {
    const Skills* base = baseOf(id);
    if (!base) continue;
    Progress& prog = progress[id];
    const Player& mp = match.players.at(id);
    DevReport rep;
    rep.id = id;
    rep.captain = isCaptain(id);
    rep.before = overalls(*base, mp.role, mp.bowlType, &prog);

    if (rep.captain) {
      bump(prog.bat, BatK::awareness, 0.3, base->bat[BatK::awareness]);
      bump(prog.bat, BatK::temperament, 0.2, base->bat[BatK::temperament]);
    }
    if (p.bat) {
      const double d = STEP * p.bat->v * gain(id, p.bat->v);
      const BatterInnings& b = *p.bat->b;
      for (const auto& def : BAT_SKILLS) {
        double k = def.key == BatK::running ? 0.4 : 1;
        if (def.key == BatK::lofted) k = b.sixes >= 2 ? 1.3 : 0.8;
        if (def.key == BatK::grounded) k = b.fours >= 3 ? 1.3 : 0.9;
        if (def.key == BatK::rotation) k = b.balls >= 10 && static_cast<double>(b.dots) / b.balls < 0.3 ? 1.2 : 0.9;
        if (def.key == BatK::awareness) k = 0.5;
        bump(prog.bat, def.key, d * k, base->bat[def.key]);
      }
      rep.bat = DevPerf{p.bat->v, std::string(*performanceLabel(p.bat->v)), p.bat->line, d};
    }
    if (p.bowl) {
      const double d = STEP * p.bowl->v * gain(id, p.bowl->v);
      const auto& defs = base->bowl.contains(BowlK::pace) ? PACE_SKILLS : SPIN_SKILLS;
      for (const auto& def : defs) bump(prog.bowl, def.key, d * (def.key == BowlK::pace ? 0.3 : 1), base->bowl[def.key]);
      rep.bowl = DevPerf{p.bowl->v, std::string(*performanceLabel(p.bowl->v)), p.bowl->line, d};
    }
    if (p.field) {
      const double d = 0.3 * p.field->taken - 0.5 * p.field->dropped;
      const bool keeps = std::any_of(match.teams.begin(), match.teams.end(), [&](const MatchTeam& t) { return t.keeperId == id; });
      const FieldK key = base->field[FieldK::keeping] > 60 && keeps ? FieldK::keeping : FieldK::catching;
      bump(prog.field, key, d, base->field[key]);
      rep.field = DevField{p.field->taken, p.field->dropped, d};
    }
    rep.after = overalls(*base, mp.role, mp.bowlType, &prog);
    reports.push_back(std::move(rep));
  }
  return reports;
}

Player applyProgress(const Player& p, const Progress* prog) {
  if (!prog) return p;
  Player q = p;
  for (size_t i = 0; i < SkillSet<BatK>::N; ++i) {
    const auto k = static_cast<BatK>(i);
    if (prog->bat.contains(k) && q.skills.bat.contains(k)) q.skills.bat.set(k, clampSkill(q.skills.bat[k] + prog->bat[k]));
  }
  for (size_t i = 0; i < SkillSet<BowlK>::N; ++i) {
    const auto k = static_cast<BowlK>(i);
    if (prog->bowl.contains(k) && q.skills.bowl.contains(k))
      q.skills.bowl.set(k, clampSkill(q.skills.bowl[k] + prog->bowl[k]));
  }
  for (size_t i = 0; i < SkillSet<FieldK>::N; ++i) {
    const auto k = static_cast<FieldK>(i);
    if (prog->field.contains(k) && q.skills.field.contains(k))
      q.skills.field.set(k, clampSkill(q.skills.field[k] + prog->field[k]));
  }
  recomputeRatings(q);
  q.base = BaseRatings{p.bat, p.bowl, p.field, p.skills};
  return q;
}

}  // namespace t20
