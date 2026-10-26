// Ports of world.test.js, fitness.test.js, ageing.test.js, finance.test.js (engine parts) and
// club.test.js: the living league, fitness, ageing, money, the auction, the board and the club.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

#include "engine/ageing.hpp"
#include "engine/auction.hpp"
#include "engine/board.hpp"
#include "engine/club.hpp"
#include "engine/finance.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/league_view.hpp"
#include "engine/season.hpp"
#include "engine/sides.hpp"
#include "engine/world.hpp"

using namespace t20;

namespace {
std::vector<TeamId> order() {
  std::vector<TeamId> o;
  for (const auto& t : LEAGUE()) o.push_back(t.id);
  return o;
}
const OrderedMap<PlayerId, Progress> kNoProgress;

const Player& byName(const World& w, const char* n) {
  for (const auto& [id, p] : w.players)
    if (p.name == n) return p;
  FAIL("no player " << n);
  return w.players.begin()->second;
}

AuctionResult fullOffseason(World& w, std::optional<TeamId> userTeam = std::nullopt) {
  runOffseason(w, order(), userTeam, std::nullopt);
  if (w.offseason->stage == "retention")
    closeRetention(w);
  else
    closeContracts(w);
  finishDraft(w);
  if (userTeam) userAutoRest(w);
  runAuction(w);
  return closeAuction(w);
}

void validSquads(const World& w, int min = SQUAD_MIN) {
  for (const Team& t : worldTeams(w)) {
    INFO(t.id);
    CHECK(static_cast<int>(t.squad.size()) >= min);
    CHECK(static_cast<int>(t.squad.size()) <= SQUAD_MAX);
    CHECK(std::count_if(t.squad.begin(), t.squad.end(), [](const Player& p) { return p.overseas; }) <= MAX_OVERSEAS_SQUAD);
    CHECK(purseLeft(w, t.id) >= 0);
    for (const auto& p : t.squad) {
      CHECK(p.contract.has_value());
      CHECK(p.team == t.id);
    }
    const BestXI xi = pickBestXI(t);
    std::vector<PlayerId> ids = xi.ids;
    std::sort(ids.begin(), ids.end());
    CHECK(std::unique(ids.begin(), ids.end()) == ids.end());
    CHECK(xi.ids.size() == 11);
    CHECK(std::find(xi.ids.begin(), xi.ids.end(), xi.keeperId) != xi.ids.end());
  }
}
}  // namespace

// ---------------------------------------------------------------------------
// world.test.js
// ---------------------------------------------------------------------------

TEST_CASE("world: starts from the roster with potential and a rating history", "[world]") {
  const World w = createWorld(LEAGUE());
  CHECK(w.players.size() == 160);
  for (const auto& [id, p] : w.players) {
    CHECK(*p.potential >= mainRating(p));
    CHECK(p.history.size() == 1);
  }
  validSquads(w, 16);
}

TEST_CASE("world: runs an off-season: ageing, retirements, academy, draft, fill", "[world]") {
  World w = createWorld(LEAGUE());
  const World before = w;
  const OffseasonReport rep = runOffseason(w, order(), std::string("MUI"), std::nullopt);
  CHECK(w.year == 1);
  CHECK_FALSE(rep.retired.empty());
  for (const auto& r : rep.retired) CHECK(w.players.at(r.id).retired->team == r.team);
  CHECK(rep.academy.size() == 10);
  CHECK(w.offseason->stage == "contracts");
  closeContracts(w);
  CHECK(w.offseason->stage == "draft");
  CHECK(w.draft->pool.size() == DRAFT_POOL);
  CHECK(w.draft->order.size() == 10 * DRAFT_ROUNDS);
  for (const Team& t : worldTeams(w))
    for (const auto& p : t.squad)
      if (const Player* b = before.players.find(p.id)) CHECK(p.age == b->age + 1);
  // The AI has picked until it's MUI's turn (MUI is second in this order)
  CHECK(draftOnTheClock(w) == std::optional<TeamId>("MUI"));
  CHECK(w.draft->made.size() == 1);
  const PlayerId target = draftAvailable(w)[3];
  CHECK_FALSE(potentialView(w, w.players.at(target), std::string("MUI"))->exact);
  CHECK(scoutProspect(w, target));
  CHECK(potentialView(w, w.players.at(target), std::string("MUI"))->exact);
  CHECK(draftPick(w, "MUI", target));
  CHECK(w.players.at(target).team == "MUI");
  CHECK_FALSE(draftPick(w, "MUI", draftAvailable(w)[0]));  // not MUI's turn
  finishDraft(w);
  CHECK_FALSE(w.draft.has_value());
  CHECK(w.lastOffseason->drafted.size() == 10 * DRAFT_ROUNDS);
  CHECK(w.offseason->stage == "auction");
  userAutoRest(w);
  CHECK(runAuction(w) == "done");
  const AuctionResult res = closeAuction(w);
  CHECK(res.sold.size() > 10);
  CHECK_FALSE(w.offseason.has_value());
  validSquads(w);
}

TEST_CASE("world: keeps a playable, competitive league over ten seasons", "[world]") {
  World w = createWorld(LEAGUE());
  int megas = 0;
  for (int y = 0; y < 10; ++y) {
    if (fullOffseason(w).type == "mega") ++megas;
    validSquads(w);
  }
  CHECK(megas == 3);
  const auto teams = worldTeams(w);
  double bat = 0, bowl = 0;
  int notRoster = 0;
  for (const auto& t : teams) {
    bat += teamRatings(t).batting;
    bowl += teamRatings(t).bowling;
    for (const auto& p : t.squad)
      if (p.origin != Origin::roster) ++notRoster;
  }
  CHECK(bat / 10 > 58);
  CHECK(bowl / 10 > 62);
  CHECK(notRoster > 80);
  const auto lg = effectiveLeague(w, kNoProgress);
  Fixture fx;
  fx.no = 1, fx.seed = 5, fx.home = lg[0].id, fx.away = lg[1].id;
  CHECK(simFixture(fx, lg, nullptr).status == MatchStatus::complete);
}

// ---------------------------------------------------------------------------
// fitness.test.js
// ---------------------------------------------------------------------------

TEST_CASE("fitness: drains fast bowlers most and recovers between games", "[fitness]") {
  World w = createWorld(LEAGUE());
  const auto lg = effectiveLeague(w, kNoProgress);
  Fixture fx;
  fx.no = 1, fx.seed = 21, fx.home = "MUI", fx.away = "CHK";
  const Match m = simFixture(fx, lg, nullptr);
  const Workload load = matchWorkload(m);
  const PlayerId quick = byName(w, "Jasprit Bumra").id;
  const PlayerId spinner = byName(w, "Rahul Chahur").id;
  if (load.loads.find(quick) && load.loads.find(spinner)) CHECK(*load.loads.find(quick) > *load.loads.find(spinner));
  applyMatchFitness(w, load);
  if (load.loads.find(quick)) CHECK(*w.players.at(quick).fitness < 100);
}

TEST_CASE("fitness: tired, older fast bowlers are more injury-prone and slower", "[fitness]") {
  const World w = createWorld(LEAGUE());
  Player quick = byName(w, "Mitchel Stark");
  quick.fitness = 100;
  const double fresh = injuryChance(quick, 20);
  quick.fitness = 50;
  CHECK(injuryChance(quick, 20) > fresh * 2);
  quick.fitness = 40;
  CHECK(applyFatigue(quick).skills.bowl[BowlK::pace] < quick.skills.bowl[BowlK::pace]);
  quick.fitness = 90;
  const Player same = applyFatigue(quick);
  CHECK(same.skills == quick.skills);
  CHECK_FALSE(same.tired);
}

TEST_CASE("fitness: injured players stay out of the XI until they recover", "[fitness]") {
  World w = createWorld(LEAGUE());
  const PlayerId id = w.teams.at("RGC").squad[0];
  w.players.at(id).injury = Injury{"hamstring", "Hamstring", 2, 0, std::nullopt};
  auto rgc = [&]() { return *teamById(effectiveLeague(w, kNoProgress), "RGC"); };
  auto picked = [&]() {
    const auto ids = pickBestXI(rgc()).ids;
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  };
  CHECK_FALSE(picked());
  applyMatchFitness(w, Workload{{"RGC"}, {}, 1});
  applyMatchFitness(w, Workload{{"RGC"}, {}, 2});
  CHECK_FALSE(isInjured(w.players.at(id)));
  CHECK(picked());
}

TEST_CASE("fitness: a realistic number of injuries over a season of matches", "[fitness]") {
  World w = createWorld(LEAGUE());
  size_t injuries = 0;
  for (int i = 0; i < 70; ++i) {
    const auto lg = effectiveLeague(w, kNoProgress);
    Fixture fx;
    fx.no = i, fx.seed = i * 31 + 5, fx.home = lg[static_cast<size_t>(i % 10)].id;
    fx.away = lg[static_cast<size_t>((i + 3) % 10)].id;
    injuries += applyMatchFitness(w, matchWorkload(simFixture(fx, lg, nullptr))).size();
  }
  CHECK(injuries > 5);
  CHECK(injuries < 60);
}

// ---------------------------------------------------------------------------
// ageing.test.js
// ---------------------------------------------------------------------------

TEST_CASE("ageing: young players improve, prime players hold, veterans decline", "[ageing]") {
  auto avgAt = [](int age) {
    double s = 0;
    int n = 0;
    for (const auto& t : LEAGUE())
      for (Player p : t.squad) {
        p.age = age - 1;
        const AgeResult r = ageOneYear(p, 1);
        s += p.role == Role::BOWL ? r.after.bowl - r.before.bowl : r.after.bat - r.before.bat;
        ++n;
      }
    return s / n;
  };
  const double young = avgAt(20), prime = avgAt(27), vet = avgAt(34), old = avgAt(38);
  CHECK(young > 0.8);
  CHECK(std::abs(prime) < 0.8);
  CHECK(vet < -0.5);
  CHECK(old < vet);
  CHECK(agePhase(22).key == "rising");
  CHECK(agePhase(37).key == "twilight");
}

TEST_CASE("ageing: fast bowlers lose more with age than spinners", "[ageing]") {
  auto drop = [](bool pace) {
    double s = 0;
    int n = 0;
    for (const auto& t : LEAGUE())
      for (Player p : t.squad) {
        if (p.role != Role::BOWL || !p.bowlType) continue;
        const BowlType b = *p.bowlType;
        const bool isPace = b == BowlType::fast || b == BowlType::lfast || b == BowlType::medium;
        const bool isSpin = b == BowlType::off || b == BowlType::leg || b == BowlType::sla;
        if (pace ? !isPace : !isSpin) continue;
        p.age = 35;
        const AgeResult r = ageOneYear(p, 1);
        s += r.after.bowl - r.before.bowl;
        ++n;
      }
    return s / n;
  };
  CHECK(drop(true) < drop(false));
}

// ---------------------------------------------------------------------------
// finance.test.js (engine parts)
// ---------------------------------------------------------------------------

TEST_CASE("money: IPL bid increments and rupee formatting", "[finance]") {
  CHECK(nextBid(30) == 35);
  CHECK(nextBid(100) == 110);
  CHECK(nextBid(200) == 220);
  CHECK(nextBid(500) == 525);
  CHECK(money(1825.0) == "₹18.25 cr");
  CHECK(money(75.0) == "₹75 L");
  CHECK(money(2000.0) == "₹20 cr");
  CHECK(money(250.0) == "₹2.5 cr");
  CHECK(money(std::nullopt) == "–");
}

TEST_CASE("money: stars cost more, base prices sit in the brackets, squads fit the purse", "[finance]") {
  const World w = createWorld(LEAGUE());
  const Player& star = byName(w, "Jasprit Bumra");
  const Player* tail = nullptr;
  for (const auto& [id, p] : w.players)
    if (p.role == Role::BOWL && (!tail || p.bowl < tail->bowl)) tail = &p;
  CHECK(marketValue(star) > 1500);
  CHECK(marketValue(*tail) < marketValue(star) / 5);
  const double base = basePrice(star);
  CHECK(std::find(std::begin(BASE_PRICES), std::end(BASE_PRICES), base) != std::end(BASE_PRICES));
  for (const auto& t : worldTeams(w)) CHECK(payroll(w, t.id) <= PURSE);
}

TEST_CASE("money: retention slabs in order and the limits", "[finance]") {
  const World w = createWorld(LEAGUE());
  const auto& sq = w.teams.at("MUI").squad;
  const std::vector<PlayerId> six(sq.begin(), sq.begin() + 6);
  const std::vector<PlayerId> five(sq.begin(), sq.begin() + 5);
  CHECK(retentionCosts(w, five) == std::vector<double>{1800, 1400, 1100, 1800, 1400});
  CHECK_FALSE(retentionValid(w, six));  // six capped players
  CHECK(isMegaYear(3));
  CHECK_FALSE(isMegaYear(2));
}

TEST_CASE("auction: pauses for the user, then sells to the highest valuation", "[auction]") {
  World w = createWorld(LEAGUE());
  runOffseason(w, order(), std::string("LKG"), std::nullopt);
  closeContracts(w);
  finishDraft(w);
  CHECK(runAuction(w) == "bid");
  const PlayerId lot = w.auction->lot->id;
  CHECK(userBid(w));
  CHECK(w.auction->lot->leader == std::optional<TeamId>("LKG"));
  runAuction(w);
  if (w.auction->lot && w.auction->lot->id == lot) {
    userPass(w);
    runAuction(w);
  }
  CHECK((!w.auction->lot || w.auction->lot->id != lot));
  userAutoRest(w);
  CHECK(runAuction(w) == "done");
  const AuctionResult res = closeAuction(w);
  for (const auto& t : worldTeams(w)) CHECK(t.squad.size() >= 18);
  for (const auto& s : res.sold) CHECK(w.players.at(s.id).team == s.team);
}

TEST_CASE("auction: mega auctions with retentions and RTM cards keep every purse whole", "[auction]") {
  World w = createWorld(LEAGUE());
  for (int y = 1; y <= 6; ++y) {
    runOffseason(w, order(), std::nullopt, std::nullopt);
    if (w.offseason->mega) {
      closeRetention(w);
      for (const auto& [id, t] : w.teams) CHECK(t.rtm.value_or(0) >= 0);
    } else {
      closeContracts(w);
    }
    finishDraft(w);
    runAuction(w);
    closeAuction(w);
    for (const auto& t : worldTeams(w)) CHECK(purseLeft(w, t.id) >= 0);
  }
}

TEST_CASE("auction: the RTM sequence: invoke, final raise, match", "[auction]") {
  World w = createWorld(LEAGUE());
  runOffseason(w, order(), std::string("CHK"), std::nullopt);
  w.offseason->mega = true;
  closeContracts(w);
  finishDraft(w);
  w.auction->type = "mega";
  w.teams.at("CHK").rtm = 2;
  runAuction(w);
  Lot& lot = *w.auction->lot;
  w.players.at(lot.id).formerTeam = "CHK";
  lot.leader = "MUI";
  lot.price = 500;
  lot.bids.push_back({"MUI", 500});
  lot.out.set("CHK", true);
  lot.stage = "rtm-invoke";
  lot.rtmTeam = "CHK";
  CHECK(awaitingUser(w) == std::optional<std::string>("rtm-invoke"));
  rtmInvoke(w, true);
  CHECK(w.auction->lot->stage == "rtm-raise");
  rtmRaise(w, 600);
  CHECK(*w.auction->lot->price == 600);
  CHECK(awaitingUser(w) == std::optional<std::string>("rtm-match"));
  const PlayerId id = w.auction->lot->id;
  rtmMatch(w, true);
  CHECK(w.players.at(id).team == "CHK");
  CHECK(w.players.at(id).contract->via == ContractVia::rtm);
  CHECK(w.teams.at("CHK").rtm == 1);
}

TEST_CASE("board: targets by squad strength and the season verdict", "[board]") {
  CHECK(boardTarget(1).key == "final");
  CHECK(boardTarget(3).key == "playoffs");
  CHECK(boardTarget(9).key == "top8");
  auto target = [](int rank) {
    const TargetDef& t = boardTarget(rank);
    return Target{std::string(t.key), std::string(t.label), t.need, rank, 1};
  };
  CHECK(seasonVerdict(target(3), 1).delta > 20);
  CHECK(seasonVerdict(target(3), 4).delta > 0);
  CHECK(seasonVerdict(target(1), 9).delta < -20);
  Season s;
  s.champion = "A";
  s.runnerUp = "B";
  std::vector<TableRow> table(3);
  table[0].team = "C", table[1].team = "A", table[2].team = "B";
  CHECK(finalPosition(s, table, "C") == 1);
  CHECK(finalPosition(s, table, "A") == 1);
  CHECK(finalPosition(s, table, "B") == 2);
  Manager m;
  applyConfidence(m, -100);
  CHECK(m.confidence == 0);
}

// ---------------------------------------------------------------------------
// club.test.js
// ---------------------------------------------------------------------------

namespace {
const TeamId T = "MUI";
}

TEST_CASE("club: every club starts with the same account; AI sides plan nothing", "[club]") {
  const World w = createWorld(LEAGUE());
  CHECK(w.clubs.empty());
  CHECK(clubOf(w, T).bank == START_BANK);
}

TEST_CASE("club: an upgrade is paid now and opens when the next season starts", "[club]") {
  World w = createWorld(LEAGUE());
  activateClub(w, T);
  const double bank = clubOf(w, T).bank;
  CHECK(orderUpgrade(w, T, "stands"));
  CHECK(clubOf(w, T).bank == bank - *upgradeCost("stands", 0));
  CHECK(clubOf(w, T).levels["stands"] == 0);
  CHECK(capacity(clubOf(w, T), T) == groundCapacity(T));
  CHECK_FALSE(orderUpgrade(w, T, "stands"));
  setTicketTier(w, T, "premium");
  CHECK(clubOf(w, T).active.tier == "standard");
  closeClubSeason(w, T, 6);
  runOffseason(w, order(), std::nullopt, std::nullopt);
  CHECK(clubOf(w, T).levels["stands"] == 0);
  activateClub(w, T);
  CHECK(clubOf(w, T).levels["stands"] == 1);
  CHECK(capacity(clubOf(w, T), T) == groundCapacity(T) + SEATS_PER_STAND);
  CHECK(clubOf(w, T).active.tier == "premium");
}

TEST_CASE("club: decisions made between seasons apply to the season about to start", "[club]") {
  World w = createWorld(LEAGUE());
  CHECK(planYear(w, ensureClub(w, T)) == 0);
  setTicketTier(w, T, "budget");
  activateClub(w, T);
  CHECK(clubOf(w, T).active.tier == "budget");
  CHECK(planYear(w, clubOf(w, T)) == 1);
}

TEST_CASE("club: season start brings central money and sponsors and pays running costs", "[club]") {
  World w = createWorld(LEAGUE());
  const auto offers = sponsorOffers(w, T);
  CHECK(offers.size() == 6);
  CHECK(offers.find("shirt")->size() == 3);
  const auto again = sponsorOffers(w, T);
  CHECK(again.find("shirt")->at(1).brand == offers.find("shirt")->at(1).brand);  // stable
  CHECK(signSponsor(w, T, "shirt", 0));
  const double fee = offers.find("shirt")->at(0).fee;
  CHECK_FALSE(sponsorOffers(w, T).contains("shirt"));
  activateClub(w, T);
  CHECK(clubOf(w, T).bank == START_BANK + CENTRAL_SHARE + fee - RUNNING_COSTS);
  activateClub(w, T);
  CHECK(clubOf(w, T).bank == START_BANK + CENTRAL_SHARE + fee - RUNNING_COSTS);
}

TEST_CASE("club: prize money, the players' share, and goodwill on renewals", "[club]") {
  World w = createWorld(LEAGUE());
  setPrizeShare(w, T, 50);
  activateClub(w, T);
  const double bank = clubOf(w, T).bank;
  const auto res = closeClubSeason(w, T, 1);
  REQUIRE(res.has_value());
  CHECK(res->prize == prizeFor(1));
  CHECK(res->shared == prizeFor(1) / 2);
  CHECK(clubOf(w, T).bank == bank + prizeFor(1) / 2);
  CHECK_FALSE(closeClubSeason(w, T, 1).has_value());
  runOffseason(w, order(), std::nullopt, std::nullopt);
  const Player& p = w.players.at(w.teams.at(T).squad[0]);
  CHECK(renewalPrice(w, T, p, 2) < renewalDemand(p, 2));
  const Player& q = w.players.at(w.teams.at("CHK").squad[0]);
  CHECK(renewalPrice(w, "CHK", q, 2) == renewalDemand(q, 2));
}

TEST_CASE("club: home gates are capped by the ground, cheaper seats draw more", "[club]") {
  World w = createWorld(LEAGUE());
  activateClub(w, T);
  const GateFixture fx{3, std::string("CHK"), Stage::league};
  const GateTake std = gateFor(w, T, fx, 0.5);
  CHECK(std.att <= std.cap);
  CHECK(std.revenue > 0);
  const auto g = recordGate(w, T, fx, 0.5);
  REQUIRE(g.has_value());
  CHECK(g->revenue == std.revenue);
  CHECK_FALSE(recordGate(w, T, fx, 0.5).has_value());
  Club& club = ensureClub(w, T);
  club.active.tier = "elite";
  club.popularity = 40;
  CHECK(gateFor(w, T, fx, 0.5).att < std.att);
}

TEST_CASE("club: the curator chooses the home pitch for the next season", "[club]") {
  World w = createWorld(LEAGUE());
  CHECK_FALSE(setHomePitch(w, T, Pitch::green));
  ensureClub(w, T).bank = 50000;
  for (int i = 0; i < 3; ++i) {
    orderUpgrade(w, T, "curator");
    activateClub(w, T);
    closeClubSeason(w, T, 6);
    runOffseason(w, order(), std::nullopt, std::nullopt);
  }
  CHECK(clubOf(w, T).levels["curator"] == 3);
  CHECK(setHomePitch(w, T, Pitch::green));
  CHECK_FALSE(clubOf(w, T).active.pitch.has_value());
  activateClub(w, T);
  CHECK(clubOf(w, T).active.pitch == Pitch::green);
  Season season = createSeason(7, 1, T, LEAGUE());
  const Club club = clubOf(w, T);
  applyHomePitch(season, T, &club);
  bool allHome = true, someAway = false;
  for (const auto& f : season.fixtures) {
    if (f.home == T && f.pitch != Pitch::green) allHome = false;
    if (f.home != T && f.pitch != Pitch::green) someAway = true;
  }
  CHECK(allHome);
  CHECK(someAway);
}

TEST_CASE("club: a personal coach starts next season and shows in the off-season after", "[club]") {
  World w = createWorld(LEAGUE());
  World twin = createWorld(LEAGUE());
  const Player* young = nullptr;
  for (const auto& id : w.teams.at(T).squad) {
    const Player& p = w.players.at(id);
    if (!young || p.age < young->age) young = &p;
  }
  const PlayerId pid = young->id;
  activateClub(w, T);
  CHECK(assignCoach(w, T, pid, "batting", 3));
  CHECK(committed(w, clubOf(w, T)) == coachTier(3)->cost);
  CHECK(coachesIn(clubOf(w, T), w.year).empty());
  closeClubSeason(w, T, 6);
  runOffseason(w, order(), std::nullopt, std::nullopt);
  runOffseason(twin, order(), std::nullopt, std::nullopt);
  CHECK(w.players.at(pid).skills == twin.players.at(pid).skills);
  activateClub(w, T);
  CHECK(clubOf(w, T).coaches[0].paid == w.year);
  closeClubSeason(w, T, 6);
  runOffseason(w, order(), std::nullopt, std::nullopt);
  runOffseason(twin, order(), std::nullopt, std::nullopt);
  const Player& coached = w.players.at(pid);
  const Player& plain = twin.players.at(pid);
  CHECK(coached.skills.bat[BatK::lofted] > plain.skills.bat[BatK::lofted]);
  CHECK(mainRating(coached) >= mainRating(plain));
  if (coached.age <= 24) CHECK(coached.coachLift == 1);
  const Club club = clubOf(w, T);
  const auto& rows = club.lastCoaching->rows;
  CHECK(std::any_of(rows.begin(), rows.end(), [&](const CoachingRow& r) { return r.id == pid; }));
}

TEST_CASE("club: coach slots and money are limited; a coach can be ended after the season", "[club]") {
  World w = createWorld(LEAGUE());
  const auto squad = w.teams.at(T).squad;
  CHECK(assignCoach(w, T, squad[0], "batting", 1));
  CHECK_FALSE(assignCoach(w, T, squad[1], "batting", 1));
  CHECK_FALSE(assignCoach(w, T, w.teams.at("CHK").squad[0], "batting", 1));
  ensureClub(w, T).bank = 50;
  CHECK_FALSE(setStaff(w, T, "bat", 3));
  CHECK(available(w, clubOf(w, T)) == 10);
  activateClub(w, T);
  CHECK(removeCoach(w, T, squad[0]));
  CHECK(clubOf(w, T).coaches[0].ends == w.year + 1);
}

TEST_CASE("club: AI clubs are untouched by the user's club", "[club]") {
  World a = createWorld(LEAGUE());
  World b = createWorld(LEAGUE());
  activateClub(b, T);
  closeClubSeason(b, T, 5);
  runOffseason(a, order(), std::nullopt, std::nullopt);
  runOffseason(b, order(), std::nullopt, std::nullopt);
  for (const auto& t : order())
    for (size_t i = 0; i < a.teams.at(t).squad.size(); ++i)
      CHECK(a.players.at(a.teams.at(t).squad[i]).skills == b.players.at(b.teams.at(t).squad[i]).skills);
}

TEST_CASE("club: a better academy produces better graduates", "[club]") {
  World a = createWorld(LEAGUE());
  World b = createWorld(LEAGUE());
  ensureClub(b, T).levels.v.set("academy", 5);
  const OffseasonReport ra = runOffseason(a, order(), std::nullopt, std::nullopt);
  const OffseasonReport rb = runOffseason(b, order(), std::nullopt, std::nullopt);
  auto grad = [&](const World& w, const OffseasonReport& r) {
    for (const auto& x : r.academy)
      if (x.team == T) return *w.players.at(x.id).potential;
    return 0.0;
  };
  CHECK(grad(b, rb) > grad(a, ra));
}

TEST_CASE("club: ending a season in the red is flagged", "[club]") {
  World w = createWorld(LEAGUE());
  activateClub(w, T);
  ensureClub(w, T).bank = -100;
  CHECK(closeClubSeason(w, T, 9)->inRed);
}
