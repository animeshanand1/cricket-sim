#include "engine/world.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "core/str.hpp"
#include "engine/ageing.hpp"
#include "engine/auction.hpp"
#include "engine/club.hpp"
#include "engine/development.hpp"
#include "engine/finance.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/names.hpp"
#include "engine/skills.hpp"

namespace t20 {

namespace {
constexpr int32_t kSalt = 0x51ed;
HistoryPt snapshot(const Player& p, int year) { return {year, p.age, p.bat, p.bowl, p.field}; }
constexpr Role ROLE_ORDER[] = {Role::WK, Role::BAT, Role::AR, Role::BOWL};

std::unordered_set<std::string> usedNames(const World& world) {
  std::unordered_set<std::string> s;
  for (const auto& [id, p] : world.players) s.insert(p.name);
  return s;
}

void release(World& world, const TeamId& teamId, const PlayerId& id, OffseasonReport* report) {
  auto& squad = world.teams.at(teamId).squad;
  auto it = std::find(squad.begin(), squad.end(), id);
  if (it == squad.end()) return;
  squad.erase(it);
  Player& p = world.players.at(id);
  p.team.reset();
  p.formerTeam = teamId;
  p.contract.reset();
  if (report) report->released.push_back({id, teamId});
}

OffseasonReport* reportOf(World& world) { return world.lastOffseason ? &*world.lastOffseason : nullptr; }

bool rookieThisYear(const Player& p, int year) {
  return p.contract && (p.contract->via == ContractVia::academy || p.contract->via == ContractVia::draft) &&
         p.contract->signed_ == year;
}

void trimSquad(World& world, const TeamId& teamId, size_t size) {
  auto& squad = world.teams.at(teamId).squad;
  std::vector<const Player*> cands;
  for (const auto& id : squad) {
    const Player& p = world.players.at(id);
    if (!rookieThisYear(p, world.year)) cands.push_back(&p);
  }
  std::stable_sort(cands.begin(), cands.end(),
                   [](const Player* a, const Player* b) { return playerValue(*a) - playerValue(*b) < 0; });
  std::vector<PlayerId> ids;
  for (const Player* p : cands) ids.push_back(p->id);
  for (const auto& id : ids) {
    if (squad.size() <= size) break;
    release(world, teamId, id, reportOf(world));
  }
}

void startDraft(World& world) {
  Offseason& o = *world.offseason;
  Rng rng(0xd4af ^ js_imul(o.year, 7919));
  std::vector<PlayerId> pool;
  for (int i = 0; i < DRAFT_POOL; ++i) {
    Player& p = makePlayer(world, rng, MakeKind::prospect);
    p.overseas = false;
    pool.push_back(p.id);
  }
  std::vector<TeamId> picks;
  for (int r = 0; r < DRAFT_ROUNDS; ++r) {
    if (r % 2 == 0)
      picks.insert(picks.end(), o.order.begin(), o.order.end());
    else
      picks.insert(picks.end(), o.order.rbegin(), o.order.rend());
  }
  Draft d;
  d.year = o.year;
  d.pool = pool;
  d.order = picks;
  d.scoutsLeft = SCOUT_REPORTS + academyScouts(world, o.userTeam);
  d.userTeam = o.userTeam;
  d.then = o.then;
  world.draft = std::move(d);
  world.offseason->stage = "draft";
  advanceDraft(world, world.offseason->userTeam);
}

void beginAuction(World& world) {
  Offseason& o = *world.offseason;
  Rng rng(0xe7e7 ^ js_imul(o.year, 15485863));
  for (int i = 0; i < (o.mega ? 24 : 16); ++i) makePlayer(world, rng, MakeKind::entrant);
  startAuction(world, world.offseason->userTeam, world.offseason->mega ? "mega" : "mini");
  world.offseason->stage = "auction";
}

}  // namespace

int roleTarget(Role r) {
  switch (r) {
    case Role::WK: return 2;
    case Role::BAT: return 6;
    case Role::AR: return 5;
    case Role::BOWL: return 7;
  }
  return 0;
}

void recompute(Player& p) { recomputeRatings(p); }

double potentialFor(const Player& p) {
  Rng rngState = hashRng(kSalt, "pot", p.id);
  RandFn rng(rngState);
  const double main = mainRating(p);
  const double room = p.age <= 21 ? bell(rng, 6, 20) : p.age <= 24 ? bell(rng, 3, 12) : p.age <= 27 ? bell(rng, 0, 6) : 0;
  return clamp(main + room, main, 97);
}

World createWorld(const std::vector<Team>& league) {
  World world;
  for (const auto& t : league) {
    WorldTeam wt;
    for (const auto& p : t.squad) wt.squad.push_back(p.id);
    world.teams.set(t.id, wt);
    for (const auto& p : t.squad) {
      Player q = p;
      q.base.reset();
      q.devBase.reset();
      q.edited = false;
      q.team = t.id;
      q.origin = Origin::roster;
      q.retired.reset();
      if (!q.potential) q.potential = potentialFor(q);
      q.history = {snapshot(q, 0)};
      const PlayerId qid = q.id;
      world.players.set(qid, std::move(q));
    }
  }
  initialContracts(world);
  return world;
}

void upgradeWorld(World& world) {
  if (world.version >= WORLD_VERSION) return;
  bool unsigned_ = false;
  for (const auto& [id, p] : world.players)
    if (p.team && !p.contract) unsigned_ = true;
  if (unsigned_) initialContracts(world);
  for (auto& [id, p] : world.players)
    if (p.team && !p.contract) p.contract = contract(30, 1, ContractVia::free, world.year);
  if (world.draft && !world.offseason) {
    Offseason o;
    o.year = world.year;
    o.stage = "draft";
    o.userTeam = world.draft->userTeam;
    o.then = world.draft->then;
    world.offseason = o;
  }
  world.version = WORLD_VERSION;
}

const TeamInfo& teamMeta(const TeamId& id) {
  static const std::vector<TeamInfo> teams = TEAMS();
  for (const auto& t : teams)
    if (t.id == id) return t;
  return teams[0];
}

std::vector<Team> worldTeams(const World& world) {
  std::vector<Team> out;
  for (const auto& info : TEAMS()) {
    Team t;
    static_cast<TeamInfo&>(t) = info;
    t.captainId = clubCaptainOf(world, info.id);
    if (const WorldTeam* wt = world.teams.find(info.id))
      for (const auto& id : wt->squad)
        if (const Player* p = world.players.find(id)) t.squad.push_back(*p);
    out.push_back(std::move(t));
  }
  return out;
}

std::optional<TeamId> teamOfId(const World& world, const PlayerId& id) {
  const Player* p = world.players.find(id);
  if (!p) return std::nullopt;
  if (p->team) return p->team;
  if (p->retired && p->retired->team) return p->retired->team;
  return std::nullopt;
}

std::optional<PlayerId> clubCaptainOf(const World& world, const TeamId& teamId) {
  const WorldTeam* t = world.teams.find(teamId);
  if (!t || !t->captainId) return std::nullopt;
  const Player* p = world.players.find(*t->captainId);
  if (p && p->team == teamId && !p->retired && std::find(t->squad.begin(), t->squad.end(), *t->captainId) != t->squad.end())
    return t->captainId;
  return std::nullopt;
}

bool setClubCaptain(World& world, const TeamId& teamId, const std::optional<PlayerId>& id) {
  WorldTeam* t = world.teams.find(teamId);
  if (!t) return false;
  if (!id) {
    t->captainId.reset();
    return true;
  }
  const Player* p = world.players.find(*id);
  if (!p || p->team != teamId || p->retired || std::find(t->squad.begin(), t->squad.end(), *id) == t->squad.end())
    return false;
  t->captainId = id;
  return true;
}

CreateResult createPlayer(World& world, const TeamId& teamId, const Edit& edit) {
  WorldTeam* t = world.teams.find(teamId);
  if (!t) return {false, "Choose a team.", {}};
  if (world.offseason) return {false, "Finish the off-season first.", {}};
  if (static_cast<int>(t->squad.size()) >= SQUAD_MAX) return {false, cat("The squad is full (", SQUAD_MAX, " players)."), {}};
  const PlayerId id = cat('P', world.nextId++);
  Player tmpl = newPlayerTemplate(edit.role, std::optional<std::optional<BowlType>>(edit.bowlType));
  tmpl.id = id;
  Player shaped = applyEdit(tmpl, edit);
  shaped.edited = false;
  shaped.devBase.reset();
  shaped.preEdit.reset();
  RawPlayer raw;
  raw.id = id;
  raw.role = shaped.role;
  raw.hand = shaped.hand;
  raw.age = shaped.age;
  raw.bat = shaped.bat;
  raw.power = shaped.power;
  raw.bowl = shaped.bowl;
  raw.bowlType = shaped.bowlType;
  raw.field = shaped.field;
  raw.awareness = shaped.awareness;
  shaped.batStyle = deriveSkills(raw, 2014 + world.year).batStyle;
  shaped.bowlStyle = bowlStyleFor(shaped.skills.bowl, shaped.bowlType);
  shaped.overseas = edit.overseas;
  shaped.team = teamId;
  shaped.origin = Origin::custom;
  shaped.retired.reset();
  shaped.joined = world.year;
  shaped.potential = potentialFor(shaped);
  shaped.history = {snapshot(shaped, world.year)};
  shaped.contract = contract(basePrice(shaped), 2, ContractVia::custom, world.year, shaped.overseas);
  world.players.set(id, shaped);
  world.teams.at(teamId).squad.push_back(id);
  return {true, {}, id};
}

Role pickRole(Rng& rng) {
  static constexpr std::pair<Role, double> ROLES[] = {{Role::WK, 0.12}, {Role::BAT, 0.3}, {Role::AR, 0.24}, {Role::BOWL, 0.34}};
  double x = rng();
  for (const auto& [r, w] : ROLES)
    if ((x -= w) < 0) return r;
  return Role::BOWL;
}

Player& makePlayer(World& world, Rng& rngState, MakeKind kind, std::optional<Role> roleIn, double quality) {
  const Role role = roleIn ? *roleIn : pickRole(rngState);
  RandFn rng(rngState);
  const PlayerId id = cat('P', world.nextId++);
  auto used = usedNames(world);
  const std::string nm = freshName(rng, used);
  const int age = static_cast<int>(kind == MakeKind::prospect ? bell(rng, 17, 21)
                                   : kind == MakeKind::entrant ? bell(rng, 21, 30)
                                                               : bell(rng, 24, 32));
  const Hand hand = rng() < 0.3 ? Hand::L : Hand::R;
  auto paceType = [&]() { return rng() < 0.22 ? BowlType::lfast : rng() < 0.6 ? BowlType::fast : BowlType::medium; };
  auto spinType = [&]() {
    static const std::vector<BowlType> s = {BowlType::off, BowlType::off, BowlType::leg, BowlType::sla};
    return pick(rng, s);
  };
  const double roleAdj = role == Role::BOWL ? 5 : role == Role::AR ? 2 : -1;
  const double lo = kind == MakeKind::prospect ? 52 : kind == MakeKind::entrant ? 56 : 50;
  const double hi = kind == MakeKind::prospect ? 92 : kind == MakeKind::entrant ? 88 : 72;
  const double potential = clamp(js_round(bell(rng, lo, hi) + quality + roleAdj), 40, 96);
  const double gap = kind == MakeKind::prospect ? js_round((22 - age) * 2.5 + bell(rng, 4, 14))
                     : kind == MakeKind::entrant ? js_round(std::max(0, 27 - age) * 1.5 + bell(rng, 0, 5))
                                                 : js_round(bell(rng, 0, 4));
  const double now = clamp(potential - gap, 28, 90);
  auto r = [&](double a, double b) { return js_round(bell(rng, a, b)); };
  double bat = 0, bowl = 0, field = 0, power = 0;
  std::optional<BowlType> bowlType;
  switch (role) {
    case Role::WK:
      bat = now;
      bowl = r(2, 10);
      field = clamp(now + r(0, 14), 30, 92);
      power = clamp(now + r(-15, 15), 20, 95);
      break;
    case Role::BAT:
      bat = now;
      bowl = r(4, 30);
      bowlType = rng() < 0.5 ? spinType() : BowlType::medium;
      field = clamp(now + r(-10, 12), 30, 92);
      power = clamp(now + r(-15, 15), 20, 95);
      break;
    case Role::AR:
      bat = clamp(now + r(-8, 4), 25, 90);
      bowl = clamp(now + r(-4, 8), 25, 90);
      bowlType = rng() < 0.5 ? paceType() : spinType();
      field = clamp(now + r(-6, 12), 30, 92);
      power = clamp(bat + r(-10, 18), 20, 95);
      break;
    case Role::BOWL:
      bat = r(8, 34);
      bowl = now;
      bowlType = rng() < 0.62 ? paceType() : spinType();
      field = clamp(now + r(-18, 6), 30, 90);
      power = r(15, 60);
      break;
  }
  RawPlayer raw;
  raw.id = id;
  raw.name = nm;
  raw.role = role;
  raw.hand = hand;
  raw.age = age;
  raw.bat = bat;
  raw.power = power;
  raw.bowl = bowl;
  raw.bowlType = bowlType;
  raw.field = field;
  raw.awareness = clamp(js_round(bell(rng, 25, 60) + (age - 20) * 2), 10, 90);
  Player q = deriveSkills(raw, 2014 + world.year);
  q.overseas = rng() < (kind == MakeKind::entrant ? 0.55 : 0.3);
  q.potential = std::max(potential, mainRating(q));
  q.team.reset();
  q.origin = kind == MakeKind::prospect ? Origin::prospect : kind == MakeKind::entrant ? Origin::entrant : Origin::journeyman;
  q.retired.reset();
  q.joined = world.year;
  q.history = {snapshot(q, world.year)};
  return world.players.set(id, std::move(q));
}

double playerValue(const Player& p, double future) {
  const double main = mainRating(p);
  const double ageAdj = p.age >= 34 ? -(p.age - 33) * 2.5 : p.age <= 23 ? 1 : 0;
  const double allround = p.role == Role::AR ? std::min(p.bat, p.bowl) * 0.15 : p.role == Role::WK ? p.field * 0.05 : 0;
  return main + allround + ageAdj + future * std::max(0.0, p.potential.value_or(main) - main);
}

RoleCounts roleCounts(const World& world, const TeamId& teamId) {
  RoleCounts c;
  for (const auto& id : world.teams.at(teamId).squad) {
    switch (world.players.at(id).role) {
      case Role::WK: ++c.wk; break;
      case Role::BAT: ++c.bat; break;
      case Role::AR: ++c.ar; break;
      case Role::BOWL: ++c.bowl; break;
    }
  }
  return c;
}

Role neediestRole(const World& world, const TeamId& teamId) {
  const RoleCounts counts = roleCounts(world, teamId);
  std::vector<Role> keys(std::begin(ROLE_ORDER), std::end(ROLE_ORDER));
  std::stable_sort(keys.begin(), keys.end(), [&](Role a, Role b) {
    return (counts[a] - roleTarget(a)) - (counts[b] - roleTarget(b)) < 0;
  });
  return keys[0];
}

namespace {
double needBonus(const World& world, const TeamId& teamId, Role role) {
  return std::max(0, roleTarget(role) - roleCounts(world, teamId)[role]) * 6.0;
}
}  // namespace

std::optional<PotentialView> potentialView(const World& world, const Player& p, const std::optional<TeamId>& userTeam) {
  if (!p.potential || !*p.potential) return std::nullopt;
  const bool scouted = (world.draft && world.draft->scouted.contains(p.id)) || (userTeam && p.team == userTeam);
  if (scouted) return PotentialView{true, *p.potential, *p.potential};
  Rng rng = hashRng(kSalt, "scout", p.id);
  const double width = 14;
  const double lo = clamp(js_round(*p.potential - rng() * width), mainRating(p), 99);
  return PotentialView{false, lo, std::min(99.0, lo + width)};
}

std::string_view potentialGrade(double v) {
  return v >= 88 ? "A+" : v >= 82 ? "A" : v >= 76 ? "B+" : v >= 70 ? "B" : v >= 64 ? "C+" : v >= 58 ? "C" : "D";
}

double retireChance(const Player& p) {
  static constexpr double table[] = {0.03, 0.06, 0.12, 0.2, 0.3, 0.42, 0.55};  // ages 33..39
  double c = p.age >= 45 ? 1 : p.age >= 40 ? 0.7 : (p.age >= 33 && p.age <= 39 ? table[p.age - 33] : 0);
  const double main = mainRating(p);
  if (main < 60)
    c *= 1.5;
  else if (main >= 80)
    c *= 0.6;
  return std::min(1.0, c);
}

const OffseasonReport& runOffseason(World& world, std::optional<std::vector<TeamId>> order,
                                    std::optional<TeamId> userTeam, std::optional<std::string> then) {
  world.year += 1;
  const int year = world.year;
  offseasonRecovery(world);
  const bool mega = isMegaYear(year);
  OffseasonReport report;
  report.year = year;
  report.mega = mega;
  const auto teams = TEAMS();

  // 1. Ageing
  for (const auto& t : teams) {
    for (const auto& id : world.teams.at(t.id).squad) {
      Player& p = world.players.at(id);
      const AgeResult r = ageOneYear(p, year);
      p.age = r.age;
      if (!p.skillFrac) p.skillFrac = SkillFrac{};
      auto apply = [&](auto& skills, auto& frac, const auto& deltas) {
        using Set = std::remove_cvref_t<decltype(deltas)>;
        for (size_t i = 0; i < Set::N; ++i) {
          const auto k = static_cast<typename Set::key_type>(i);
          if (!deltas.contains(k)) continue;
          const double raw = clamp(skills[k] + frac.get_or(k, 0) + deltas[k], 1, 99);
          skills.set(k, js_round(raw));
          frac.set(k, js_round((raw - skills[k]) * 100) / 100);
        }
      };
      apply(p.skills.bat, p.skillFrac->bat, r.deltas.bat);
      apply(p.skills.bowl, p.skillFrac->bowl, r.deltas.bowl);
      apply(p.skills.field, p.skillFrac->field, r.deltas.field);
      recompute(p);
      p.history.push_back(snapshot(p, year));
      report.aged.push_back({id, t.id, p.age, {r.before.bat, r.before.bowl, r.before.field}, {p.bat, p.bowl, p.field}});
    }
  }
  // Coaching from the season just played
  clubOffseason(world, report);
  for (auto& [id, p] : world.players)
    if (!p.team && !p.retired) p.age += 1;

  // 2. Retirements
  for (const auto& t : teams) {
    auto& squad = world.teams.at(t.id).squad;
    struct Going {
      PlayerId id;
      double c;
    };
    std::vector<Going> going;
    for (const auto& id : squad) {
      const double c = retireChance(world.players.at(id));
      Rng r = hashRng(kSalt, "retire", id, year);
      if (r() < c) going.push_back({id, c});
    }
    std::stable_sort(going.begin(), going.end(), [&](const Going& a, const Going& b) {
      const double d = b.c - a.c;
      return (d ? d : world.players.at(b.id).age - world.players.at(a.id).age) < 0;
    });
    if (going.size() > MAX_RETIRE_PER_TEAM) going.resize(MAX_RETIRE_PER_TEAM);
    for (const auto& g : going) {
      Player& p = world.players.at(g.id);
      p.retired = Retired{year, p.age, t.id, false};
      p.team.reset();
      squad.erase(std::find(squad.begin(), squad.end(), g.id));
      report.retired.push_back({g.id, t.id, p.age, mainRating(p)});
    }
  }

  // Free agents nobody wants drift out of the game
  for (auto& [id, p] : world.players) {
    if (p.team || p.retired) continue;
    p.unsoldYears = p.unsoldYears.value_or(0) + 1;
    if (p.age >= 33 || *p.unsoldYears >= 3) {
      std::optional<TeamId> team = p.formerTeam ? p.formerTeam : p.drafted ? std::optional<TeamId>(p.drafted->team) : std::nullopt;
      p.retired = Retired{year, p.age, team, true};
    }
  }

  // 3. Contracts run down a year
  for (const auto& t : teams)
    for (const auto& id : world.teams.at(t.id).squad) {
      Player& p = world.players.at(id);
      if (p.contract) p.contract->years -= 1;
    }

  // 4. Academy graduates: one prospect per franchise
  Rng rng(0xacad ^ js_imul(year, 7919));
  for (const auto& t : teams) {
    const Role need = neediestRole(world, t.id);
    const std::optional<Role> role = rng() < 0.6 ? std::optional<Role>(need) : std::nullopt;
    Player& p = makePlayer(world, rng, MakeKind::prospect, role, -4 + academyQuality(world, t.id));
    p.team = t.id;
    p.origin = Origin::academy;
    p.overseas = false;
    p.contract = contract(30, 3, ContractVia::academy, year);
    world.teams.at(t.id).squad.push_back(p.id);
    report.academy.push_back({p.id, t.id});
  }

  Offseason o;
  o.year = year;
  o.stage = mega ? "retention" : "contracts";
  o.mega = mega;
  o.order = order.value_or(std::vector<TeamId>{});
  o.userTeam = userTeam;
  o.then = then;
  world.offseason = o;
  world.lastOffseason = std::move(report);
  return *world.lastOffseason;
}

std::vector<PlayerId> expiring(const World& world, const TeamId& teamId) {
  std::vector<PlayerId> out;
  for (const auto& id : world.teams.at(teamId).squad) {
    const Player& p = world.players.at(id);
    if ((p.contract ? p.contract->years : 0) <= 0) out.push_back(id);
  }
  return out;
}

bool renewPlayer(World& world, const TeamId& teamId, const PlayerId& id, int years) {
  Player* p = world.players.find(id);
  if (!p || p->team != teamId) return false;
  const double demand = renewalPrice(world, teamId, *p, years);
  p->contract = contract(demand, years, ContractVia::renewed, world.year, false);
  if (world.lastOffseason) world.lastOffseason->renewed.push_back({id, teamId, demand, years});
  return true;
}

bool releasePlayer(World& world, const TeamId& teamId, const PlayerId& id) {
  const Player* p = world.players.find(id);
  if (!p || p->team != teamId) return false;
  release(world, teamId, id, reportOf(world));
  return true;
}

void closeContracts(World& world) {
  const Offseason o = *world.offseason;
  for (const auto& t : TEAMS()) {
    for (const auto& id : expiring(world, t.id)) {
      if (o.userTeam && t.id == *o.userTeam) {
        release(world, t.id, id, reportOf(world));
        continue;
      }
      const auto deal = aiRenews(world, t.id, world.players.at(id), t.personality);
      if (deal)
        renewPlayer(world, t.id, id, deal->years);
      else
        release(world, t.id, id, reportOf(world));
    }
    if (!(o.userTeam && t.id == *o.userTeam)) {
      std::vector<PlayerId> deadweight;
      for (const auto& id : world.teams.at(t.id).squad) {
        const Player& p = world.players.at(id);
        if (p.contract && p.contract->cost > 1.6 * marketValue(p) && p.age >= 31) deadweight.push_back(id);
      }
      for (size_t i = 0; i < deadweight.size() && i < 2; ++i) release(world, t.id, deadweight[i], reportOf(world));
      trimSquad(world, t.id, 22);
    }
  }
  startDraft(world);
}

bool setRetention(World& world, const std::vector<PlayerId>& ids) {
  if (!retentionValid(world, ids)) return false;
  world.offseason->retain = ids;
  return true;
}

void closeRetention(World& world) {
  const Offseason o = *world.offseason;
  for (const auto& t : TEAMS()) {
    std::vector<PlayerId> keep;
    if (o.userTeam && t.id == *o.userTeam) {
      for (const auto& id : o.retain)
        if (world.players.at(id).team == t.id) keep.push_back(id);
    } else {
      keep = aiRetention(world, t.id, t.personality);
    }
    const auto costs = retentionCosts(world, keep);
    for (size_t i = 0; i < keep.size(); ++i) {
      world.players.at(keep[i]).contract = contract(costs[i], 3, ContractVia::retained, world.year, false);
      if (world.lastOffseason) world.lastOffseason->retained.push_back({keep[i], t.id, costs[i]});
    }
    const auto squad = world.teams.at(t.id).squad;
    for (const auto& id : squad) {
      const Player& p = world.players.at(id);
      const bool rookie = p.contract && (p.contract->via == ContractVia::academy || p.contract->via == ContractVia::draft) &&
                          p.contract->years > 0;
      if (std::find(keep.begin(), keep.end(), id) != keep.end() || rookie) continue;
      release(world, t.id, id, reportOf(world));
    }
    world.teams.at(t.id).rtm = MAX_RETAIN - static_cast<int>(keep.size());
  }
  startDraft(world);
}

std::optional<TeamId> draftOnTheClock(const World& world) {
  if (world.draft && world.draft->made.size() < world.draft->order.size()) return world.draft->order[world.draft->made.size()];
  return std::nullopt;
}

std::vector<PlayerId> draftAvailable(const World& world) {
  std::vector<PlayerId> out;
  if (!world.draft) return out;
  for (const auto& id : world.draft->pool) {
    const bool taken = std::any_of(world.draft->made.begin(), world.draft->made.end(),
                                   [&](const DraftPick& m) { return m.id == id; });
    if (!taken) out.push_back(id);
  }
  return out;
}

bool scoutProspect(World& world, const PlayerId& id) {
  if (!world.draft) return false;
  Draft& d = *world.draft;
  if (d.scoutsLeft <= 0 || d.scouted.contains(id)) return false;
  d.scouted.set(id, true);
  d.scoutsLeft -= 1;
  return true;
}

bool draftPick(World& world, const TeamId& teamId, const std::optional<PlayerId>& id) {
  if (!world.draft || draftOnTheClock(world) != teamId || !id) return false;
  const auto avail = draftAvailable(world);
  if (std::find(avail.begin(), avail.end(), *id) == avail.end()) return false;
  Draft& d = *world.draft;
  if (static_cast<int>(world.teams.at(teamId).squad.size()) >= SQUAD_MAX) {
    d.made.push_back({teamId, std::nullopt, static_cast<int>(d.made.size()) + 1, true});
    return true;
  }
  d.made.push_back({teamId, id, static_cast<int>(d.made.size()) + 1, false});
  const int no = static_cast<int>(d.made.size());
  Player& p = world.players.at(*id);
  p.team = teamId;
  p.origin = Origin::draft;
  p.drafted = Drafted{d.year, no, teamId};
  p.contract = contract(rookieCost(no), 3, ContractVia::draft, d.year);
  world.teams.at(teamId).squad.push_back(*id);
  if (world.lastOffseason) world.lastOffseason->drafted.push_back({*id, teamId, no});
  return true;
}

std::optional<PlayerId> aiDraftChoice(const World& world, const TeamId& teamId) {
  std::optional<PlayerId> best;
  double bestV = 0;
  for (const auto& id : draftAvailable(world)) {
    const Player& p = world.players.at(id);
    Rng r = hashRng(kSalt, "aiscout", teamId, id);
    const double seen = p.potential.value_or(kNaN) + (r() - 0.5) * 10;
    const double v = seen * 0.7 + mainRating(p) * 0.3 + needBonus(world, teamId, p.role);
    if (!best || v > bestV) {
      best = id;
      bestV = v;
    }
  }
  return best;
}

void advanceDraft(World& world, const std::optional<TeamId>& userTeam) {
  for (;;) {
    const auto t = draftOnTheClock(world);
    if (!t || t == userTeam) break;
    if (!draftPick(world, *t, aiDraftChoice(world, *t))) break;  // (JS would loop forever)
  }
}

void finishDraft(World& world) {
  if (!world.draft) return;
  advanceDraft(world, std::nullopt);
  world.draft.reset();
  if (world.offseason)
    beginAuction(world);
  else
    fillSquads(world);
}

AuctionResult closeAuction(World& world) {
  const int oyear = world.offseason->year;
  Rng rng(0xf111 ^ js_imul(oyear, 104729));
  AuctionResult result = finishAuction(world, [&](const TeamId& teamId) -> Player& {
    return makePlayer(world, rng, MakeKind::journeyman, neediestRole(world, teamId));
  });
  if (world.lastOffseason) {
    world.lastOffseason->auction = result;
    world.lastOffseason->signed_.insert(world.lastOffseason->signed_.end(), result.signed_.begin(), result.signed_.end());
  }
  for (auto& [id, p] : world.players) {
    p.formerTeam.reset();
    if (p.team) p.capped = isCapped(p);
  }
  world.offseason.reset();
  return result;
}

void fillSquads(World& world) {
  Rng rng(0xf111 ^ js_imul(world.year, 104729));
  std::vector<Signing>* signedList = world.lastOffseason ? &world.lastOffseason->signed_ : nullptr;
  for (const auto& t : TEAMS()) {
    for (;;) {
      auto& squad = world.teams.at(t.id).squad;
      const RoleCounts counts = roleCounts(world, t.id);
      std::vector<Role> missing;
      for (Role r : ROLE_ORDER)
        if (counts[r] < (r == Role::WK ? 2 : roleTarget(r) - 1)) missing.push_back(r);
      if (missing.empty() && static_cast<int>(squad.size()) >= SQUAD_MIN) break;
      if (static_cast<int>(squad.size()) >= SQUAD_MAX) break;
      const std::optional<Role> role = missing.empty() ? std::nullopt : std::optional<Role>(missing[0]);
      std::vector<Player*> cands;
      for (auto& [id, p] : world.players)
        if (!p.team && !p.retired && p.age <= 36 && (!role || p.role == *role)) cands.push_back(&p);
      std::stable_sort(cands.begin(), cands.end(), [](const Player* a, const Player* b) {
        return playerValue(*b, 0.2) - playerValue(*a, 0.2) < 0;
      });
      PlayerId pid;
      if (!cands.empty()) {
        pid = cands[0]->id;
      } else {
        const Role r = role ? *role : pickRole(rng);
        pid = makePlayer(world, rng, MakeKind::journeyman, r).id;
      }
      Player& p = world.players.at(pid);
      p.team = t.id;
      p.contract = contract(basePrice(p), 1, ContractVia::base, world.year, p.overseas);
      world.teams.at(t.id).squad.push_back(pid);
      if (signedList) signedList->push_back({pid, t.id, std::nullopt});
    }
  }
}

std::optional<Trend> ratingTrend(const Player& p) {
  if (p.history.size() < 2) return std::nullopt;
  return Trend{p.history.front(), p.history.back()};
}

}  // namespace t20
