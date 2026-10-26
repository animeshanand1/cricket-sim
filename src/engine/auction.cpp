#include "engine/auction.hpp"

#include <algorithm>
#include <cmath>

#include "core/str.hpp"
#include "engine/finance.hpp"
#include "engine/league.hpp"
#include "engine/rng.hpp"
#include "engine/world.hpp"

namespace t20 {

namespace {
constexpr int32_t kSalt = static_cast<int32_t>(0xa0c7);

std::string_view personalityOf(const TeamId& id) { return teamMeta(id).personality; }

struct LotOrder {
  std::vector<PlayerId> lots;
  OrderedMap<PlayerId, std::string> setOf;
};

LotOrder lotOrder(const World& world, const std::vector<PlayerId>& ids) {
  auto P = [&](const PlayerId& id) -> const Player& { return world.players.at(id); };
  std::vector<PlayerId> sorted = ids;
  std::stable_sort(sorted.begin(), sorted.end(),
                   [&](const PlayerId& a, const PlayerId& b) { return marketValue(P(b)) - marketValue(P(a)) < 0; });
  std::vector<PlayerId> marquee;
  for (const auto& id : sorted)
    if (isCapped(P(id)) && marquee.size() < 8) marquee.push_back(id);
  std::vector<PlayerId> rest;
  for (const auto& id : sorted)
    if (std::find(marquee.begin(), marquee.end(), id) == marquee.end()) rest.push_back(id);
  auto set = [&](auto f) {
    std::vector<PlayerId> out;
    for (const auto& id : rest)
      if (f(P(id))) out.push_back(id);
    return out;
  };
  const std::vector<std::pair<std::string, std::vector<PlayerId>>> sets = {
      {"Marquee", marquee},
      {"Batters", set([](const Player& p) { return isCapped(p) && p.role == Role::BAT; })},
      {"All-rounders", set([](const Player& p) { return isCapped(p) && p.role == Role::AR; })},
      {"Wicketkeepers", set([](const Player& p) { return isCapped(p) && p.role == Role::WK; })},
      {"Fast bowlers", set([](const Player& p) { return isCapped(p) && p.role == Role::BOWL && !isSpinner(p); })},
      {"Spinners", set([](const Player& p) { return isCapped(p) && p.role == Role::BOWL && isSpinner(p); })},
      {"Uncapped", set([](const Player& p) { return !isCapped(p); })},
  };
  LotOrder out;
  for (const auto& [name, list] : sets)
    for (const auto& id : list) {
      out.lots.push_back(id);
      out.setOf.set(id, name);
    }
  return out;
}

bool canHold(const World& world, const TeamId& teamId, const Player& p) {
  return static_cast<int>(world.teams.at(teamId).squad.size()) < SQUAD_MAX_AUCTION &&
         (!p.overseas || overseasCount(world, teamId) < MAX_OVERSEAS_SQUAD);
}

double nextPrice(const Lot& lot) { return lot.price ? nextBid(*lot.price) : lot.base; }

bool isUser(const Auction& a, const std::optional<TeamId>& t) { return a.userTeam && t == a.userTeam; }

void openLot(World& world) {
  Auction& a = *world.auction;
  const PlayerId id = a.lots[static_cast<size_t>(a.idx)];
  const Player& p = world.players.at(id);
  Lot lot;
  lot.id = id;
  lot.base = basePrice(p);
  for (const auto& t : TEAMS()) {
    if (isUser(a, t.id) && !a.userAuto) continue;
    lot.vals.set(t.id, teamValuation(world, t.id, p, true, t.personality));
  }
  a.lot = std::move(lot);
}

void bid(World& world, const TeamId& team, double price) {
  Lot& lot = *world.auction->lot;
  lot.price = price;
  lot.leader = team;
  lot.bids.push_back({team, price});
}

bool aiBidStep(World& world) {
  Auction& a = *world.auction;
  Lot& lot = *a.lot;
  const double next = nextPrice(lot);
  std::vector<std::pair<TeamId, double>> cands;
  for (const auto& [t, v] : lot.vals)
    if (t != lot.leader && !lot.out.contains(t) && v >= next) cands.push_back({t, v});
  if (lot.userMax && a.userTeam && !lot.out.contains(*a.userTeam) && lot.leader != a.userTeam && *lot.userMax >= next)
    cands.push_back({*a.userTeam, *lot.userMax});
  if (cands.empty()) return false;
  Rng rng = hashRng(kSalt, "bid", a.year, lot.id, lot.bids.size());
  std::vector<double> weights;
  double total = 0;
  for (const auto& c : cands) {
    weights.push_back(std::max(1.0, c.second - next));
    total += weights.back();
  }
  double x = rng() * total;
  TeamId pick = cands[0].first;
  for (size_t i = 0; i < cands.size(); ++i)
    if ((x -= weights[i]) < 0) {
      pick = cands[i].first;
      break;
    }
  bid(world, pick, next);
  return true;
}

int yearsUntilMega(int year) {
  const int left = MEGA_EVERY - (year % MEGA_EVERY);
  return std::max(1, left == 0 ? MEGA_EVERY : left);
}

void closeLot(World& world) {
  Auction& a = *world.auction;
  a.lot.reset();
  a.idx += 1;
}

void sell(World& world, const TeamId& team, double price, bool rtm = false) {
  Auction& a = *world.auction;
  Player& p = world.players.at(a.lot->id);
  p.team = team;
  p.contract = contract(price, a.type == "mega" ? MEGA_EVERY : yearsUntilMega(world.year),
                        rtm ? ContractVia::rtm : ContractVia::auction, world.year, p.overseas && a.type == "mini");
  p.capped = isCapped(p);
  world.teams.at(team).squad.push_back(p.id);
  const std::string* set = a.setOf.find(p.id);
  a.results.push_back(Sale{p.id, team, price, rtm, a.idx + 1, set ? *set : std::string(), p.formerTeam});
  a.log.insert(a.log.begin(), cat(p.name, ' ', rtm ? "retained by RTM" : "sold", " to ", team, " for ", price));
  closeLot(world);
}

void settleLot(World& world) {
  Auction& a = *world.auction;
  Lot& lot = *a.lot;
  const Player& p = world.players.at(lot.id);
  if (!lot.leader) {
    a.unsold.push_back(lot.id);
    closeLot(world);
    return;
  }
  const auto& former = p.formerTeam;
  const bool rtmOk = a.type == "mega" && former && former != lot.leader && world.teams.at(*former).rtm.value_or(0) > 0 &&
                     canHold(world, *former, p);
  if (rtmOk && lot.stage == "bidding") {
    lot.stage = "rtm-invoke";
    lot.rtmTeam = former;
    return;
  }
  sell(world, *lot.leader, *lot.price);
}

bool aiRtmStep(World& world) {
  Auction& a = *world.auction;
  Lot& lot = *a.lot;
  const Player& p = world.players.at(lot.id);
  auto val = [&](const TeamId& t) -> std::optional<double> {
    if (isUser(a, t) && !a.userAuto) return std::nullopt;
    return teamValuation(world, t, p, true, personalityOf(t));
  };
  const double price = lot.price.value_or(0);
  if (lot.stage == "rtm-invoke" && !(isUser(a, lot.rtmTeam) && !a.userAuto)) {
    const auto v = val(*lot.rtmTeam);
    rtmInvoke(world, v && *v >= price);
    return true;
  }
  if (lot.stage == "rtm-raise" && !(isUser(a, lot.leader) && !a.userAuto)) {
    double v;
    if (const double* x = lot.vals.find(*lot.leader))
      v = *x;
    else if (auto y = val(*lot.leader))
      v = *y;
    else
      v = price;
    const double raise = std::min(v, js_round((price * 1.2) / 5) * 5);
    rtmRaise(world, raise > price ? raise : price);
    return true;
  }
  if (lot.stage == "rtm-match" && !(isUser(a, lot.rtmTeam) && !a.userAuto)) {
    const auto v = val(*lot.rtmTeam);
    rtmMatch(world, v && *v >= price && userCeiling(world, *lot.rtmTeam, p) >= price);
    return true;
  }
  return false;
}

}  // namespace

Auction& startAuction(World& world, const std::optional<TeamId>& userTeam, const std::string& type) {
  const size_t size = type == "mega" ? 170 : 60;
  std::vector<const Player*> pool;
  for (const auto& [id, p] : world.players)
    if (!p.team && !p.retired && p.age <= 38) pool.push_back(&p);
  std::stable_sort(pool.begin(), pool.end(),
                   [](const Player* a, const Player* b) { return marketValue(*b) - marketValue(*a) < 0; });
  std::vector<PlayerId> ids;
  for (size_t i = 0; i < pool.size() && i < size; ++i) ids.push_back(pool[i]->id);
  LotOrder lo = lotOrder(world, ids);
  Auction a;
  a.year = world.year;
  a.type = type;
  a.lots = std::move(lo.lots);
  a.setOf = std::move(lo.setOf);
  a.userTeam = userTeam;
  world.auction = std::move(a);
  return *world.auction;
}

double userCeiling(const World& world, const TeamId& teamId, const Player& p) {
  if (!canHold(world, teamId, p)) return 0;
  const int slotsAfter = std::max(0, SQUAD_MIN_AUCTION - static_cast<int>(world.teams.at(teamId).squad.size()) - 1);
  return std::max(0.0, std::min(PRICE_CAP, purseLeft(world, teamId) - slotsAfter * 30));
}

std::optional<std::string> awaitingUser(const World& world) {
  if (!world.auction || !world.auction->lot || world.auction->userAuto || !world.auction->userTeam) return std::nullopt;
  const Auction& a = *world.auction;
  const Lot& lot = *a.lot;
  const TeamId& me = *a.userTeam;
  const Player& p = world.players.at(lot.id);
  if (lot.stage == "bidding") {
    if (lot.out.contains(me) || lot.leader == me) return std::nullopt;
    if (lot.userMax) return std::nullopt;
    return userCeiling(world, me, p) >= nextPrice(lot) ? std::optional<std::string>("bid") : std::nullopt;
  }
  if (lot.stage == "rtm-invoke" && lot.rtmTeam == me) return "rtm-invoke";
  if (lot.stage == "rtm-raise" && lot.leader == me) return "rtm-raise";
  if (lot.stage == "rtm-match" && lot.rtmTeam == me) return "rtm-match";
  return std::nullopt;
}

void rtmInvoke(World& world, bool yes) {
  if (!world.auction || !world.auction->lot || world.auction->lot->stage != "rtm-invoke") return;
  Lot& lot = *world.auction->lot;
  if (!yes) {
    sell(world, *lot.leader, *lot.price);
    return;
  }
  lot.stage = "rtm-raise";
}

void rtmRaise(World& world, double amount) {
  if (!world.auction || !world.auction->lot || world.auction->lot->stage != "rtm-raise") return;
  Lot& lot = *world.auction->lot;
  const double ceiling = userCeiling(world, *lot.leader, world.players.at(lot.id));
  lot.price = std::max(*lot.price, std::min(amount, ceiling));
  lot.stage = "rtm-match";
}

void rtmMatch(World& world, bool yes) {
  if (!world.auction || !world.auction->lot || world.auction->lot->stage != "rtm-match") return;
  Lot& lot = *world.auction->lot;
  if (yes && userCeiling(world, *lot.rtmTeam, world.players.at(lot.id)) >= *lot.price) {
    WorldTeam& team = world.teams.at(*lot.rtmTeam);
    team.rtm = team.rtm.value_or(0) - 1;
    sell(world, *lot.rtmTeam, *lot.price, true);
  } else {
    sell(world, *lot.leader, *lot.price);
  }
}

std::string runAuction(World& world, int maxLots) {
  Auction& a = *world.auction;
  int lotsDone = 0;
  for (;;) {
    if (!a.lot) {
      if (a.idx >= static_cast<int>(a.lots.size())) return "done";
      if (lotsDone >= maxLots) return "paused";
      openLot(world);
      lotsDone++;
    }
    const auto waiting = awaitingUser(world);
    if (a.lot->stage == "bidding") {
      if (aiBidStep(world)) continue;
      if (waiting) return *waiting;
      settleLot(world);
      continue;
    }
    if (waiting) return *waiting;
    if (!aiRtmStep(world)) return "stuck";  // (JS would loop forever)
  }
}

bool userBid(World& world) {
  if (!world.auction || !world.auction->lot) return false;
  Auction& a = *world.auction;
  Lot& lot = *a.lot;
  if (lot.stage != "bidding" || !a.userTeam || lot.leader == a.userTeam) return false;
  const double next = nextPrice(lot);
  if (userCeiling(world, *a.userTeam, world.players.at(lot.id)) < next) return false;
  bid(world, *a.userTeam, next);
  return true;
}

bool userPass(World& world) {
  if (!world.auction || !world.auction->lot || world.auction->lot->stage != "bidding" || !world.auction->userTeam) return false;
  world.auction->lot->out.set(*world.auction->userTeam, true);
  return true;
}

bool userLimit(World& world, double max) {
  if (!world.auction || !world.auction->lot || world.auction->lot->stage != "bidding" || !world.auction->userTeam) return false;
  Lot& lot = *world.auction->lot;
  lot.userMax = std::min(max, userCeiling(world, *world.auction->userTeam, world.players.at(lot.id)));
  return true;
}

void userAutoRest(World& world) {
  if (!world.auction) return;
  Auction& a = *world.auction;
  a.userAuto = true;
  if (a.lot && a.userTeam) {
    const Player& p = world.players.at(a.lot->id);
    a.lot->vals.set(*a.userTeam, teamValuation(world, *a.userTeam, p, true, "balanced"));
  }
}

AuctionResult finishAuction(World& world, const std::function<Player&(const TeamId&)>& makeJourneyman) {
  std::vector<Signing> signedList;
  auto pool = [&]() {
    std::vector<PlayerId> out;
    for (const auto& id : world.auction->unsold)
      if (!world.players.at(id).team) out.push_back(id);
    return out;
  };
  for (const auto& t : TEAMS()) {
    int guard = 0;
    while (static_cast<int>(world.teams.at(t.id).squad.size()) < SQUAD_MIN_AUCTION && guard++ < 30) {
      std::vector<const Player*> options;
      for (const auto& id : pool()) {
        const Player& p = world.players.at(id);
        if (canHold(world, t.id, p) && purseLeft(world, t.id) >= basePrice(p)) options.push_back(&p);
      }
      std::stable_sort(options.begin(), options.end(), [&](const Player* x, const Player* y) {
        return teamValuation(world, t.id, *y, false, t.personality) - teamValuation(world, t.id, *x, false, t.personality) < 0;
      });
      PlayerId pid = options.empty() ? makeJourneyman(t.id).id : options[0]->id;
      Player& p = world.players.at(pid);
      const double price = std::min(basePrice(p), std::max(30.0, purseLeft(world, t.id)));
      p.team = t.id;
      p.contract = contract(price, 1, ContractVia::base, world.year, p.overseas);
      auto& squad = world.teams.at(t.id).squad;
      if (std::find(squad.begin(), squad.end(), pid) == squad.end()) squad.push_back(pid);
      signedList.push_back({pid, t.id, price});
    }
  }
  AuctionResult res;
  res.type = world.auction->type;
  res.sold = world.auction->results;
  for (const auto& id : world.auction->unsold)
    if (!world.players.at(id).team) res.unsold.push_back(id);
  res.signed_ = std::move(signedList);
  world.auction.reset();
  return res;
}

}  // namespace t20
