// A5: the Game controller and the save system.
//  - Game against the real JS store (tests/golden/flow.json), with the same stubbed seeds
//  - quitting halfway through a season and relaunching from the save changes nothing
//  - save files: round trips, a corrupt file falls back to its backup, browser saves load
//  - ports of lineup_and_settings.test.js, the board-through-the-store test and the
//    create-player store tests

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "app/game.hpp"
#include "app/match_session.hpp"
#include "engine/season.hpp"
#include "app/save_manager.hpp"
#include "core/json.hpp"
#include "engine/state_json.hpp"
#include "engine/world.hpp"

using namespace t20;
using namespace t20::app;
namespace fs = std::filesystem;

namespace {

// tools/parity/flow_js.mjs stubs Math.random with this generator and Date.now with a constant
Seeds stubSeeds(uint32_t seed = 7) {
  auto r = std::make_shared<uint32_t>(seed ? seed : 7);
  return Seeds{[r] {
                 *r = static_cast<uint32_t>(js_add32(js_imul(static_cast<int32_t>(*r), 1103515245), 12345));
                 return *r / 4294967296.0;
               },
               [] { return 1700000000000.0; }};
}

Json readGolden(const std::string& name) {
  std::ifstream in(std::string(T20_GOLDEN_DIR) + "/" + name, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  auto j = Json::parse(ss.str());
  REQUIRE(j.has_value());
  return *j;
}

Json setupView(const SetupState& s) {
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
}

void requireSame(const Json& want, const Json& got, const std::string& where) {
  const auto d = jsonDiff(want, got, where);
  INFO("JS vs C++: " << d.value_or(""));
  REQUIRE_FALSE(d.has_value());
}

// A fresh folder for one test, removed afterwards
struct TempDir {
  fs::path path;
  TempDir() {
    std::random_device rd;
    path = fs::temp_directory_path() / ("t20coach_test_" + std::to_string(rd()) + "_" + std::to_string(rd()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

Game freshGame(Seeds seeds = stubSeeds()) {
  Game g(Game::freshState(), std::move(seeds));
  g.boot();
  return g;
}

void simToChampion(Game& g) {
  for (int i = 0; i < 10 && !g.state().season.season->champion; ++i) g.simFixtures({true, std::nullopt, {}});
}

}  // namespace

// ---------------------------------------------------------------------------
// Parity with the JS store
// ---------------------------------------------------------------------------

TEST_CASE("game: the flow test's path matches the JS store, setup screen included", "[game][parity]") {
  const Json golden = readGolden("flow.json");
  const Json& cps = golden["checkpoints"];
  Game g = freshGame(stubSeeds(7));
  const TeamId team = golden["team"].str();

  g.startSeason(team);
  requireSame(cps["start"]["season"], toJson(g.state().season), "start.season");
  requireSame(cps["start"]["setup"], setupView(g.state().setup), "start.setup");

  simToChampion(g);
  requireSame(cps["seasonDone"]["season"], toJson(g.state().season), "seasonDone.season");
  requireSame(cps["seasonDone"]["career"], toJson(g.state().career), "seasonDone.career");
  requireSame(cps["seasonDone"]["world"], toJson(g.state().world), "seasonDone.world");

  g.nextSeason();
  requireSame(cps["offseason"]["world"], toJson(g.state().world), "offseason.world");
  requireSame(cps["offseason"]["setup"], setupView(g.state().setup), "offseason.setup");
  REQUIRE(g.state().setup.screen == "offseason");

  g.contractsClosed();
  g.completeDraft();
  g.auctionAutoRest();
  g.finishOffseason();
  requireSame(cps["season2"]["world"], toJson(g.state().world), "season2.world");
  requireSame(cps["season2"]["career"], toJson(g.state().career), "season2.career");
  requireSame(cps["season2"]["season"], toJson(g.state().season), "season2.season");
  requireSame(cps["season2"]["setup"], setupView(g.state().setup), "season2.setup");
  CHECK(g.state().season.season->number == 2);
  CHECK(g.state().world.manager.target.has_value());
  CHECK(g.state().world.teams.at(team).squad.size() >= 18);
}

// ---------------------------------------------------------------------------
// Quit and relaunch
// ---------------------------------------------------------------------------

TEST_CASE("game: quitting mid-season and relaunching from the save changes nothing", "[game][save]") {
  TempDir dir;
  // One career played straight through...
  Game straight = freshGame(stubSeeds(11));
  straight.startSeason("KNR");
  straight.simFixtures({true, 30, {}});
  straight.simFixtures({true, std::nullopt, {}});
  simToChampion(straight);
  straight.nextSeason();

  // ...and the same career saved after 30 games, the game closed, and reopened from the files
  {
    Game first = freshGame(stubSeeds(11));
    first.startSeason("KNR");
    first.simFixtures({true, 30, {}});
    SaveManager saves(dir.path);
    REQUIRE(saves.flush(first));
    REQUIRE(saves.hasSave());
  }
  SaveManager saves(dir.path);
  std::vector<std::string> notes;
  Game resumed(saves.load(&notes), stubSeeds(11));
  resumed.boot();
  CHECK(resumed.state().career.recorded.size() == 30);
  resumed.simFixtures({true, std::nullopt, {}});
  simToChampion(resumed);
  resumed.nextSeason();

  requireSame(toJson(straight.state().world), toJson(resumed.state().world), "world");
  requireSame(toJson(straight.state().career), toJson(resumed.state().career), "career");
  requireSame(toJson(straight.state().season), toJson(resumed.state().season), "season");
  requireSame(setupToJson(straight.state().setup), setupToJson(resumed.state().setup), "setup");
  CHECK(resumed.state().setup.screen == straight.state().setup.screen);
}

// ---------------------------------------------------------------------------
// Save files
// ---------------------------------------------------------------------------

TEST_CASE("saves: everything written reads back the same; backups are kept; no temp files left", "[save]") {
  TempDir dir;
  Game g = freshGame();
  g.startSeason("GUT");
  g.simFixtures({true, 12, {}});
  SaveManager saves(dir.path);
  REQUIRE(saves.saveAll(g.state()));
  REQUIRE(saves.saveAll(g.state()));  // second write keeps the first as .bak
  for (const char* f : {"world.json", "career.json", "season.json", "setup.json", "meta.json"}) {
    INFO(f);
    CHECK(fs::exists(saves.slotDir() / f));
    CHECK(fs::exists(saves.slotDir() / (std::string(f) + ".bak")));
    CHECK_FALSE(fs::exists(saves.slotDir() / (std::string(f) + ".tmp")));
  }
  const GameState back = saves.load();
  requireSame(toJson(g.state().world), toJson(back.world), "world");
  requireSame(toJson(g.state().career), toJson(back.career), "career");
  requireSame(toJson(g.state().season), toJson(back.season), "season");
  requireSame(setupToJson(g.state().setup), setupToJson(back.setup), "setup");
  const auto meta = Json::parse(readText(saves.slotDir() / "meta.json"));
  REQUIRE(meta.has_value());
  CHECK((*meta)["team"].str() == "GUT");
  CHECK((*meta)["season"].integer() == 1);
}

TEST_CASE("saves: a corrupt file falls back to its backup, then to a fresh start", "[save]") {
  TempDir dir;
  Game g = freshGame();
  g.startSeason("MUI");
  g.simFixtures({true, 8, {}});
  SaveManager saves(dir.path);
  REQUIRE(saves.saveAll(g.state()));
  g.simFixtures({true, 16, {}});
  REQUIRE(saves.saveAll(g.state()));  // .bak now holds the 8-game career
  { std::ofstream(saves.slotDir() / "career.json", std::ios::trunc) << "{\"players\": {broken"; }
  std::vector<std::string> notes;
  const GameState back = saves.load(&notes);
  CHECK(back.career.recorded.size() == 8);
  CHECK(std::any_of(notes.begin(), notes.end(), [](const std::string& n) { return n.find("career.json.bak") != std::string::npos; }));
  // Both copies broken: the career starts again, everything else still loads
  { std::ofstream(saves.slotDir() / "career.json.bak", std::ios::trunc) << "nonsense"; }
  const GameState fresh = saves.load(&notes);
  CHECK(fresh.career.recorded.empty());
  CHECK(fresh.season.season.has_value());
  CHECK(fresh.world.year == 0);
}

TEST_CASE("saves: an empty folder starts a new game", "[save]") {
  TempDir dir;
  SaveManager saves(dir.path);
  CHECK_FALSE(saves.hasSave());
  Game g(saves.load(), stubSeeds());
  g.boot();
  CHECK(g.state().world.players.size() == 160);
  CHECK(g.state().setup.xi.size() == 11);
  CHECK(g.state().setup.userTeamId == "CHK");
}

TEST_CASE("saves: the browser game's saves load (same JSON)", "[save]") {
  TempDir dir;
  const Json golden = readGolden("flow.json");
  const Json& cp = golden["checkpoints"]["seasonDone"];
  const fs::path slot = dir.path / "saves" / "slot1";
  fs::create_directories(slot);
  auto write = [&](const char* name, const Json& j) { std::ofstream(slot / name, std::ios::binary) << j.dump(); };
  write("world.json", cp["world"]);
  write("career.json", cp["career"]);
  write("season.json", cp["season"]);
  Json setup = Json::object();
  setup.add("userTeamId", golden["team"].str());
  setup.add("oppTeamId", "MUI");
  setup.add("pitch", "random");
  write("setup.json", setup);
  SaveManager saves(dir.path);
  Game g(saves.load(), stubSeeds());
  g.boot();
  requireSame(cp["world"], toJson(g.state().world), "world");
  requireSame(cp["career"], toJson(g.state().career), "career");
  requireSame(cp["season"], toJson(g.state().season), "season");
  CHECK(g.state().setup.userTeamId == golden["team"].str());
  // ...and the career carries on from there
  g.nextSeason();
  CHECK(g.state().setup.screen == "offseason");
}

// ---------------------------------------------------------------------------
// lineup_and_settings.test.js
// ---------------------------------------------------------------------------

TEST_CASE("lineup: swapping two players in the XI swaps their batting places", "[game][lineup]") {
  Game g = freshGame();
  const auto xi = g.state().setup.xi;
  g.swapPlayers(xi[0], xi[4]);
  CHECK(g.state().setup.xi[0] == xi[4]);
  CHECK(g.state().setup.xi[4] == xi[0]);
  CHECK(g.state().setup.xi.size() == 11);
}

TEST_CASE("lineup: a bench player swapped in takes the exact batting spot", "[game][lineup]") {
  Game g = freshGame();
  const auto xi = g.state().setup.xi;
  const auto& squad = g.state().world.teams.at(g.state().setup.userTeamId).squad;
  PlayerId bench;
  for (const auto& id : squad)
    if (std::find(xi.begin(), xi.end(), id) == xi.end()) bench = id;
  g.swapPlayers(bench, xi[2]);
  const auto& now = g.state().setup.xi;
  CHECK(now[2] == bench);
  CHECK(std::find(now.begin(), now.end(), xi[2]) == now.end());
  CHECK(now.size() == 11);
}

TEST_CASE("lineup: reordering moves a batter", "[game][lineup]") {
  Game g = freshGame();
  const auto xi = g.state().setup.xi;
  g.reorderXI(0, 3);
  CHECK(g.state().setup.xi[3] == xi[0]);
  CHECK(g.state().setup.xi.size() == 11);
}

TEST_CASE("lineup: the user's batting order is kept across season fixtures", "[game][lineup]") {
  Game g = freshGame();
  g.startSeason("CHK");
  const auto xi = g.state().setup.xi;
  g.swapPlayers(xi[0], xi[4]);
  const auto custom = g.state().setup.xi;
  g.playNextFixture();
  CHECK(g.state().setup.xi == custom);
}

TEST_CASE("settings: tempo and auto-bowl carry from a live match into the next", "[game][lineup]") {
  Game g = freshGame();
  g.chooseTeam("MUI");
  g.startFixture();
  g.setAggression(5);
  g.setAutoBowl(true);
  CHECK(*g.state().match->settings.aggression == 5);
  CHECK(g.state().match->settings.autoBowl);
  g.newGame();
  CHECK(g.state().setup.matchSettings.aggression == 5);
  CHECK(g.state().setup.matchSettings.autoBowl);
  g.startFixture();
  CHECK(*g.state().match->settings.aggression == 5);
  CHECK(g.state().match->settings.autoBowl);
}

TEST_CASE("game: the revision moves with every change, not with reads", "[game]") {
  Game g = freshGame();
  const uint64_t r0 = g.revision();
  (void)g.league();
  (void)g.state();
  CHECK(g.revision() == r0);
  g.chooseTeam("MUI");
  const uint64_t r1 = g.revision();
  CHECK(r1 > r0);
  g.setPitch("dusty");
  CHECK(g.revision() > r1);
}

TEST_CASE("game: a season fixture set up by the Game and played in the Match screen's session lands in the season",
          "[game][session]") {
  Game g = freshGame();
  g.startSeason("MUI");
  g.simFixtures();  // the other games, up to the user's first
  const Fixture* next = nextUserFixture(*g.state().season.season);
  REQUIRE(next);
  const int no = next->no;
  g.playNextFixture();
  REQUIRE(g.state().setup.screen == "xi");
  g.startFixture();
  REQUIRE(g.state().setup.screen == "toss");
  g.callToss("heads");
  if (g.state().match->status == MatchStatus::toss) g.chooseToss(TossDecision::bowl);
  REQUIRE(g.state().match->status == MatchStatus::live);

  app::MatchSession s;
  int recorded = 0;
  s.onComplete = [&](const Match& m) {
    ++recorded;
    g.recordFinishedMatch(m);
  };
  s.adopt(*g.state().match, g.league());
  REQUIRE(s.adopted());
  REQUIRE(s.match().fixture);
  CHECK(s.match().fixture->no == no);
  CHECK(s.homeTeam().id == s.match().fixture->home);
  s.simMatch();
  REQUIRE(s.match().status == MatchStatus::complete);
  CHECK(recorded == 1);

  const Season& season = *g.state().season.season;
  const auto fx = std::find_if(season.fixtures.begin(), season.fixtures.end(), [&](const Fixture& f) { return f.no == no; });
  REQUIRE(fx != season.fixtures.end());
  REQUIRE(fx->result);
  CHECK(fx->result->matchId == s.match().id);
  const auto& ids = g.state().career.recorded;
  CHECK(std::count(ids.begin(), ids.end(), s.match().id) == 1);

  s.clear();
  CHECK_FALSE(s.hasMatch());
  g.backToSeason();
  CHECK(g.state().setup.screen == "season");
  CHECK_FALSE(g.state().match);
}

// ---------------------------------------------------------------------------
// Matches through the Game: the listener
// ---------------------------------------------------------------------------

TEST_CASE("game: a friendly played through to the end is recorded once, with fitness", "[game]") {
  Game g = freshGame();
  g.startFixture();
  REQUIRE(g.state().setup.screen == "toss");
  g.callToss("heads");
  if (g.state().match->status == MatchStatus::toss) g.chooseToss(TossDecision::bat);
  g.setAutoBowl(true);
  g.setAutoReview(true);
  for (int i = 0; i < 400 && g.state().match->status != MatchStatus::complete; ++i) {
    if (g.state().match->status == MatchStatus::break_) g.startChase();
    if (g.state().match->awaiting == Awaiting::batter) {
      const auto& inn = g.state().match->innings[static_cast<size_t>(g.state().match->current)];
      g.pickBatter(inn.yetToBat.front());
      continue;
    }
    g.nextOver();
  }
  g.simMatch();
  REQUIRE(g.state().match->status == MatchStatus::complete);
  const auto& c = g.state().career;
  CHECK(c.recorded.size() == 1);
  CHECK(c.recorded[0] == g.state().match->id);
  CHECK(c.recorded[0].find("-1700000000000") != std::string::npos);  // `${seed}-${Date.now()}`
  CHECK(c.manager.played == 1);  // a captained match counts for the manager
  g.simMatch();                  // nothing more to record
  CHECK(g.state().career.recorded.size() == 1);
  const auto& xi = g.state().match->teams[0].players;
  CHECK(std::any_of(xi.begin(), xi.end(), [&](const PlayerId& id) { return g.state().world.players.at(id).fitness.has_value(); }));
  g.recordFinishedMatch(*g.state().match);  // the same match again: skipped by id
  CHECK(g.state().career.recorded.size() == 1);
}

TEST_CASE("game: a season fixture played ball by ball counts in the season and the board", "[game]") {
  Game g = freshGame();
  g.startSeason("RJR");
  // Sim up to the user's first game, then play it
  g.simFixtures({false, std::nullopt, {}});
  g.playNextFixture();
  REQUIRE(g.state().setup.screen == "xi");
  const int no = g.state().setup.fixture->no;
  const double conf = g.state().world.manager.confidence;
  g.startFixture();
  g.callToss("tails");
  if (g.state().match->status == MatchStatus::toss) g.chooseToss(TossDecision::bowl);
  g.simMatch();
  REQUIRE(g.state().match->status == MatchStatus::complete);
  const Fixture* fx = nullptr;
  for (const auto& f : g.state().season.season->fixtures)
    if (f.no == no) fx = &f;
  REQUIRE(fx);
  CHECK(fx->result.has_value());
  if (fx->result->winner) CHECK(g.state().world.manager.confidence != conf);  // won or lost: the board noticed
  g.backToSeason();
  CHECK(g.state().setup.screen == "season");
  CHECK_FALSE(g.state().match.has_value());
}

// ---------------------------------------------------------------------------
// The board through the Game (finance.test.js) and the create-player store tests
// ---------------------------------------------------------------------------

TEST_CASE("game: the board sets a target, reacts, sacks and re-hires", "[game]") {
  Game g = freshGame();
  g.startSeason("MUI");
  CHECK(g.state().world.manager.team == std::optional<TeamId>("MUI"));
  CHECK_FALSE(g.state().world.manager.target->label.empty());
  // The board wanted the title and has no patience left: anything short of it is the sack
  for (int s = 0; s < 3 && !g.state().world.manager.sacked; ++s) {
    simToChampion(g);
    g.state().world.manager.confidence = 0;
    g.state().world.manager.target->need = 1;
    g.nextSeason();
    if (g.state().setup.screen == "offseason") {
      g.contractsClosed();
      g.completeDraft();
      g.auctionAutoRest();
      g.finishOffseason();
    }
  }
  REQUIRE(g.state().world.manager.sacked.has_value());
  CHECK(g.state().setup.screen == "sacked");
  const TeamId offer = g.state().world.manager.sacked->offers[0];
  g.acceptJob(offer);
  CHECK(g.state().world.manager.team == offer);
  CHECK(g.state().world.manager.confidence == 55);
  CHECK(g.state().world.offseason->userTeam == offer);
  CHECK(g.state().setup.screen == "offseason");
  CHECK(g.state().world.manager.history.back().event == std::optional<std::string>("sacked"));
}

TEST_CASE("game: a new player made club captain goes into the XI and leads the next match", "[game][create]") {
  Game g = freshGame();
  const TeamId team = g.state().setup.userTeamId;
  EditDraft d = draftFrom(newPlayerTemplate(Role::BAT));
  d.name = "Skip Per";
  d.age = 23;
  for (size_t i = 0; i < SkillSet<BatK>::N; ++i) d.skills.bat.set(static_cast<BatK>(i), 40);
  d.captain = true;
  const CreateOutcome res = g.createNewPlayer(team, d);
  REQUIRE(res.ok);
  CHECK(clubCaptainOf(g.state().world, team) == res.id);
  const auto& xi = g.state().setup.xi;
  CHECK(std::find(xi.begin(), xi.end(), res.id) != xi.end());
  CHECK(xi.size() == 11);
  CHECK(g.state().setup.captainId == res.id);
  g.autoPick();
  CHECK(std::find(g.state().setup.xi.begin(), g.state().setup.xi.end(), res.id) != g.state().setup.xi.end());
  g.startFixture();
  CHECK(g.state().match->teams[0].captainId == res.id);
  // Refusals change nothing
  const World before = g.state().world;
  EditDraft dup = d;
  dup.captain = false;
  CHECK(g.createNewPlayer(team, dup).errors.contains("name"));
  CHECK(g.createNewPlayer("NOPE", d).errors.contains("form"));
  CHECK(g.state().world.nextId == before.nextId);
}

TEST_CASE("game: club captain for a bench player, an injured one, and choosing in Pick XI", "[game][create]") {
  Game g = freshGame();
  const TeamId team = g.state().setup.userTeamId;
  const auto xi0 = g.state().setup.xi;
  PlayerId bench;
  for (const auto& id : g.state().world.teams.at(team).squad)
    if (std::find(xi0.begin(), xi0.end(), id) == xi0.end()) bench = id;
  const CaptainOutcome made = g.makeClubCaptain(bench);
  CHECK(made.ok);
  CHECK(made.inXI);
  CHECK(g.state().setup.captainId == bench);
  CHECK(g.state().setup.xi.size() == 11);
  // Injured: named captain, XI untouched
  PlayerId other;
  for (const auto& id : g.state().world.teams.at(team).squad)
    if (std::find(g.state().setup.xi.begin(), g.state().setup.xi.end(), id) == g.state().setup.xi.end()) other = id;
  g.state().world.players.at(other).injury = Injury{"hamstring", "Hamstring", 3, 0, std::nullopt};
  const auto xi1 = g.state().setup.xi;
  const CaptainOutcome hurt = g.makeClubCaptain(other);
  CHECK(hurt.ok);
  CHECK(hurt.injured);
  CHECK(g.state().setup.xi == xi1);
  // Pick XI's C toggle names the club captain
  const PlayerId pick = g.state().setup.xi[8];
  g.chooseCaptain(pick);
  CHECK(clubCaptainOf(g.state().world, team) == pick);
  g.removeClubCaptain(team);
  CHECK_FALSE(clubCaptainOf(g.state().world, team).has_value());
  CHECK_FALSE(g.state().setup.captainId.has_value());
}

TEST_CASE("game: player edits save through the Game and clear development", "[game][create]") {
  Game g = freshGame();
  g.startSeason("CHK");
  g.simFixtures({true, 10, {}});
  const PlayerId id = "CHK-1";
  REQUIRE(g.state().career.progress.contains(id));
  EditDraft d = draftFrom(g.state().world.players.at(id));
  d.name = "Test Player";
  d.skills.bat.set(BatK::lofted, 99);
  REQUIRE(g.savePlayerEdit(id, d).ok);
  CHECK_FALSE(g.state().career.progress.contains(id));
  CHECK(g.state().world.players.at(id).power == 99);
  g.restorePlayer(id);
  CHECK(g.state().world.players.at(id).name != "Test Player");
}
