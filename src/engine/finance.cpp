#include "engine/finance.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/ageing.hpp"
#include "engine/league.hpp"
#include "engine/rng.hpp"

namespace t20 {

namespace {
constexpr int32_t kSalt = 0x70a1;
int roleTargetBig(Role r) {
  switch (r) {
    case Role::WK: return 2;
    case Role::BAT: return 6;
    case Role::AR: return 5;
    case Role::BOWL: return 7;
  }
  return 0;
}
// Sort descending like `arr.sort((a, b) => b - a)`
void sortDesc(std::vector<double>& v) {
  std::stable_sort(v.begin(), v.end(), [](double a, double b) { return b - a < 0; });
}
}  // namespace

std::string money(std::optional<double> l) {
  if (!l) return "–";
  if (*l >= 100) {
    std::string s = js_fixed(*l / 100, 2);
    // .replace(/\.?0+$/, '')
    size_t end = s.size();
    while (end > 0 && s[end - 1] == '0') --end;
    if (end < s.size()) {
      if (end > 0 && s[end - 1] == '.') --end;
      s.resize(end);
    }
    return cat("₹", s, " cr");
  }
  return cat("₹", js_round(*l), " L");
}

double nextBid(double price) {
  if (price < 100) return price + 5;
  if (price < 200) return price + 10;
  if (price < 500) return price + 20;
  return price + 25;
}

bool isCapped(const Player& p) {
  if (p.capped) return *p.capped;
  return p.origin == Origin::roster || p.age >= 24 || mainRating(p) >= 75;
}

double worth(const Player& p) {
  const double main = mainRating(p);
  const double ageAdj = p.age >= 33 ? -(p.age - 32) * 2.2 : p.age <= 24 ? 1.5 : 0;
  const double allround = p.role == Role::AR ? std::min(p.bat, p.bowl) * 0.08 : p.role == Role::WK ? 1.5 : 0;
  const double future = p.age <= 26 ? 0.35 * std::max(0.0, p.potential.value_or(main) - main) : 0;
  return main + ageAdj + allround + future;
}

double marketValue(const Player& p) {
  const double v = worth(p);
  return clamp(js_round(30 * js_exp((v - 55) / 8.2) / 5) * 5, 30, PRICE_CAP);
}

double basePrice(const Player& p) {
  const double floor = isCapped(p) ? 75 : 30;
  const double target = marketValue(p) * 0.35;
  double base = floor;
  for (double b : BASE_PRICES)
    if (b <= target && b >= floor) base = b;
  return base;
}

double payroll(const World& world, const TeamId& teamId) {
  double s = 0;
  for (const auto& id : world.teams.at(teamId).squad) {
    const Player& p = world.players.at(id);
    s += p.contract ? p.contract->cost : 0;
  }
  return s;
}
double purseLeft(const World& world, const TeamId& teamId) { return PURSE - payroll(world, teamId); }
int overseasCount(const World& world, const TeamId& teamId) {
  int n = 0;
  for (const auto& id : world.teams.at(teamId).squad)
    if (world.players.at(id).overseas) ++n;
  return n;
}

double personalityFactor(std::string_view kind, const Player& p) {
  const double main = mainRating(p);
  if (kind == "stars") return main >= 82 ? 1.28 : main < 70 ? 0.85 : 0.95;
  if (kind == "moneyball")
    return p.potential.value_or(main) - main >= 6 || (p.age <= 25 && main >= 65) ? 1.2 : main >= 85 ? 0.85 : 0.93;
  if (kind == "youth") return p.age <= 24 ? 1.3 : p.age >= 32 ? 0.7 : 1;
  if (kind == "experience") return p.age >= 30 ? 1.18 : p.age <= 22 ? 0.8 : 1;
  if (kind == "mavericks") return (isSpinner(p) && p.bowl >= 65) || p.power >= 82 ? 1.22 : 0.95;
  if (kind == "bowling") return p.role == Role::BOWL ? 1.18 : p.role == Role::BAT ? 0.9 : 1;
  if (kind == "bigspender") return 1.12;
  return 1;
}

double teamValuation(const World& world, const TeamId& teamId, const Player& p, bool noise,
                     std::string_view personality) {
  const WorldTeam& team = world.teams.at(teamId);
  if (static_cast<int>(team.squad.size()) >= SQUAD_MAX_AUCTION) return 0;
  if (p.overseas && overseasCount(world, teamId) >= MAX_OVERSEAS_SQUAD) return 0;
  double v = marketValue(p) * personalityFactor(personality.empty() ? "balanced" : personality, p);
  std::vector<double> values;
  int count = 0;
  for (const auto& id : team.squad) {
    const Player& x = world.players.at(id);
    values.push_back(worth(x));
    if (x.role == p.role) ++count;
  }
  sortDesc(values);
  const double eleventh = values.size() > 10 ? values[10] : 0;
  const double w = worth(p);
  v *= w > eleventh + 2 ? 1.15 : w > eleventh - 4 ? 0.9 : 0.55;
  const int shortBy = roleTargetBig(p.role) - count;
  v *= shortBy > 0 ? 1 + 0.12 * shortBy : shortBy < -2 ? 0.7 : 1;
  if (noise) {
    Rng r = hashRng(kSalt, "val", teamId, p.id, world.year);
    v *= 0.85 + r() * 0.3;
  }
  const int slotsAfter = std::max(0, SQUAD_MIN_AUCTION - static_cast<int>(team.squad.size()) - 1);
  const double affordable = purseLeft(world, teamId) - slotsAfter * 30;
  return std::max(0.0, std::min({js_round(v), affordable, PRICE_CAP}));
}

Contract contract(double cost, int years, ContractVia via, int year, bool overseas) {
  return {cost, overseas ? std::min(cost, OVERSEAS_FEE_CAP) : cost, years, via, year};
}

void initialContracts(World& world) {
  for (auto& [teamId, t] : world.teams) {
    std::vector<double> raw;
    double total = 0;
    for (const auto& id : t.squad) {
      raw.push_back(marketValue(world.players.at(id)));
      total += raw.back();
    }
    const double scale = std::min(1.0, (PURSE * 0.88) / total);
    for (size_t i = 0; i < t.squad.size(); ++i) {
      Player& p = world.players.at(t.squad[i]);
      Rng r = hashRng(kSalt, "contract", p.id);
      const int years = p.age >= 35 ? 1 : 1 + static_cast<int>(std::floor(r() * 3));
      p.contract = contract(std::max(30.0, js_round((raw[i] * scale) / 5) * 5), years, ContractVia::retained, 0,
                            p.overseas);
      p.capped = isCapped(p);
    }
    t.rtm = 0;
  }
}

double renewalDemand(const Player& p, int years) {
  Rng r = hashRng(kSalt, "demand", p.id, p.age);
  const double base = marketValue(p) * (0.92 + r() * 0.2);
  const double mult = years == 1 ? 1.08 : years == 3 ? 0.95 : 1;
  return clamp(js_round((base * mult) / 5) * 5, 30, PRICE_CAP);
}

std::vector<int> renewalYears(const Player& p) {
  if (p.age >= 34) return {1};
  if (p.age >= 31) return {1, 2};
  return {1, 2, 3};
}

std::optional<RenewDeal> aiRenews(const World& world, const TeamId& teamId, const Player& p,
                                  std::string_view personality) {
  const int years = p.age >= 33 ? 1 : p.age >= 29 ? 2 : 3;
  const double demand = renewalDemand(p, years);
  const double value = marketValue(p) * personalityFactor(personality, p);
  std::vector<double> squadValues;
  for (const auto& id : world.teams.at(teamId).squad) squadValues.push_back(worth(world.players.at(id)));
  sortDesc(squadValues);
  const bool core = worth(p) >= (squadValues.size() > 13 ? squadValues[13] : 0);
  const bool roomy = payroll(world, teamId) + demand <= PURSE * 0.9;
  if (core && roomy && value >= demand * 0.9) return RenewDeal{years, demand};
  return std::nullopt;
}

std::vector<double> retentionCosts(const World& world, const std::vector<PlayerId>& ids) {
  std::vector<double> out;
  size_t capped = 0;
  for (const auto& id : ids) {
    if (!isCapped(world.players.at(id))) {
      out.push_back(RETENTION_UNCAPPED);
      continue;
    }
    const size_t n = std::size(RETENTION_SLABS);
    out.push_back(capped < n ? RETENTION_SLABS[capped] : RETENTION_SLABS[n - 1]);
    ++capped;
  }
  return out;
}

bool retentionValid(const World& world, const std::vector<PlayerId>& ids) {
  int capped = 0;
  for (const auto& id : ids)
    if (isCapped(world.players.at(id))) ++capped;
  const int n = static_cast<int>(ids.size());
  return n <= MAX_RETAIN && capped <= MAX_RETAIN_CAPPED && n - capped <= MAX_RETAIN_UNCAPPED;
}

std::vector<PlayerId> aiRetention(const World& world, const TeamId& teamId, std::string_view personality) {
  std::vector<const Player*> squad;
  for (const auto& id : world.teams.at(teamId).squad) squad.push_back(&world.players.at(id));
  auto val = [&](const Player* p) { return marketValue(*p) * personalityFactor(personality, *p); };
  std::stable_sort(squad.begin(), squad.end(), [&](const Player* a, const Player* b) { return val(b) - val(a) < 0; });
  std::vector<PlayerId> keep;
  for (const Player* p : squad) {
    std::vector<PlayerId> trial = keep;
    trial.push_back(p->id);
    if (!retentionValid(world, trial)) continue;
    const double cost = retentionCosts(world, trial).back();
    if (val(p) >= cost * 0.95 && p->age <= 35) keep.push_back(p->id);
    if (keep.size() >= 4) break;
  }
  return keep;
}

}  // namespace t20
