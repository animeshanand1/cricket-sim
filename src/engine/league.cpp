#include "engine/league.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/rng.hpp"
#include "engine/roster.hpp"
#include "engine/skills.hpp"

namespace t20 {

// ---------------------------------------------------------------------------
// Static tables
// ---------------------------------------------------------------------------

const BowlTypeDef& bowlTypeDef(BowlType t) {
  static const BowlTypeDef defs[] = {
      {"Right-arm fast", "RF", false},      {"Right-arm fast-medium", "RFM", false},
      {"Right-arm medium", "RM", false},    {"Left-arm fast", "LF", false},
      {"Left-arm fast-medium", "LFM", false}, {"Left-arm medium", "LM", false},
      {"Off-spin", "OB", true},             {"Leg-spin", "LB", true},
      {"Slow left-arm", "SLA", true},       {"Left-arm wrist spin", "LWS", true},
  };
  return defs[static_cast<size_t>(t)];
}

std::string_view roleLabel(Role r) {
  switch (r) {
    case Role::BAT: return "Batter";
    case Role::WK: return "Wicketkeeper";
    case Role::AR: return "All-rounder";
    default: return "Bowler";
  }
}

std::string_view batStyleFromCode(std::string_view code) {
  if (code == "PH") return "Power hitter";
  if (code == "AN") return "Anchor";
  if (code == "AC") return "Accumulator";
  if (code == "SM") return "Stroke-maker";
  if (code == "FN") return "Finisher";
  if (code == "SS") return "Spin specialist";
  if (code == "PS") return "Pace specialist";
  return {};
}

std::string_view bowlStyleFromCode(std::string_view code) {
  if (code == "SW") return "Swing bowler";
  if (code == "SE") return "Hit-the-deck seamer";
  if (code == "EN") return "Enforcer";
  if (code == "DE") return "Death specialist";
  if (code == "LL") return "Line-and-length";
  if (code == "BT") return "Big turner";
  if (code == "MY") return "Mystery spinner";
  if (code == "CS") return "Control spinner";
  if (code == "FD") return "Flight and drift";
  return {};
}

std::vector<TeamInfo> TEAMS() {
  std::vector<TeamInfo> out;
  for (const auto& t : ROSTER()) out.push_back(t.info);
  return out;
}

bool isSpinner(const Player& p) { return p.bowlType && bowlTypeDef(*p.bowlType).spin; }

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

namespace {

// Roster ratings are on a "how good is he" scale; the engine is tuned to the league averages,
// so hitting power and batting are eased slightly (CALIBRATE = { power: -8, bat: -2 }).
constexpr double kCalibratePower = -8;
constexpr double kCalibrateBat = -2;

// Game awareness (0-100), drawn from its own seeded stream.
double awarenessFor(const std::string& id, Role role, int age, int32_t seed) {
  int32_t h = seed;
  for (unsigned char ch : id) h = js_imul(h ^ static_cast<int32_t>(ch), kGolden);
  Rng rngState(h);
  RandFn rng(rngState);
  double lo = 0, hi = 0;
  switch (role) {
    case Role::WK: lo = 45; hi = 92; break;
    case Role::BAT: lo = 48; hi = 94; break;
    case Role::AR: lo = 40; hi = 88; break;
    case Role::BOWL: lo = 22; hi = 72; break;
  }
  const double base = bell(rng, lo, hi);
  // Older players have usually seen more situations
  return clamp(base + js_round((age - 26) * 0.8), 10, 98);
}

}  // namespace

std::vector<Team> generateLeague(int32_t seed) {
  std::vector<Team> league;
  for (const RosterTeam& rt : ROSTER()) {
    Team team;
    static_cast<TeamInfo&>(team) = rt.info;
    int n = 0;
    for (const RosterRow& row : rt.players) {
      ++n;
      RawPlayer raw;
      raw.id = cat(rt.info.id, '-', n);
      raw.name = std::string(row.name);
      raw.role = row.role;
      raw.hand = row.hand;
      raw.age = row.age;
      raw.bat = clamp(row.bat + (row.bat >= 45 ? kCalibrateBat : 0), 5, 97);
      raw.power = clamp(row.power + (row.power >= 45 ? kCalibratePower : kCalibratePower / 2), 5, 97);
      raw.bowl = row.bowl;
      raw.bowlType = row.bowlType;
      raw.field = row.field;
      raw.awareness = awarenessFor(raw.id, row.role, row.age, seed);
      raw.batHint = batStyleFromCode(row.batStyle);
      raw.bowlHint = bowlStyleFromCode(row.bowlStyle);
      Player p = deriveSkills(raw, seed);
      p.overseas = row.overseas;
      team.squad.push_back(std::move(p));
    }
    league.push_back(std::move(team));
  }
  return league;
}

TeamRatings teamRatings(const Team& team) {
  std::vector<double> bats, bowls;
  for (const auto& p : team.squad) {
    bats.push_back(p.bat);
    bowls.push_back(p.bowl);
  }
  std::stable_sort(bats.begin(), bats.end(), [](double a, double b) { return b - a < 0; });
  std::stable_sort(bowls.begin(), bowls.end(), [](double a, double b) { return b - a < 0; });
  auto top = [](const std::vector<double>& arr, int n) {
    double s = 0;
    for (int i = 0; i < n && i < static_cast<int>(arr.size()); ++i) s += arr[static_cast<size_t>(i)];
    return s / n;
  };
  return {js_round(top(bats, 7)), js_round(top(bowls, 5))};
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

namespace {
using PList = std::vector<const Player*>;

bool contains(const PList& v, const Player* p) { return std::find(v.begin(), v.end(), p) != v.end(); }

// Stable sort with a JS-style comparator returning a number (negative = a first)
template <class Cmp>
void jsSort(PList& v, Cmp cmp) {
  std::stable_sort(v.begin(), v.end(), [&](const Player* a, const Player* b) { return cmp(a, b) < 0; });
}
}  // namespace

BestXI pickBestXI(const Team& team, Pitch pitchType) {
  const double spinBoost = pitchType == Pitch::dusty ? 8 : pitchType == Pitch::green ? -8 : 0;
  auto bowlValue = [&](const Player* p) { return p->bowl + (isSpinner(*p) ? spinBoost : -spinBoost / 2); };
  auto allValue = [&](const Player* p) { return p->bat * 0.55 + bowlValue(p) * 0.55; };
  // Tired players are rested if there's a decent alternative
  auto tired = [](const Player* p) {
    const double f = p->fitness.value_or(100);
    return f < 65 ? (65 - f) * 0.6 : 0.0;
  };
  auto byDesc = [&](auto f) {
    return [&, f](const Player* a, const Player* b) { return (f(b) - tired(b)) - (f(a) - tired(a)); };
  };
  auto batValue = [](const Player* p) { return p->bat + p->power * 0.2; };

  PList squad;
  for (const auto& p : team.squad)
    if (!p.unavailable) squad.push_back(&p);
  auto filterRole = [&](Role r) {
    PList out;
    for (const Player* p : squad)
      if (p->role == r) out.push_back(p);
    return out;
  };

  PList keepers = filterRole(Role::WK);
  jsSort(keepers, byDesc([](const Player* p) { return p->bat; }));
  PList batters = filterRole(Role::BAT);
  jsSort(batters, byDesc(batValue));
  if (batters.size() > 4) batters.resize(4);
  PList ars = filterRole(Role::AR);
  jsSort(ars, byDesc(allValue));
  if (ars.size() > 3) ars.resize(3);
  PList bowlers = filterRole(Role::BOWL);
  jsSort(bowlers, byDesc(bowlValue));
  if (bowlers.size() > 3) bowlers.resize(3);

  PList xi;
  if (!keepers.empty()) xi.push_back(keepers[0]);
  xi.insert(xi.end(), batters.begin(), batters.end());
  xi.insert(xi.end(), ars.begin(), ars.end());
  xi.insert(xi.end(), bowlers.begin(), bowlers.end());

  // Squads change shape over the seasons: fill any gaps with the best players left, making sure
  // there is a keeper and at least five bowling options
  auto overall = [&](const Player* p) {
    return std::max({batValue(p), bowlValue(p) + 8, allValue(p)});
  };
  auto rest = [&]() {
    PList out;
    for (const Player* p : squad)
      if (!contains(xi, p)) out.push_back(p);
    return out;
  };
  const bool hasWK = std::any_of(xi.begin(), xi.end(), [](const Player* p) { return p->role == Role::WK; });
  if (!hasWK) {
    PList pool = rest();
    jsSort(pool, byDesc([](const Player* p) { return p->field + p->bat; }));
    if (!pool.empty()) xi.insert(xi.begin(), pool[0]);
  }
  auto bowlingOpts = [&]() {
    return std::count_if(xi.begin(), xi.end(), [](const Player* p) { return p->bowl >= 50; });
  };
  while (xi.size() < 11) {
    PList pool = rest();
    if (pool.empty()) break;
    if (bowlingOpts() < 5)
      jsSort(pool, byDesc(bowlValue));
    else
      jsSort(pool, byDesc(overall));
    xi.push_back(pool[0]);
  }
  // Still short of 11 fit players: the side takes the field anyway, with the nearest-to-fit injured
  PList injured;
  for (const auto& p : team.squad)
    if (p.unavailable && !contains(xi, &p)) injured.push_back(&p);
  jsSort(injured, [](const Player* a, const Player* b) {
    const double am = a->injury ? a->injury->matches : 0;
    const double bm = b->injury ? b->injury->matches : 0;
    return am - bm;
  });
  size_t nextInjured = 0;
  while (xi.size() < 11 && nextInjured < injured.size()) xi.push_back(injured[nextInjured++]);

  const Player* wk = nullptr;
  for (const Player* p : xi)
    if (p->role == Role::WK) {
      wk = p;
      break;
    }
  if (!wk && !xi.empty()) {
    PList copy = xi;
    jsSort(copy, byDesc([](const Player* p) { return p->field; }));
    wk = copy[0];
  }

  // IPL: at most 4 overseas players in the XI. Swap out the overseas player whose best local
  // replacement (same role) costs the least.
  auto value = [&](const Player* p) {
    return p->role == Role::BOWL ? bowlValue(p) : p->role == Role::AR ? allValue(p) : batValue(p);
  };
  auto overseasCount = [&]() {
    return std::count_if(xi.begin(), xi.end(), [](const Player* p) { return p->overseas; });
  };
  while (overseasCount() > MAX_OVERSEAS) {
    const Player* bestOut = nullptr;
    const Player* bestIn = nullptr;
    double bestLoss = 0;
    for (const Player* out : xi) {
      if (!out->overseas) continue;
      PList cands;
      for (const Player* p : squad)
        if (!p->overseas && !contains(xi, p) && p->role == out->role) cands.push_back(p);
      if (cands.empty()) {
        for (const Player* p : squad)
          if (!p->overseas && !contains(xi, p) && p->role != Role::WK) cands.push_back(p);
      }
      for (const Player* in : cands) {
        const double loss = value(out) - value(in);
        if (!bestOut || loss < bestLoss) {
          bestOut = out;
          bestIn = in;
          bestLoss = loss;
        }
      }
    }
    if (!bestOut) break;
    for (auto& p : xi)
      if (p == bestOut) p = bestIn;
    if (bestOut == wk) {
      if (bestIn->role == Role::WK) {
        wk = bestIn;
      } else {
        const Player* found = nullptr;
        for (const Player* p : xi)
          if (p->role == Role::WK) {
            found = p;
            break;
          }
        wk = found ? found : bestIn;
      }
    }
  }

  BestXI out;
  for (const Player* p : battingOrder(xi)) out.ids.push_back(p->id);
  out.keeperId = wk ? wk->id : PlayerId{};
  // The club captain plays whenever he's fit
  if (!team.captainId || std::find(out.ids.begin(), out.ids.end(), *team.captainId) != out.ids.end()) return out;
  BestXI withCap = withCaptain(team, out.ids, out.keeperId, *team.captainId);
  PList capXi;
  for (const auto& id : withCap.ids)
    for (const auto& p : team.squad)
      if (p.id == id) {
        capXi.push_back(&p);
        break;
      }
  withCap.ids.clear();
  for (const Player* p : battingOrder(capXi)) withCap.ids.push_back(p->id);
  return withCap;
}

BestXI withCaptain(const Team& team, const std::vector<PlayerId>& ids, const PlayerId& keeperId,
                   const PlayerId& captainId) {
  auto byId = [&](const PlayerId& id) -> const Player* {
    for (const auto& p : team.squad)
      if (p.id == id) return &p;
    return nullptr;
  };
  const Player* cap = byId(captainId);
  if (!cap || cap->unavailable || std::find(ids.begin(), ids.end(), captainId) != ids.end()) return {ids, keeperId};
  if (ids.size() < 11) {
    std::vector<PlayerId> more = ids;
    more.push_back(captainId);
    return {more, keeperId.empty() && cap->role == Role::WK ? captainId : keeperId};
  }
  PList xi;
  for (const auto& id : ids)
    if (const Player* p = byId(id)) xi.push_back(p);
  const bool osFull = cap->overseas && std::count_if(xi.begin(), xi.end(), [](const Player* p) { return p->overseas; }) >=
                                           MAX_OVERSEAS;
  PList cands;
  for (const Player* p : xi)
    if ((cap->role == Role::WK || p->id != keeperId) && (!osFull || p->overseas)) cands.push_back(p);
  PList sameRole;
  for (const Player* p : cands)
    if (p->role == cap->role) sameRole.push_back(p);
  if (!sameRole.empty()) cands = sameRole;
  if (cands.empty()) return {ids, keeperId};
  // Injured players go first, then the weakest
  auto worth = [](const Player* p) {
    return p->unavailable ? -1000.0 : std::max({p->bat + p->power * 0.2, p->bowl, (p->bat + p->bowl) * 0.55});
  };
  const Player* out = cands[0];
  for (const Player* p : cands)
    if (worth(p) < worth(out)) out = p;
  std::vector<PlayerId> next = ids;
  for (auto& id : next)
    if (id == out->id) id = captainId;
  return {next, out->id == keeperId ? captainId : keeperId};
}

std::optional<PlayerId> pickCaptain(const std::vector<const Player*>& players) {
  auto score = [](const Player* p) {
    return p->awareness + std::max(p->bat, p->bowl) * 0.25 + std::min(p->age, 36) * 0.5;
  };
  const Player* best = nullptr;
  for (const Player* p : players)
    if (!best || score(p) > score(best)) best = p;
  if (!best) return std::nullopt;
  return best->id;
}

std::vector<PlayerId> benchOf(const Team& team, const std::vector<PlayerId>& xiIds) {
  PList rest;
  for (const auto& p : team.squad)
    if (std::find(xiIds.begin(), xiIds.end(), p.id) == xiIds.end() && !p.unavailable) rest.push_back(&p);
  std::vector<PlayerId> out;
  if (static_cast<int>(rest.size()) <= IMPACT_SUBS) {
    for (const Player* p : rest) out.push_back(p->id);
    return out;
  }
  PList bat = rest;
  jsSort(bat, [](const Player* a, const Player* b) { return b->bat + b->power * 0.2 - (a->bat + a->power * 0.2); });
  PList bowl = rest;
  jsSort(bowl, [](const Player* a, const Player* b) { return b->bowl - a->bowl; });
  for (size_t i = 0; static_cast<int>(out.size()) < IMPACT_SUBS && i < rest.size(); ++i) {
    for (const PList* list : {&bowl, &bat}) {
      const PlayerId& id = (*list)[i]->id;
      if (static_cast<int>(out.size()) < IMPACT_SUBS && std::find(out.begin(), out.end(), id) == out.end())
        out.push_back(id);
    }
  }
  return out;
}

std::vector<const Player*> battingOrder(std::vector<const Player*> players) {
  auto score = [](const Player* p) {
    return p->bat + (p->role == Role::BOWL ? -30 : 0) + (p->role == Role::AR ? -6 : 0);
  };
  jsSort(players, [&](const Player* a, const Player* b) { return score(b) - score(a); });
  return players;
}

}  // namespace t20
