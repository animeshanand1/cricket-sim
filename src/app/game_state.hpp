#pragma once
// Everything the game knows, in one place (the C++ form of the Redux store: setup, match, career,
// ui, season, world). Field names follow the JS so the saves match the browser's.

#include <optional>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/ordered_map.hpp"
#include "engine/career_types.hpp"
#include "engine/development.hpp"
#include "engine/match_types.hpp"
#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20::app {

struct Lineup {
  std::vector<PlayerId> xi;
  std::optional<PlayerId> keeperId, captainId;
};

struct MatchPrefs {
  double aggression = 3;
  bool autoBowl = false;
};

// setupSlice.js. screen: home | xi | training | toss | match | season | offseason | draft | players
// | club | sacked (strings, as in the JS)
struct SetupState {
  std::string screen = "home";
  std::string returnTo = "home";
  TeamId userTeamId, oppTeamId;
  std::string pitch = "random";  // a pitch key or "random"
  std::vector<TrainingSlot> training;
  std::optional<FixtureRef> fixture;
  std::vector<PlayerId> xi;
  std::optional<PlayerId> keeperId, captainId;
  OrderedMap<TeamId, Lineup> lineups;
  MatchPrefs matchSettings;
};

struct UiState {
  std::optional<PlayerId> cardPlayerId;
  bool cardEdit = false;
};

struct GameState {
  SetupState setup;
  std::optional<Match> match;
  Career career;
  UiState ui;
  SeasonState season;
  World world;
};

// The saved part of setup ({ userTeamId, oppTeamId, pitch, lineups, matchSettings }) and the
// state built from it on boot (setupSlice initialState)
Json setupToJson(const SetupState& s);
SetupState setupFromSaved(const Json* saved);

}  // namespace t20::app
