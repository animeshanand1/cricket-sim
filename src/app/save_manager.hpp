#pragma once
// Saves (docs/cpp-migration/07-persistence.md). One folder per slot under the app's save folder:
//
//   saves/slot1/career.json   the browser's t20coach.career.v2   (same JSON)
//               season.json   t20coach.season.v2                 ({ season, history })
//               world.json    t20coach.world.v1
//               setup.json    t20coach.setup.v2
//               meta.json     { format, gameVersion, savedAt, team, season, year }
//
// Writes are atomic (write X.tmp, keep the old file as X.bak, rename) so a crash never leaves a
// half-written save; a missing or corrupt file falls back to X.bak, then to a fresh start. The
// match in progress is not saved (the browser game doesn't either).

#include <filesystem>
#include <string>
#include <vector>

#include "app/game.hpp"
#include "app/game_state.hpp"

namespace t20::app {

inline constexpr int SAVE_FORMAT = 1;
inline constexpr const char* GAME_VERSION = "0.5.0";

class SaveManager {
 public:
  // root: the app's save folder (SDL_GetPrefPath); the slot lives in root/saves/<slot>
  explicit SaveManager(std::filesystem::path root, std::string slot = "slot1");

  const std::filesystem::path& slotDir() const { return dir_; }

  // Read the slot, or start fresh for any part that's missing or unreadable. Never throws.
  // notes: what was loaded or why something was skipped (for the log).
  GameState load(std::vector<std::string>* notes = nullptr) const;
  bool hasSave() const;

  // Write the parts that changed (world, career, season, setup) and meta.json
  bool save(const GameState& s, const Game::Dirty& what);
  bool saveAll(const GameState& s);

  // Autosave: at most once every `interval` seconds, only what changed
  void tick(Game& game, double dt, double interval = 1.0);
  // On quit and after big steps: write whatever has changed now
  bool flush(Game& game);

  const std::string& lastError() const { return error_; }

 private:
  bool writeFile(const std::string& name, const std::string& text);

  std::filesystem::path dir_;
  std::string error_;
  double clock_ = 0;
  Game::Dirty pending_;
};

// Read a whole file (empty if missing)
std::string readText(const std::filesystem::path& p);

}  // namespace t20::app
