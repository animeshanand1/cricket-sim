#include "parity/flow.hpp"

#include <memory>

#include "app/game.hpp"

#include <algorithm>

#include "engine/auction.hpp"
#include "engine/board.hpp"
#include "engine/career.hpp"
#include "engine/club.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/league_view.hpp"
#include "engine/season.hpp"
#include "engine/sides.hpp"
#include "engine/state_json.hpp"
#include "engine/trades.hpp"
#include "engine/world.hpp"

namespace t20::parity {

namespace {

struct State {
  World world;
  Career career;
  SeasonState season;
  std::string screen = "home";
  std::vector<int32_t> seeds;
  size_t nextSeed = 0;
};

std::vector<Team> leagueOf(const State& s) { return effectiveLeague(s.world, s.career.progress); }

void startSeason(State& s, const TeamId& team) {
  activateClub(s.world, team);
  const int number = static_cast<int>(s.season.history.size()) + 1;
  const int32_t seed = s.seeds.at(s.nextSeed++);
  Season season = createSeason(seed, number, team, leagueOf(s));
  applyHomePitch(season, team, s.world.clubs.find(team));
  s.season.season = std::move(season);
  const auto league = leagueOf(s);
  const int rank = strengthRank(league, team);
  const TargetDef& target = boardTarget(rank);
  Manager& m = s.world.manager;
  if (m.team != team) {
    if (m.team) {
      BoardEntry e;
      e.season = number;
      e.event = "moved";
      e.from = m.team;
      e.to = team;
      m.history.push_back(e);
    }
    m.team = team;
    m.confidence = 60;
  }
  m.target = Target{std::string(target.key), std::string(target.label), target.need, rank, number};
  m.sacked.reset();
  s.screen = "season";
}

void simAll(State& s) {
  for (;;) {
    Season& season = *s.season.season;
    const Fixture* next = nextFixture(season);
    if (!next) break;
    const Fixture fx = *next;
    const bool mine = fx.home == season.userTeam || fx.away == season.userTeam;
    Match m = simFixture(fx, leagueOf(s), &s.career.players, &s.career.matchups);
    m.fixture = FixtureRef{fx.no, season.id, season.number, fx.stage, fx.venue, fx.home};
    if (mine) m.managed = season.userTeam;
    const BaseOf baseOf = [&](const PlayerId& id) -> const Skills* {
      if (const Player* p = m.players.find(id); p && p->devBase) return &*p->devBase;
      if (const Player* r = playerById(id)) return &r->skills;
      return nullptr;
    };
    recordMatch(s.career, m, &baseOf);
    applyMatchFitness(s.world, matchWorkload(m));
    topUpFreeAgents(s.world);
    if (s.world.manager.team) aiReplacements(s.world, s.world.manager.team);
    MatchSummary summary = summarizeMatch(m);
    const std::optional<TeamId> winner = summary.winner;
    recordResult(season, fx.no, std::move(summary));
    // takeGate: ticket sales for a home game of the user's club
    if (const auto& team = s.world.manager.team; team && fx.home == *team)
      recordGate(s.world, *team, GateFixture{fx.no, fx.away, fx.stage}, seasonForm(season, *team));
    if (mine) applyConfidence(s.world.manager, matchNudge(winner == season.userTeam, !winner));
  }
}

TeamId teamOf(const World& w, const PlayerId& id) {
  if (const auto t = teamOfId(w, id)) return *t;
  return id.substr(0, id.find('-'));
}

void nextSeason(State& s) {
  if (!s.season.season || !s.season.season->champion) return;
  const Season season = *s.season.season;
  const World& w = s.world;
  const auto aw = seasonAwards(
      season, [&](const PlayerId& id) { return teamOf(w, id); },
      [&](const PlayerId& id) -> std::optional<int> {
        const Player* p = w.players.find(id);
        return p ? std::optional<int>(p->age) : std::nullopt;
      });
  const auto table = pointsTable(season);
  SeasonArchive a;
  a.number = season.number;
  a.champion = *season.champion;
  a.runnerUp = *season.runnerUp;
  a.userTeam = season.userTeam;
  for (const auto& r : table)
    if (r.team == season.userTeam) a.userPos = r.pos;
  if (aw.orangeCap) a.orangeCap = CapLine{aw.orangeCap->id, static_cast<double>(aw.orangeCap->runs)};
  if (aw.purpleCap) a.purpleCap = CapLine{aw.purpleCap->id, static_cast<double>(aw.purpleCap->wkts)};
  if (aw.mvp) a.mvp = CapLine{aw.mvp->id, aw.mvp->pts};
  s.season.history.push_back(a);
  s.season.season.reset();

  const auto league = leagueOf(s);
  std::vector<TeamId> ranking;
  for (const auto& t : league) ranking.push_back(t.id);
  std::stable_sort(ranking.begin(), ranking.end(), [&](const TeamId& x, const TeamId& y) {
    return finalPosition(season, table, x) - finalPosition(season, table, y) < 0;
  });
  const std::vector<TeamId> order(ranking.rbegin(), ranking.rend());

  // The board's verdict (worldSlice boardSeasonReview)
  const int position = finalPosition(season, table, season.userTeam);
  const auto offers = jobOffers(league, season.userTeam);
  Manager& m = s.world.manager;
  std::optional<ClubSeasonSummary> books;
  if (m.team) books = closeClubSeason(s.world, *m.team, position);
  if (m.target) {
    const Verdict v = seasonVerdict(*m.target, position);
    applyConfidence(m, v.delta);
    const bool inRed = books && books->inRed;
    if (inRed) applyConfidence(m, -RED_INK_PENALTY);
    BoardEntry e;
    e.season = season.number;
    e.team = m.team;
    e.target = m.target->label;
    e.position = position;
    e.verdict = std::string(v.verdict);
    e.delta = v.delta - (inRed ? RED_INK_PENALTY : 0);
    e.confidence = m.confidence;
    e.inRed = inRed;
    m.history.push_back(e);
    m.target.reset();
    if (m.confidence <= SACK_AT) m.sacked = Sacked{season.number, *m.team, offers, order};
  }
  if (m.sacked) {
    s.screen = "sacked";
    return;
  }
  runOffseason(s.world, order, season.userTeam, std::string("season"));
  s.screen = "offseason";
}

void finishOffseason(State& s) {
  const std::string then = s.world.offseason && s.world.offseason->then ? *s.world.offseason->then : "players";
  const std::optional<TeamId> team = s.world.offseason ? s.world.offseason->userTeam : std::nullopt;
  if (s.world.auction) {
    if (s.world.offseason && s.world.offseason->stage == "auction") {
      runAuction(s.world);
      if (!s.world.auction->lot && s.world.auction->idx >= static_cast<int>(s.world.auction->lots.size()))
        closeAuction(s.world);
    }
  }
  if (s.world.auction) return;
  if (then == "season" && team)
    startSeason(s, *team);
  else
    s.screen = "players";
}

Json checkpoint(const State& s, bool world, bool career, bool season) {
  Json o = Json::object();
  if (world) o.add("world", toJson(s.world));
  if (career) o.add("career", toJson(s.career));
  if (season) o.add("season", toJson(s.season));
  return o;
}

}  // namespace

Json runFlow(const std::string& team, const std::vector<int32_t>& seeds) {
  State s;
  s.world = createWorld(LEAGUE());
  s.seeds = seeds;
  Json out = Json::object();
  out.add("team", team);
  Json cps = Json::object();

  startSeason(s, team);
  cps.add("start", checkpoint(s, false, false, true));
  for (int i = 0; i < 10 && !s.season.season->champion; ++i) simAll(s);
  cps.add("seasonDone", checkpoint(s, true, true, true));
  nextSeason(s);
  cps.add("offseason", checkpoint(s, true, false, true));
  if (s.screen == "offseason") {
    if (s.world.offseason && s.world.offseason->stage == "contracts") closeContracts(s.world);
    if (s.world.draft) {
      finishDraft(s.world);
      if (s.world.auction)
        runAuction(s.world);
      else
        finishOffseason(s);
    }
    if (s.world.auction) {
      userAutoRest(s.world);
      runAuction(s.world);
    }
    finishOffseason(s);
    cps.add("season2", checkpoint(s, true, true, true));
  }
  out.add("checkpoints", cps);
  return out;
}

Json runGameFlow(const std::string& team, uint32_t rseed) {
  auto r = std::make_shared<uint32_t>(rseed ? rseed : 7);
  app::Seeds seeds{[r] {
                     *r = static_cast<uint32_t>(js_add32(js_imul(static_cast<int32_t>(*r), 1103515245), 12345));
                     return *r / 4294967296.0;
                   },
                   [] { return 1700000000000.0; }};
  app::Game g(app::Game::freshState(), seeds);
  g.boot();
  auto setupView = [&g] {
    const auto& s = g.state().setup;
    Json o = Json::object();
    o.add("screen", s.screen);
    o.add("userTeamId", s.userTeamId);
    o.add("oppTeamId", s.oppTeamId);
    Json xi = Json::array();
    for (const auto& id : s.xi) xi.push(id);
    o.add("xi", xi);
    o.add("keeperId", s.keeperId ? Json(*s.keeperId) : Json());
    o.add("captainId", s.captainId ? Json(*s.captainId) : Json());
    return o;
  };
  auto cp = [&](bool world, bool career, bool season) {
    Json o = Json::object();
    if (world) o.add("world", toJson(g.state().world));
    if (career) o.add("career", toJson(g.state().career));
    if (season) o.add("season", toJson(g.state().season));
    o.add("setup", setupView());
    return o;
  };
  Json cps = Json::object();
  g.startSeason(team);
  cps.add("start", cp(false, false, true));
  for (int i = 0; i < 10 && !g.state().season.season->champion; ++i) g.simFixtures({true, std::nullopt, {}});
  cps.add("seasonDone", cp(true, true, true));
  g.nextSeason();
  cps.add("offseason", cp(true, false, true));
  if (g.state().setup.screen == "offseason") {
    g.contractsClosed();
    g.completeDraft();
    g.auctionAutoRest();
    g.finishOffseason();
    cps.add("season2", cp(true, true, true));
  }
  Json out = Json::object();
  out.add("team", team);
  out.add("checkpoints", cps);
  return out;
}

Json runOffseasons(int years) {
  World w = createWorld(LEAGUE());
  std::vector<TeamId> order;
  for (const auto& t : LEAGUE()) order.push_back(t.id);
  Json results = Json::array();
  for (int y = 0; y < years; ++y) {
    runOffseason(w, order, std::nullopt, std::nullopt);
    if (w.offseason->stage == "retention")
      closeRetention(w);
    else
      closeContracts(w);
    finishDraft(w);
    runAuction(w);
    closeAuction(w);
    // closeAuction keeps its result in the report: the same JSON as the JS return value
    results.push(toJson(w)["lastOffseason"]["auction"]);
  }
  Json out = Json::object();
  out.add("years", years);
  out.add("results", results);
  out.add("world", toJson(w));
  return out;
}

}  // namespace t20::parity
