#include "app/game.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

#include "core/str.hpp"
#include "engine/auction.hpp"
#include "engine/board.hpp"
#include "engine/career.hpp"
#include "engine/club.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/league_view.hpp"
#include "engine/pitch.hpp"
#include "engine/season.hpp"
#include "engine/sides.hpp"
#include "engine/world.hpp"

namespace t20::app {

namespace {

bool contains(const std::vector<PlayerId>& v, const PlayerId& id) { return std::find(v.begin(), v.end(), id) != v.end(); }
bool contains(const std::vector<PlayerId>& v, const std::optional<PlayerId>& id) { return id && contains(v, *id); }

std::optional<Pitch> pitchFromKey(const std::string& key) {
  for (size_t i = 0; i < std::size(kPitchNames); ++i)
    if (kPitchNames[i] == key) return static_cast<Pitch>(i);
  return std::nullopt;
}
Pitch pitchOrBalanced(const std::string& key) { return key == "random" ? Pitch::balanced : pitchFromKey(key).value_or(Pitch::balanced); }

const Team* findTeam(const std::vector<Team>& league, const TeamId& id) { return teamById(league, id); }

// carrerSlice baseOfMatch: the player's own skills (his edit or the world's), or the roster's
BaseOf baseOfMatch(const Match& m) {
  return [&m](const PlayerId& id) -> const Skills* {
    if (const Player* p = m.players.find(id); p && p->devBase) return &*p->devBase;
    if (const Player* r = playerById(id)) return &r->skills;
    return nullptr;
  };
}

TeamId teamOfIn(const World& w, const PlayerId& id) {
  if (const auto t = teamOfId(w, id)) return *t;
  return id.substr(0, id.find('-'));
}

}  // namespace

Seeds Seeds::system() {
  auto gen = std::make_shared<std::mt19937_64>(std::random_device{}());
  return Seeds{[gen] { return std::uniform_real_distribution<double>(0.0, 1.0)(*gen); },
               [] {
                 return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::system_clock::now().time_since_epoch())
                                                .count());
               }};
}

Game::Game(GameState state, Seeds seeds) : s_(std::move(state)), seeds_(std::move(seeds)) {}

GameState Game::freshState() {
  GameState s;
  s.world = createWorld(LEAGUE());
  s.setup = setupFromSaved(nullptr);
  return s;
}

void Game::boot() {
  const WorldTeam* t = s_.world.teams.find(s_.setup.userTeamId);
  const std::vector<PlayerId> squad = t ? t->squad : std::vector<PlayerId>{};
  const bool missing = std::any_of(s_.setup.xi.begin(), s_.setup.xi.end(), [&](const PlayerId& id) { return !contains(squad, id); });
  if (s_.setup.xi.empty() || missing) autoPick();
}

std::vector<Team> Game::league() const { return effectiveLeague(s_.world, s_.career.progress); }

Game::Dirty Game::takeDirty() {
  Dirty d = dirty_;
  dirty_ = Dirty{};
  return d;
}
void Game::markAllDirty() { dirty_ = Dirty{true, true, true, true, true}; }

int32_t Game::newSeed() {
  const int32_t now = js_toint32(seeds_.now());
  const int32_t r = js_toint32(std::floor(seeds_.random() * 2147483648.0));
  return now ^ r;
}

// ---------------------------------------------------------------------------
// setupSlice
// ---------------------------------------------------------------------------

void Game::recordLineup() {
  auto& st = s_.setup;
  if (!st.userTeamId.empty() && !st.xi.empty()) st.lineups.set(st.userTeamId, Lineup{st.xi, st.keeperId, st.captainId});
}

void Game::goTo(const std::string& screen) {
  s_.setup.screen = screen;
  dirty_.setup = true;
}
void Game::openPlayers() {
  if (s_.setup.screen != "players") s_.setup.returnTo = s_.setup.screen;
  s_.setup.screen = "players";
  dirty_.setup = true;
}
void Game::closePlayers() {
  s_.setup.screen = s_.setup.returnTo.empty() ? "home" : s_.setup.returnTo;
  dirty_.setup = true;
}

void Game::selectUserTeam(const TeamId& id) {
  auto& st = s_.setup;
  st.userTeamId = id;
  if (st.oppTeamId == id)
    for (const auto& t : LEAGUE())
      if (t.id != id) {
        st.oppTeamId = t.id;
        break;
      }
  st.training.clear();
  const Lineup* saved = st.lineups.find(id);
  if (saved && saved->xi.size() == 11) {
    st.xi = saved->xi;
    st.keeperId = saved->keeperId;
    st.captainId = saved->captainId;
  } else {
    const Team* team = findTeam(LEAGUE(), id);
    if (!team) team = &LEAGUE()[0];
    const BestXI xi = pickBestXI(*team, pitchOrBalanced(st.pitch));
    st.xi = xi.ids;
    st.keeperId = xi.keeperId;
    st.captainId.reset();
    recordLineup();
  }
  dirty_.setup = true;
}

void Game::selectOpponent(const TeamId& id) {
  if (id != s_.setup.userTeamId) s_.setup.oppTeamId = id;
  dirty_.setup = true;
}
void Game::setPitch(const std::string& pitch) {
  s_.setup.pitch = pitch;
  dirty_.setup = true;
}

void Game::setXI(const std::vector<PlayerId>& xi, const std::optional<PlayerId>& keeperId,
                 const std::optional<PlayerId>& captainId) {
  auto& st = s_.setup;
  st.xi = xi;
  st.keeperId = keeperId;
  st.captainId = contains(xi, captainId) ? captainId : contains(xi, st.captainId) ? st.captainId : std::nullopt;
  st.training.clear();
  recordLineup();
  dirty_.setup = true;
}

void Game::setTraining(size_t slot, const std::optional<PlayerId>& playerId, const std::string& drill) {
  auto t = s_.setup.training;
  if (!playerId) {
    if (slot < t.size()) t.erase(t.begin() + static_cast<std::ptrdiff_t>(slot));
  } else if (slot < t.size()) {
    t[slot] = TrainingSlot{*playerId, drill};
  } else {
    t.push_back(TrainingSlot{*playerId, drill});
  }
  if (t.size() > static_cast<size_t>(MAX_TRAINING_SLOTS)) t.resize(MAX_TRAINING_SLOTS);
  s_.setup.training = t;
  dirty_.setup = true;
}
void Game::clearTraining() {
  s_.setup.training.clear();
  dirty_.setup = true;
}

void Game::togglePlayer(const PlayerId& id) {
  auto& st = s_.setup;
  if (contains(st.xi, id)) {
    st.xi.erase(std::find(st.xi.begin(), st.xi.end(), id));
    std::erase_if(st.training, [&](const TrainingSlot& t) { return t.playerId == id; });
    if (st.keeperId == id) st.keeperId.reset();
    if (st.captainId == id) st.captainId.reset();
  } else if (st.xi.size() < 11) {
    st.xi.push_back(id);
  }
  recordLineup();
  dirty_.setup = true;
}

void Game::movePlayer(const PlayerId& id, int dir) {
  auto& xi = s_.setup.xi;
  auto it = std::find(xi.begin(), xi.end(), id);
  if (it == xi.end()) return;
  const int i = static_cast<int>(it - xi.begin());
  const int j = i + dir;
  if (j < 0 || j >= static_cast<int>(xi.size())) return;
  std::swap(xi[static_cast<size_t>(i)], xi[static_cast<size_t>(j)]);
  recordLineup();
  dirty_.setup = true;
}

void Game::swapPlayers(const PlayerId& id1, const PlayerId& id2) {
  auto& st = s_.setup;
  if (id1.empty() || id2.empty() || id1 == id2) return;
  auto pos = [&](const PlayerId& id) {
    auto it = std::find(st.xi.begin(), st.xi.end(), id);
    return it == st.xi.end() ? -1 : static_cast<int>(it - st.xi.begin());
  };
  const int i = pos(id1), j = pos(id2);
  auto drop = [&](const PlayerId& leaving) {
    std::erase_if(st.training, [&](const TrainingSlot& t) { return t.playerId == leaving; });
    if (st.keeperId == leaving) st.keeperId.reset();
    if (st.captainId == leaving) st.captainId.reset();
  };
  if (i >= 0 && j >= 0) {
    std::swap(st.xi[static_cast<size_t>(i)], st.xi[static_cast<size_t>(j)]);
  } else if (i >= 0) {
    st.xi[static_cast<size_t>(i)] = id2;
    drop(id1);
  } else if (j >= 0) {
    st.xi[static_cast<size_t>(j)] = id1;
    drop(id2);
  }
  recordLineup();
  dirty_.setup = true;
}

void Game::reorderXI(int fromIndex, int toIndex) {
  auto& xi = s_.setup.xi;
  const int n = static_cast<int>(xi.size());
  if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n || fromIndex == toIndex) return;
  const PlayerId moved = xi[static_cast<size_t>(fromIndex)];
  xi.erase(xi.begin() + fromIndex);
  xi.insert(xi.begin() + toIndex, moved);
  recordLineup();
  dirty_.setup = true;
}

void Game::setCaptain(const PlayerId& id) {
  if (contains(s_.setup.xi, id)) s_.setup.captainId = id;
  recordLineup();
  dirty_.setup = true;
}
void Game::captainCleared() {
  s_.setup.captainId.reset();
  recordLineup();
  dirty_.setup = true;
}
void Game::setKeeper(const PlayerId& id) {
  if (contains(s_.setup.xi, id)) s_.setup.keeperId = id;
  recordLineup();
  dirty_.setup = true;
}
void Game::updateMatchSettings(std::optional<double> aggression, std::optional<bool> autoBowl) {
  if (aggression) s_.setup.matchSettings.aggression = *aggression;
  if (autoBowl) s_.setup.matchSettings.autoBowl = *autoBowl;
  dirty_.setup = true;
}

// ---------------------------------------------------------------------------
// Friendlies and the match
// ---------------------------------------------------------------------------

void Game::chooseTeam(const TeamId& teamId) {
  selectUserTeam(teamId);
  const WorldTeam* t = s_.world.teams.find(teamId);
  const std::vector<PlayerId> squad = t ? t->squad : std::vector<PlayerId>{};
  const Lineup* saved = s_.setup.lineups.find(teamId);
  if (saved && saved->xi.size() == 11 &&
      std::all_of(saved->xi.begin(), saved->xi.end(), [&](const PlayerId& id) { return contains(squad, id); })) {
    const Lineup copy = *saved;
    setXI(copy.xi, copy.keeperId, copy.captainId);
  } else {
    autoPick();
  }
}

void Game::autoPick() {
  const auto lg = league();
  const Team* team = findTeam(lg, s_.setup.userTeamId);
  if (!team) return;
  const BestXI xi = pickBestXI(*team, pitchOrBalanced(s_.setup.pitch));
  setXI(xi.ids, xi.keeperId, team->captainId);
}

void Game::startFixture() {
  const SetupState st = s_.setup;
  const auto lg = league();
  const Pitch actualPitch =
      st.pitch == "random" ? PITCH_KEYS[static_cast<size_t>(std::floor(seeds_.random() * PITCH_KEYS.size()))]
                           : pitchOrBalanced(st.pitch);
  const Team* team = findTeam(lg, st.userTeamId);
  const Team* opp = findTeam(lg, st.oppTeamId);
  if (!team || !opp) return;
  const BestXI oppXI = pickBestXI(*opp, actualPitch);
  std::vector<const Player*> oppPlayers;
  for (const auto& id : oppXI.ids)
    for (const auto& p : opp->squad)
      if (p.id == id) oppPlayers.push_back(&p);
  const auto oppTraining = aiTrainingPlan(oppPlayers, actualPitch);
  MatchSettings settings;
  const MatchSettings* live = s_.match ? &s_.match->settings : nullptr;
  settings.aggression = live && live->aggression ? live->aggression : Tempo(st.matchSettings.aggression);
  settings.autoBowl = live ? live->autoBowl : st.matchSettings.autoBowl;
  const int32_t seed = newSeed();
  const TeamId home = st.fixture ? st.fixture->home : st.userTeamId;
  const Club* club = s_.world.clubs.find(home);
  MatchSetup setup;
  setup.seed = seed;
  setup.id = cat(seed, '-', seeds_.now());
  setup.pitch = actualPitch;
  setup.userTeam = 0;
  setup.teams = {buildSide(*team, st.xi, st.keeperId.value_or(PlayerId{}), &s_.career.players, st.training, st.captainId),
                 buildSide(*opp, oppXI.ids, oppXI.keeperId, &s_.career.players, oppTraining)};
  setup.settings = settings;
  setup.ground.home = std::optional<TeamId>(home);
  if (club) setup.ground.capacity = capacity(*club, home);
  setup.ground.big = st.fixture && st.fixture->stage != Stage::league;
  setup.matchups = &s_.career.matchups;
  Match m = createMatch(setup);
  if (st.fixture) m.fixture = st.fixture;
  s_.match = std::move(m);
  dirty_.match = true;
  clearTraining();
  goTo("toss");
}

void Game::rematch() { goTo("training"); }

void Game::newGame() {
  if (s_.match) updateMatchSettings(s_.match->settings.aggression.value_or(3), s_.match->settings.autoBowl);
  s_.match.reset();
  s_.setup.fixture.reset();
  dirty_.match = true;
  goTo("home");
}

void Game::withMatch(const std::function<void(Match&)>& fn) {
  if (!s_.match) return;
  const MatchStatus before = s_.match->status;
  fn(*s_.match);
  dirty_.match = true;
  if (before != MatchStatus::complete && s_.match->status == MatchStatus::complete) afterMatch(*s_.match);
}

void Game::callToss(const std::string& call) { withMatch([&](Match& m) { t20::callToss(m, call); }); }
void Game::chooseToss(TossDecision d) { withMatch([&](Match& m) { chooseTossDecision(m, d); }); }
void Game::nextBall() { withMatch([](Match& m) { playBall(m); }); }
void Game::nextOver() { withMatch([](Match& m) { playOver(m); }); }
void Game::simInnings() { withMatch([](Match& m) { t20::simInnings(m); }); }
void Game::simMatch() { withMatch([](Match& m) { t20::simMatch(m); }); }
void Game::startChase() { withMatch([](Match& m) { startSecondInnings(m); }); }
void Game::pickBowler(const PlayerId& id) { withMatch([&](Match& m) { setBowler(m, id); }); }
void Game::changeBowler() { withMatch([](Match& m) { t20::changeBowler(m); }); }
void Game::setBowlingPlan(std::optional<Plan> plan) { withMatch([&](Match& m) { t20::setBowlingPlan(m, plan); }); }
void Game::setField(FieldPreset preset, std::optional<std::vector<Pos>> positions) {
  withMatch([&](Match& m) { t20::setField(m, preset, positions); });
}
void Game::setBatterTarget(const PlayerId& bowlerId, std::optional<TargetMode> mode) {
  withMatch([&](Match& m) { t20::setBatterTarget(m, bowlerId, mode); });
}
void Game::pickBatter(const PlayerId& id) { withMatch([&](Match& m) { setNextBatter(m, id); }); }
void Game::setAggression(double level) {
  withMatch([&](Match& m) {
    m.settings.aggression = level;
    refreshMindset(m);
  });
  s_.setup.matchSettings.aggression = level;
  dirty_.setup = true;
}
void Game::setBatterTempo(const PlayerId& batterId, Tempo level) {
  withMatch([&](Match& m) { t20::setBatterTempo(m, batterId, level); });
}
void Game::setAutoBowl(bool on) {
  withMatch([&](Match& m) {
    m.settings.autoBowl = on;
    if (on && m.awaiting == Awaiting::bowler) resolvePending(m);
  });
  s_.setup.matchSettings.autoBowl = on;
  dirty_.setup = true;
}
void Game::impactSub(int side, const PlayerId& out, const PlayerId& in) {
  withMatch([&](Match& m) { useImpactPlayer(m, side, out, in); });
}
void Game::reviewCall(bool review) { withMatch([&](Match& m) { reviewDecision(m, review); }); }
void Game::setAutoReview(bool on) {
  withMatch([&](Match& m) {
    m.settings.autoReview = on;
    if (on && m.awaiting == Awaiting::review) resolvePending(m);
  });
}

void Game::recordFinishedMatch(const Match& m) {
  if (m.status != MatchStatus::complete) return;
  afterMatch(m);
}

// The store's listener: a finished match goes into the career, the players' fitness and (for a
// season fixture) the season, the gate and the board
void Game::afterMatch(const Match& m) {
  const BaseOf baseOf = baseOfMatch(m);
  recordMatch(s_.career, m, &baseOf);
  matchFitness(m);
  if (m.fixture && s_.season.season && s_.season.season->id == m.fixture->seasonId) {
    MatchSummary summary = summarizeMatch(m);
    const std::optional<TeamId> winner = summary.winner;
    recordResult(*s_.season.season, m.fixture->no, std::move(summary));
    takeGate(m.fixture->no);
    const TeamId me = m.teams[static_cast<size_t>(m.userTeam.value_or(0))].id;
    boardMatchResult(winner == me, !winner);
  }
  dirty_ = Dirty{true, true, true, true, true};
}

void Game::matchFitness(const Match& m) {
  applyMatchFitness(s_.world, matchWorkload(m));
  topUpFreeAgents(s_.world);
  if (s_.world.manager.team) aiReplacements(s_.world, s_.world.manager.team);
}

void Game::takeGate(int no) {
  const Season* season = s_.season.season ? &*s_.season.season : nullptr;
  const auto& team = s_.world.manager.team;
  if (!season || !team) return;
  for (const auto& fx : season->fixtures)
    if (fx.no == no) {
      if (fx.home != *team) return;
      recordGate(s_.world, *team, GateFixture{fx.no, fx.away, fx.stage}, seasonForm(*season, *team));
      return;
    }
}

void Game::boardMatchResult(bool won, bool tied) { applyConfidence(s_.world.manager, matchNudge(won, tied)); }

// ---------------------------------------------------------------------------
// Season
// ---------------------------------------------------------------------------

void Game::seasonTargetSet(const TeamId& team, int rank, int season) {
  Manager& m = s_.world.manager;
  if (m.team != team) {
    if (m.team) {
      BoardEntry e;
      e.season = season;
      e.event = "moved";
      e.from = m.team;
      e.to = team;
      m.history.push_back(e);
    }
    m.team = team;
    m.confidence = 60;
  }
  const TargetDef& t = boardTarget(rank);
  m.target = Target{std::string(t.key), std::string(t.label), t.need, rank, season};
  m.sacked.reset();
}

void Game::startSeason(const TeamId& userTeamId) {
  activateClub(s_.world, userTeamId);
  const int number = static_cast<int>(s_.season.history.size()) + 1;
  const int32_t seed = newSeed();
  Season season = createSeason(seed, number, userTeamId, league());
  applyHomePitch(season, userTeamId, s_.world.clubs.find(userTeamId));
  s_.season.season = std::move(season);
  selectUserTeam(userTeamId);
  const WorldTeam* t = s_.world.teams.find(userTeamId);
  const std::vector<PlayerId> squad = t ? t->squad : std::vector<PlayerId>{};
  const Lineup* saved = s_.setup.lineups.find(userTeamId);
  if (saved && saved->xi.size() == 11 &&
      std::all_of(saved->xi.begin(), saved->xi.end(), [&](const PlayerId& id) { return contains(squad, id); })) {
    const Lineup copy = *saved;
    setXI(copy.xi, copy.keeperId, copy.captainId);
  } else {
    autoPick();
  }
  const int rank = strengthRank(league(), userTeamId);
  seasonTargetSet(userTeamId, rank, number);
  markAllDirty();
  goTo("season");
}

void Game::simFixtures(const SimOptions& opts) {
  if (!s_.season.season) return;
  int done = 0;
  for (;;) {
    Season& season = *s_.season.season;
    const Fixture* next = nextFixture(season);
    if (!next) break;
    const Fixture fx = *next;
    const bool mine = fx.home == season.userTeam || fx.away == season.userTeam;
    if ((mine && !opts.includeUser) || (opts.stopAfter && fx.no > *opts.stopAfter)) break;
    Match m = simFixture(fx, league(), &s_.career.players, &s_.career.matchups);
    m.fixture = FixtureRef{fx.no, season.id, season.number, fx.stage, fx.venue, fx.home};
    if (mine) m.managed = season.userTeam;
    const BaseOf baseOf = baseOfMatch(m);
    recordMatch(s_.career, m, &baseOf);
    matchFitness(m);
    MatchSummary summary = summarizeMatch(m);
    const std::optional<TeamId> winner = summary.winner;
    recordResult(*s_.season.season, fx.no, std::move(summary));
    takeGate(fx.no);
    if (mine) boardMatchResult(winner == s_.season.season->userTeam, !winner);
    ++done;
    if (opts.progress && done % 2 == 0) opts.progress(done);
  }
  markAllDirty();
}

void Game::playNextFixture() {
  if (!s_.season.season) return;
  const Season& season = *s_.season.season;
  const Fixture* next = nextFixture(season);
  if (!next || (next->home != season.userTeam && next->away != season.userTeam)) return;
  const Fixture fx = *next;
  s_.match.reset();
  auto& st = s_.setup;
  st.userTeamId = season.userTeam;
  st.oppTeamId = fx.home == season.userTeam ? fx.away : fx.home;
  st.pitch = std::string(name(fx.pitch));
  st.fixture = FixtureRef{fx.no, season.id, season.number, fx.stage, fx.venue, fx.home};
  st.training.clear();
  const WorldTeam* t = s_.world.teams.find(season.userTeam);
  const std::vector<PlayerId> squad = t ? t->squad : std::vector<PlayerId>{};
  auto inSquad = [&](const std::vector<PlayerId>& xi) {
    return std::all_of(xi.begin(), xi.end(), [&](const PlayerId& id) { return contains(squad, id); });
  };
  const Lineup* saved = st.lineups.find(season.userTeam);
  if (st.xi.size() == 11 && inSquad(st.xi)) {
    // keep the current XI and order
  } else if (saved && saved->xi.size() == 11 && inSquad(saved->xi)) {
    const Lineup copy = *saved;
    setXI(copy.xi, copy.keeperId, copy.captainId);
  } else {
    autoPick();
  }
  dirty_.match = dirty_.setup = true;
  goTo("xi");
}

void Game::backToSeason() {
  if (s_.match) updateMatchSettings(s_.match->settings.aggression.value_or(3), s_.match->settings.autoBowl);
  s_.match.reset();
  s_.setup.fixture.reset();
  dirty_.match = true;
  goTo("season");
}

void Game::nextSeason() {
  if (!s_.season.season || !s_.season.season->champion) return;
  const Season season = *s_.season.season;
  const World& w = s_.world;
  const auto aw = seasonAwards(
      season, [&](const PlayerId& id) { return teamOfIn(w, id); },
      [&](const PlayerId& id) -> std::optional<int> {
        const Player* p = w.players.find(id);
        return p ? std::optional<int>(p->age) : std::nullopt;
      });
  const auto table = pointsTable(season);
  SeasonArchive a;
  a.number = season.number;
  a.champion = *season.champion;
  a.runnerUp = season.runnerUp.value_or("");
  a.userTeam = season.userTeam;
  for (const auto& r : table)
    if (r.team == season.userTeam) a.userPos = r.pos;
  if (aw.orangeCap) a.orangeCap = CapLine{aw.orangeCap->id, static_cast<double>(aw.orangeCap->runs)};
  if (aw.purpleCap) a.purpleCap = CapLine{aw.purpleCap->id, static_cast<double>(aw.purpleCap->wkts)};
  if (aw.mvp) a.mvp = CapLine{aw.mvp->id, aw.mvp->pts};
  s_.season.history.push_back(a);
  s_.season.season.reset();

  const auto lg = league();
  std::vector<TeamId> ranking;
  for (const auto& t : lg) ranking.push_back(t.id);
  std::stable_sort(ranking.begin(), ranking.end(), [&](const TeamId& x, const TeamId& y) {
    return finalPosition(season, table, x) - finalPosition(season, table, y) < 0;
  });
  const std::vector<TeamId> order(ranking.rbegin(), ranking.rend());

  // boardSeasonReview
  const int position = finalPosition(season, table, season.userTeam);
  const auto offers = jobOffers(lg, season.userTeam);
  Manager& m = s_.world.manager;
  std::optional<ClubSeasonSummary> books;
  if (m.team) books = closeClubSeason(s_.world, *m.team, position);
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
    if (m.confidence <= SACK_AT) m.sacked = Sacked{season.number, m.team.value_or(""), offers, order};
  }
  markAllDirty();
  if (m.sacked) {
    goTo("sacked");
    return;
  }
  startOffseason(order, "season", season.userTeam);
}

void Game::acceptJob(const TeamId& teamId) {
  Manager& m = s_.world.manager;
  const std::optional<std::vector<TeamId>> order = m.sacked ? m.sacked->order : std::nullopt;
  BoardEntry e;
  if (m.sacked) e.season = m.sacked->season;
  e.event = "sacked";
  e.from = m.team;
  e.to = teamId;
  m.history.push_back(e);
  m.team = teamId;
  m.confidence = 55;
  m.sacked.reset();
  selectUserTeam(teamId);
  startOffseason(order, "season", teamId);
}

// ---------------------------------------------------------------------------
// Off-season
// ---------------------------------------------------------------------------

void Game::startOffseason(std::optional<std::vector<TeamId>> order, const std::string& then,
                          std::optional<TeamId> userTeam) {
  const auto lg = league();
  const TeamId me = userTeam ? *userTeam : s_.setup.userTeamId;
  std::vector<TeamId> draftOrder;
  if (order) {
    draftOrder = *order;
  } else {
    std::vector<const Team*> sorted;
    for (const auto& t : lg) sorted.push_back(&t);
    auto score = [](const Team* t) {
      const TeamRatings r = teamRatings(*t);
      return r.batting + r.bowling;
    };
    std::stable_sort(sorted.begin(), sorted.end(), [&](const Team* a, const Team* b) { return score(a) - score(b) < 0; });
    for (const Team* t : sorted) draftOrder.push_back(t->id);
  }
  runOffseason(s_.world, draftOrder, me, then);
  markAllDirty();
  goTo("offseason");
}

void Game::contractRenewed(const PlayerId& id, int years) {
  if (s_.world.offseason && s_.world.offseason->userTeam) renewPlayer(s_.world, *s_.world.offseason->userTeam, id, years);
  dirty_.world = true;
}
void Game::contractReleased(const PlayerId& id) {
  const auto& o = s_.world.offseason;
  if (o && o->userTeam && (o->stage == "contracts" || o->stage == "retention")) releasePlayer(s_.world, *o->userTeam, id);
  dirty_.world = true;
}
void Game::contractsClosed() {
  if (s_.world.offseason && s_.world.offseason->stage == "contracts") closeContracts(s_.world);
  dirty_.world = true;
}
void Game::retentionSet(const std::vector<PlayerId>& ids) {
  if (s_.world.offseason && s_.world.offseason->stage == "retention") setRetention(s_.world, ids);
  dirty_.world = true;
}
void Game::retentionClosed() {
  if (s_.world.offseason && s_.world.offseason->stage == "retention") closeRetention(s_.world);
  dirty_.world = true;
}
void Game::scout(const PlayerId& id) {
  scoutProspect(s_.world, id);
  dirty_.world = true;
}
void Game::makeDraftPick(const PlayerId& id) {
  if (!s_.world.draft || !s_.world.draft->userTeam) return;
  const TeamId team = *s_.world.draft->userTeam;
  if (draftPick(s_.world, team, id)) advanceDraft(s_.world, s_.world.draft->userTeam);
  dirty_.world = true;
}
void Game::completeDraft() {
  if (!s_.world.draft) return;
  finishDraft(s_.world);
  dirty_.world = true;
  if (s_.world.auction)
    runAuction(s_.world);
  else
    finishOffseason();
}
void Game::auctionBid() {
  if (s_.world.auction && userBid(s_.world)) runAuction(s_.world);
  dirty_.world = true;
}
void Game::auctionPass() {
  if (s_.world.auction && userPass(s_.world)) runAuction(s_.world);
  dirty_.world = true;
}
void Game::auctionLimit(double max) {
  if (s_.world.auction && userLimit(s_.world, max)) runAuction(s_.world);
  dirty_.world = true;
}
void Game::auctionAutoRest() {
  if (s_.world.auction) {
    userAutoRest(s_.world);
    runAuction(s_.world);
  }
  dirty_.world = true;
}
void Game::auctionRtm(const std::string& step, double value) {
  if (!s_.world.auction) return;
  if (step == "invoke") rtmInvoke(s_.world, value != 0);
  if (step == "raise") rtmRaise(s_.world, value);
  if (step == "match") rtmMatch(s_.world, value != 0);
  runAuction(s_.world);
  dirty_.world = true;
}

void Game::finishOffseason() {
  const auto& o = s_.world.offseason;
  const std::string then = o && o->then ? *o->then : "players";
  const TeamId team = o && o->userTeam ? *o->userTeam : s_.setup.userTeamId;
  if (s_.world.auction && s_.world.offseason && s_.world.offseason->stage == "auction") {
    runAuction(s_.world);
    if (!s_.world.auction->lot && s_.world.auction->idx >= static_cast<int>(s_.world.auction->lots.size()))
      closeAuction(s_.world);
  }
  dirty_.world = true;
  if (s_.world.auction) return;  // auction not finished
  autoPick();
  if (then == "season")
    startSeason(team);
  else
    goTo("players");
}

void Game::offseasonReportDismissed() {
  s_.world.lastOffseason.reset();
  dirty_.world = true;
}

// ---------------------------------------------------------------------------
// Club business (decisions for the user's club)
// ---------------------------------------------------------------------------

namespace {
TeamId clubTeam(const GameState& s) { return s.world.manager.team.value_or(s.setup.userTeamId); }
}  // namespace

bool Game::clubUpgradeOrdered(const std::string& key) {
  dirty_.world = true;
  return orderUpgrade(s_.world, clubTeam(s_), key);
}
bool Game::clubTicketsSet(const std::string& tier) {
  dirty_.world = true;
  return setTicketTier(s_.world, clubTeam(s_), tier);
}
bool Game::clubPitchSet(std::optional<Pitch> pitch) {
  dirty_.world = true;
  return setHomePitch(s_.world, clubTeam(s_), pitch);
}
bool Game::clubPrizeShareSet(int share) {
  dirty_.world = true;
  return setPrizeShare(s_.world, clubTeam(s_), share);
}
bool Game::clubStaffSet(const std::string& role, int tier) {
  dirty_.world = true;
  return setStaff(s_.world, clubTeam(s_), role, tier);
}
bool Game::clubCoachAssigned(const PlayerId& playerId, const std::string& area, int tier) {
  dirty_.world = true;
  return assignCoach(s_.world, clubTeam(s_), playerId, area, tier);
}
bool Game::clubCoachRemoved(const PlayerId& playerId) {
  dirty_.world = true;
  return removeCoach(s_.world, clubTeam(s_), playerId);
}
bool Game::clubSponsorSigned(const std::string& slot, size_t index) {
  dirty_.world = true;
  return signSponsor(s_.world, clubTeam(s_), slot, index);
}

// ---------------------------------------------------------------------------
// Editor, new players, the club captain
// ---------------------------------------------------------------------------

EditResult Game::savePlayerEdit(const PlayerId& id, const EditDraft& draft) {
  Player* p = s_.world.players.find(id);
  if (!p) {
    EditResult r;
    r.errors.set("name", "Unknown player");
    return r;
  }
  EditResult res = validateEdit(*p, draft);
  if (!res.ok) return res;
  writeEdit(*p, res.edit, s_.world.year);
  recompute(*p);
  s_.career.progress.erase(id);
  dirty_.world = dirty_.career = true;
  return res;
}

void Game::restorePlayer(const PlayerId& id) {
  if (Player* p = s_.world.players.find(id); p && undoEdit(*p, s_.world.year)) recompute(*p);
  dirty_.world = true;
}

void Game::allEditsRestored() {
  for (auto& [id, p] : s_.world.players)
    if (undoEdit(p, s_.world.year)) recompute(p);
  dirty_.world = true;
}

CreateOutcome Game::createNewPlayer(const TeamId& teamId, const EditDraft& draft) {
  CreateOutcome out;
  const World& w = s_.world;
  auto fail = [&](const std::string& msg) {
    out.errors.set("form", msg);
    return out;
  };
  const WorldTeam* t = w.teams.find(teamId);
  if (!t) return fail("Choose a team.");
  if (w.offseason) return fail("Finish the off-season first.");
  if (static_cast<int>(t->squad.size()) >= SQUAD_MAX)
    return fail(cat("The squad is full (", SQUAD_MAX, " players). Release or trade someone first."));
  std::vector<std::string> taken;
  for (const auto& [id, p] : w.players)
    if (!p.retired) taken.push_back(p.name);
  const EditResult res = validateNewPlayer(draft, taken);
  if (!res.ok) {
    out.errors = res.errors;
    return out;
  }
  const CreateResult made = createPlayer(s_.world, teamId, res.edit);
  if (!made.ok) return fail(made.error.empty() ? "He could not be added to the squad." : made.error);
  dirty_.world = true;
  if (draft.captain) makeClubCaptain(made.id);
  out.ok = true;
  out.id = made.id;
  return out;
}

CaptainOutcome Game::makeClubCaptain(const PlayerId& id) {
  const Player* p = s_.world.players.find(id);
  if (!p || !p->team || p->retired) return {false, false, false, "Only a player in a squad can captain it."};
  const TeamId team = *p->team;
  setClubCaptain(s_.world, team, id);
  dirty_.world = true;
  if (team != s_.setup.userTeamId) return {true, false, false, {}};
  const auto lg = league();
  const Team* t = findTeam(lg, team);
  const Player* me = nullptr;
  if (t)
    for (const auto& q : t->squad)
      if (q.id == id) me = &q;
  if (!me || me->unavailable) return {true, contains(s_.setup.xi, id), me && me->unavailable, {}};
  if (contains(s_.setup.xi, id)) {
    setCaptain(id);
  } else {
    const BestXI next = withCaptain(*t, s_.setup.xi, s_.setup.keeperId.value_or(PlayerId{}), id);
    setXI(next.ids, next.keeperId.empty() ? std::nullopt : std::optional<PlayerId>(next.keeperId), id);
  }
  return {true, contains(s_.setup.xi, id), false, {}};
}

void Game::removeClubCaptain(const TeamId& teamId) {
  const auto was = clubCaptainOf(s_.world, teamId);
  setClubCaptain(s_.world, teamId, std::nullopt);
  dirty_.world = true;
  if (was && teamId == s_.setup.userTeamId && s_.setup.captainId == was) captainCleared();
}

void Game::chooseCaptain(const PlayerId& id) {
  setCaptain(id);
  if (s_.setup.captainId == id) {
    setClubCaptain(s_.world, s_.setup.userTeamId, id);
    dirty_.world = true;
  }
}

// ---------------------------------------------------------------------------
// Transfers
// ---------------------------------------------------------------------------

TradeVerdict Game::proposeTrade(const TeamId& other, const Offer& offer) {
  const Season* season = s_.season.season ? &*s_.season.season : nullptr;
  auto refuse = [](const std::string& text) {
    TradeVerdict v;
    v.errors.push_back(text);
    return v;
  };
  if (!season || !tradeWindow(season).open) return refuse("The trade window is shut.");
  if (s_.match && s_.match->status != MatchStatus::complete) return refuse("Finish the match in progress first.");
  const TeamId team = season->userTeam;
  TradeVerdict v = evaluateTrade(s_.world, team, other, offer);
  if (!v.accept) return v;
  makeTrade(s_.world, team, other, offer);
  dirty_.world = true;
  const auto& squad = s_.world.teams.at(team).squad;
  if (std::any_of(s_.setup.xi.begin(), s_.setup.xi.end(), [&](const PlayerId& id) { return !contains(squad, id); }))
    autoPick();
  v.done = true;
  return v;
}

bool Game::signCover(const PlayerId& injuredId, const PlayerId& id) {
  const std::optional<TeamId> team =
      s_.season.season ? std::optional<TeamId>(s_.season.season->userTeam) : s_.world.manager.team;
  if (!team) return false;
  signReplacement(s_.world, *team, injuredId, id);
  dirty_.world = true;
  const Player* p = s_.world.players.find(id);
  return p && p->team == team;
}

// ---------------------------------------------------------------------------
// Career and UI
// ---------------------------------------------------------------------------

void Game::resetCareer() {
  s_.career = Career{};
  dirty_.career = true;
}
void Game::openCard(const PlayerId& id) {
  s_.ui.cardPlayerId = id;
  s_.ui.cardEdit = false;
}
void Game::openEditor(const PlayerId& id) {
  s_.ui.cardPlayerId = id;
  s_.ui.cardEdit = true;
}
void Game::closeCard() {
  s_.ui.cardPlayerId.reset();
  s_.ui.cardEdit = false;
}

}  // namespace t20::app
