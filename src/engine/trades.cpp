#include "engine/trades.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/str.hpp"
#include "engine/ageing.hpp"
#include "engine/club.hpp"
#include "engine/finance.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/world.hpp"

namespace t20 {

namespace {
constexpr size_t LOG_KEEP = 40;
constexpr Role ROLES[] = {Role::WK, Role::BAT, Role::AR, Role::BOWL};

std::vector<const Player*> squadOf(const World& world, const TeamId& teamId) {
  std::vector<const Player*> out;
  for (const auto& id : world.teams.at(teamId).squad)
    if (const Player* p = world.players.find(id)) out.push_back(p);
  return out;
}
void logTransfer(World& world, Transfer entry) {
  entry.year = world.year;
  world.transfers.insert(world.transfers.begin(), std::move(entry));
  if (world.transfers.size() > LOG_KEEP) world.transfers.resize(LOG_KEEP);
}
std::string lowerAll(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
void move(World& world, const PlayerId& id, const TeamId& from, const TeamId& to) {
  auto& squad = world.teams.at(from).squad;
  squad.erase(std::find(squad.begin(), squad.end(), id));
  world.teams.at(to).squad.push_back(id);
  Player& p = world.players.at(id);
  p.team = to;
  if (p.contract) p.contract->via = ContractVia::trade;
}
}  // namespace

TradeWindow tradeWindow(const Season* season) {
  if (!season) return {"none", false, 0};
  int played = 0;
  for (const auto& f : season->fixtures)
    if (f.stage == Stage::league && f.result) ++played;
  if (played < TRADE_OPEN) return {"before", false, TRADE_OPEN - played};
  if (played >= TRADE_CLOSE) return {"closed", false, 0};
  return {"open", true, TRADE_CLOSE - played};
}

int tradesMade(const World& world, const TeamId& teamId) {
  int n = 0;
  for (const auto& t : world.transfers)
    if (t.kind == "trade" && t.team == teamId && t.year == world.year) ++n;
  return n;
}

double tradeValue(const World& world, const TeamId& teamId, const Player& p, bool joining) {
  std::vector<double> ranked;
  for (const Player* x : squadOf(world, teamId)) ranked.push_back(worth(*x));
  std::stable_sort(ranked.begin(), ranked.end(), [](double a, double b) { return b - a < 0; });
  auto at = [&](size_t i) { return i < ranked.size() ? ranked[i] : 0.0; };
  double v = marketValue(p) * personalityFactor(teamMeta(teamId).personality, p);
  const double w = worth(p);
  if (joining)
    v *= w > at(10) ? 1.1 : 0.75;
  else
    v *= w >= at(2) ? 1.35 : w >= at(10) ? 1.1 : 0.8;
  v += (marketValue(p) - (p.contract ? p.contract->cost : 0)) * 0.25;
  return std::max(15.0, js_round(v));
}

TradeVerdict evaluateTrade(const World& world, const TeamId& teamId, const TeamId& otherId, const Offer& offer) {
  TradeVerdict v;
  const WorldTeam* mine = world.teams.find(teamId);
  const WorldTeam* theirs = world.teams.find(otherId);
  if (!mine || !theirs || teamId == otherId) {
    v.errors.push_back("Choose another franchise to deal with.");
    return v;
  }
  std::vector<const Player*> out, inn;
  for (const auto& id : offer.give) out.push_back(world.players.find(id));
  for (const auto& id : offer.get) inn.push_back(world.players.find(id));
  auto& errors = v.errors;
  if (offer.give.empty() && offer.get.empty()) errors.push_back("Pick at least one player to trade.");
  if (offer.give.size() > MAX_PER_SIDE || offer.get.size() > MAX_PER_SIDE)
    errors.push_back(cat("No more than ", static_cast<int>(MAX_PER_SIDE), " players each way."));
  const bool moved = std::any_of(out.begin(), out.end(), [&](const Player* p) { return !p || p->team != teamId; }) ||
                     std::any_of(inn.begin(), inn.end(), [&](const Player* p) { return !p || p->team != otherId; });
  if (moved) errors.push_back("A player in this deal has moved clubs.");
  if (!errors.empty()) return v;

  if (tradesMade(world, teamId) >= MAX_TRADES) errors.push_back(cat("You have made your ", MAX_TRADES, " trades for this season."));
  std::vector<const Player*> both = out;
  both.insert(both.end(), inn.begin(), inn.end());
  if (std::any_of(both.begin(), both.end(), [](const Player* p) { return isInjured(*p); }))
    errors.push_back("Injured players cannot be traded.");
  auto after = [&](const TeamId& team, const std::vector<const Player*>& leaving, const std::vector<const Player*>& joining) {
    std::vector<const Player*> s;
    for (const Player* p : squadOf(world, team))
      if (std::find(leaving.begin(), leaving.end(), p) == leaving.end()) s.push_back(p);
    s.insert(s.end(), joining.begin(), joining.end());
    return s;
  };
  const std::pair<std::vector<const Player*>, std::string> checks[] = {{after(teamId, out, inn), "Your squad"},
                                                                       {after(otherId, inn, out), "Their squad"}};
  for (const auto& [squad, who] : checks) {
    if (squad.size() < SQUAD_FLOOR) errors.push_back(cat(who, " would drop below ", static_cast<int>(SQUAD_FLOOR), " players."));
    if (static_cast<int>(squad.size()) > SQUAD_MAX_AUCTION) errors.push_back(cat(who, " would go over ", SQUAD_MAX_AUCTION, " players."));
    if (std::count_if(squad.begin(), squad.end(), [](const Player* p) { return p->overseas; }) > MAX_OVERSEAS_SQUAD)
      errors.push_back(cat(who, " would have more than ", MAX_OVERSEAS_SQUAD, " overseas players."));
    double wages = 0;
    for (const Player* p : squad) wages += p->contract ? p->contract->cost : 0;
    if (wages > PURSE) errors.push_back(cat(who == "Your squad" ? "Your" : "Their", " purse cannot cover the wages."));
    if (std::none_of(squad.begin(), squad.end(), [](const Player* p) { return p->role == Role::WK; }))
      errors.push_back(cat(who, " would be left without a wicketkeeper."));
  }
  if (offer.cash > 0 && offer.cash > available(world, clubOf(world, teamId)))
    errors.push_back("Your club account cannot cover that fee.");
  if (offer.cash < -MAX_CASH_BACK) errors.push_back("No club pays that much in a mid-season deal.");

  double theirIn = 0;
  for (const Player* p : out) theirIn += tradeValue(world, otherId, *p, true);
  theirIn += offer.cash >= 0 ? offer.cash : offer.cash * 1.25;
  double theirOut = 0;
  for (const Player* p : inn) theirOut += tradeValue(world, otherId, *p, false);
  const double gap = theirOut * PREMIUM - theirIn;
  v.ok = errors.empty();
  v.accept = errors.empty() && gap <= 0;
  v.shortBy = std::max(0.0, std::ceil(gap / CASH_STEP) * CASH_STEP);
  v.theirIn = js_round(theirIn);
  v.theirOut = js_round(theirOut);
  return v;
}

TradeVerdict makeTrade(World& world, const TeamId& teamId, const TeamId& otherId, const Offer& offer) {
  TradeVerdict v = evaluateTrade(world, teamId, otherId, offer);
  if (!v.accept) return v;
  for (const auto& id : offer.give) move(world, id, teamId, otherId);
  for (const auto& id : offer.get) move(world, id, otherId, teamId);
  auto names = [&](const std::vector<PlayerId>& ids) {
    std::string s;
    for (size_t i = 0; i < ids.size(); ++i) s += (i ? " and " : "") + world.players.at(ids[i]).name;
    return s;
  };
  if (offer.cash)
    transferMoney(world, teamId, offer.cash,
                  offer.cash > 0 ? cat("Transfer fee paid to ", otherId) : cat("Transfer fee received from ", otherId));
  Transfer t;
  t.kind = "trade";
  t.team = teamId;
  t.with = otherId;
  t.got = offer.get;
  t.gave = offer.give;
  t.cash = offer.cash;
  t.text = cat(offer.get.empty() ? cat("Sold to ", otherId) : cat(names(offer.get), " from ", otherId),
               offer.give.empty() ? std::string() : cat(" for ", names(offer.give)));
  logTransfer(world, std::move(t));
  v.done = true;
  return v;
}

void topUpFreeAgents(World& world) {
  int count[4] = {0, 0, 0, 0};
  for (const auto& [id, p] : world.players)
    if (!p.team && !p.retired) ++count[static_cast<int>(p.role)];
  Rng rng(0x7ade ^ js_imul(world.year, 7919) ^ js_imul(world.nextId, 31));
  for (Role role : ROLES)
    for (int n = count[static_cast<int>(role)]; n < FREE_AGENTS_PER_ROLE; ++n)
      makePlayer(world, rng, n == 0 ? MakeKind::entrant : MakeKind::journeyman, role);
}

std::vector<PlayerId> replaceable(const World& world, const TeamId& teamId) {
  std::vector<PlayerId> out;
  for (const Player* p : squadOf(world, teamId))
    if (isInjured(*p) && p->injury->matches >= REPLACE_MIN_MATCHES && !p->injury->replacedBy) out.push_back(p->id);
  return out;
}

std::optional<std::string> replacementBlock(const World& world, const TeamId& teamId, const Player* p) {
  const auto& squad = world.teams.at(teamId).squad;
  if (!p || p->team || p->retired) return "He is no longer available.";
  if (static_cast<int>(squad.size()) >= SQUAD_MAX_AUCTION) return cat("Your squad is full (", SQUAD_MAX_AUCTION, ").");
  if (p->overseas && overseasCount(world, teamId) >= MAX_OVERSEAS_SQUAD)
    return cat("You already have ", MAX_OVERSEAS_SQUAD, " overseas players.");
  if (payroll(world, teamId) + basePrice(*p) > PURSE) return "Not enough purse left for his fee.";
  return std::nullopt;
}

std::vector<ReplacementOption> replacementOptions(const World& world, const TeamId& teamId, const PlayerId& injuredId,
                                                  size_t limit) {
  const Player* hurt = world.players.find(injuredId);
  if (!hurt) return {};
  std::vector<const Player*> free;
  for (const auto& [id, p] : world.players)
    if (!p.team && !p.retired) free.push_back(&p);
  std::stable_sort(free.begin(), free.end(), [&](const Player* a, const Player* b) {
    const int d = static_cast<int>(b->role == hurt->role) - static_cast<int>(a->role == hurt->role);
    return (d ? d : mainRating(*b) - mainRating(*a)) < 0;
  });
  std::vector<ReplacementOption> out;
  for (size_t i = 0; i < free.size() && i < limit; ++i)
    out.push_back({free[i]->id, basePrice(*free[i]), replacementBlock(world, teamId, free[i])});
  return out;
}

bool signReplacement(World& world, const TeamId& teamId, const PlayerId& injuredId, const PlayerId& id) {
  Player* hurt = world.players.find(injuredId);
  if (!hurt || hurt->team != teamId) return false;
  const auto rep = replaceable(world, teamId);
  if (std::find(rep.begin(), rep.end(), injuredId) == rep.end()) return false;
  Player* p = world.players.find(id);
  if (replacementBlock(world, teamId, p)) return false;
  p->team = teamId;
  p->contract = contract(basePrice(*p), 1, ContractVia::replacement, world.year, p->overseas);
  world.teams.at(teamId).squad.push_back(id);
  hurt->injury->replacedBy = id;
  Transfer t;
  t.kind = "replacement";
  t.team = teamId;
  t.got = {id};
  t.for_ = injuredId;
  t.text = cat(p->name, " signed as cover for ", hurt->name, " (", lowerAll(hurt->injury->label), ")");
  logTransfer(world, std::move(t));
  return true;
}

void aiReplacements(World& world, const std::optional<TeamId>& userTeam) {
  for (const auto& t : TEAMS()) {
    if (userTeam && t.id == *userTeam) continue;
    for (const auto& hurtId : replaceable(world, t.id)) {
      const Player& hurt = world.players.at(hurtId);
      if (hurt.injury->matches < AI_REPLACE_MATCHES ||
          static_cast<int>(world.teams.at(t.id).squad.size()) >= SQUAD_MAX_AUCTION - 3)
        continue;
      const Player* best = nullptr;
      std::vector<const Player*> cands;
      for (const auto& [id, p] : world.players)
        if (!p.team && !p.retired && p.role == hurt.role && !replacementBlock(world, t.id, &p)) cands.push_back(&p);
      std::stable_sort(cands.begin(), cands.end(), [](const Player* a, const Player* b) {
        return playerValue(*b, 0.1) - playerValue(*a, 0.1) < 0;
      });
      if (!cands.empty()) best = cands[0];
      if (best) signReplacement(world, t.id, hurtId, best->id);
    }
  }
}

}  // namespace t20
