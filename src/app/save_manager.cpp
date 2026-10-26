#include "app/save_manager.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

#include "engine/league_view.hpp"
#include "engine/state_json.hpp"
#include "engine/world.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace t20::app {

namespace fs = std::filesystem;

namespace {

// Replace `to` with `from` in one step where the OS allows it
bool replaceFile(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
  return MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code ec;
  fs::rename(from, to, ec);
  return !ec;
#endif
}

std::optional<Json> readJson(const fs::path& p, std::string* why) {
  const std::string text = readText(p);
  if (text.empty()) {
    if (why) *why = "missing";
    return std::nullopt;
  }
  std::string err;
  auto j = Json::parse(text, &err);
  if (!j && why) *why = "unreadable (" + err + ")";
  return j;
}

// The file, or its backup if the file is missing or corrupt
std::optional<Json> readWithBackup(const fs::path& dir, const std::string& name, std::vector<std::string>* notes) {
  std::string why;
  if (auto j = readJson(dir / name, &why)) return j;
  std::string whyBak;
  if (auto j = readJson(dir / (name + ".bak"), &whyBak)) {
    if (notes) notes->push_back(name + " " + why + ": using " + name + ".bak");
    return j;
  }
  if (notes && why != "missing") notes->push_back(name + " " + why + " and no usable backup: starting it fresh");
  return std::nullopt;
}

std::string isoNow() {
  const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

}  // namespace

std::string readText(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

SaveManager::SaveManager(fs::path root, std::string slot) : dir_(std::move(root) / "saves" / slot) {}

bool SaveManager::hasSave() const {
  std::error_code ec;
  return fs::exists(dir_ / "world.json", ec) || fs::exists(dir_ / "world.json.bak", ec);
}

GameState SaveManager::load(std::vector<std::string>* notes) const {
  GameState s;
  // career: accepted if it has players and a recorded list (careerSlice loadCareer)
  if (auto j = readWithBackup(dir_, "career.json", notes); j && (*j)["players"].isObject() && (*j)["recorded"].isArray())
    s.career = careerFromJson(*j);
  // season: { season, history } (seasonSlice loadSeason)
  if (auto j = readWithBackup(dir_, "season.json", notes); j && j->find("season") && (*j)["history"].isArray())
    s.season = seasonStateFromJson(*j);
  // world: players and teams, brought up to date (worldSlice loadWorld)
  if (auto j = readWithBackup(dir_, "world.json", notes); j && (*j)["players"].isObject() && (*j)["teams"].isObject()) {
    s.world = worldFromJson(*j);
    upgradeWorld(s.world);
    // A save without the ten franchises can't be played: start the world again
    for (const auto& t : LEAGUE())
      if (!s.world.teams.contains(t.id)) {
        if (notes) notes->push_back("world.json has no " + t.id + ": starting a new world");
        s.world = createWorld(LEAGUE());
        break;
      }
  } else {
    s.world = createWorld(LEAGUE());
  }
  const auto setup = readWithBackup(dir_, "setup.json", notes);
  s.setup = setupFromSaved(setup ? &*setup : nullptr);
  if (notes)
    notes->push_back("loaded: " + std::to_string(s.career.recorded.size()) + " matches recorded, " +
                     std::to_string(s.season.history.size()) + " seasons archived, world year " +
                     std::to_string(s.world.year));
  return s;
}

bool SaveManager::writeFile(const std::string& name, const std::string& text) {
  std::error_code ec;
  fs::create_directories(dir_, ec);
  if (ec) {
    error_ = "cannot create " + dir_.string() + ": " + ec.message();
    return false;
  }
  const fs::path file = dir_ / name;
  const fs::path tmp = dir_ / (name + ".tmp");
  const fs::path bak = dir_ / (name + ".bak");
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      error_ = "cannot write " + tmp.string();
      return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    if (!out) {
      error_ = "write failed for " + tmp.string();
      return false;
    }
  }
  if (fs::exists(file, ec) && !replaceFile(file, bak)) {
    error_ = "cannot keep a backup of " + file.string();
    return false;
  }
  if (!replaceFile(tmp, file)) {
    error_ = "cannot replace " + file.string();
    return false;
  }
  return true;
}

bool SaveManager::save(const GameState& s, const Game::Dirty& what) {
  bool ok = true;
  if (what.world) ok = writeFile("world.json", toJson(s.world).dump()) && ok;
  if (what.career) ok = writeFile("career.json", toJson(s.career).dump()) && ok;
  if (what.season) ok = writeFile("season.json", toJson(s.season).dump()) && ok;
  if (what.setup) ok = writeFile("setup.json", setupToJson(s.setup).dump()) && ok;
  if (what.world || what.career || what.season || what.setup) {
    Json meta = Json::object();
    meta.add("format", SAVE_FORMAT);
    meta.add("gameVersion", GAME_VERSION);
    meta.add("savedAt", isoNow());
    meta.add("team", s.world.manager.team ? Json(*s.world.manager.team) : Json(s.setup.userTeamId));
    meta.add("season", s.season.season ? Json(s.season.season->number) : Json());
    meta.add("year", s.world.year);
    ok = writeFile("meta.json", meta.dump()) && ok;
  }
  return ok;
}

bool SaveManager::saveAll(const GameState& s) { return save(s, Game::Dirty{true, true, true, true, true}); }

void SaveManager::tick(Game& game, double dt, double interval) {
  const Game::Dirty d = game.takeDirty();
  pending_.setup |= d.setup;
  pending_.career |= d.career;
  pending_.season |= d.season;
  pending_.world |= d.world;
  clock_ += dt;
  if (clock_ < interval || !(pending_.setup || pending_.career || pending_.season || pending_.world)) return;
  clock_ = 0;
  if (save(game.state(), pending_)) pending_ = Game::Dirty{};
}

bool SaveManager::flush(Game& game) {
  const Game::Dirty d = game.takeDirty();
  pending_.setup |= d.setup;
  pending_.career |= d.career;
  pending_.season |= d.season;
  pending_.world |= d.world;
  if (!(pending_.setup || pending_.career || pending_.season || pending_.world)) return true;
  const bool ok = save(game.state(), pending_);
  if (ok) pending_ = Game::Dirty{};
  clock_ = 0;
  return ok;
}

}  // namespace t20::app
