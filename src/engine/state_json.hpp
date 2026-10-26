#pragma once
// The game state as JSON, in the same shape as the browser game's saves (JSON.stringify of the
// Redux state), so saves can move between the two and parity can be checked field by field.
// Readers are forgiving: missing or null fields fall back to defaults, never throw.

#include "core/json.hpp"
#include "engine/career_types.hpp"
#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

Json toJson(const Player& p);
Player playerFromJson(const Json& j);

Json toJson(const World& w);
World worldFromJson(const Json& j);

Json toJson(const Career& c);
Career careerFromJson(const Json& j);

Json toJson(const MatchSummary& s);
MatchSummary summaryFromJson(const Json& j);
Json toJson(const Season& s);
Season seasonFromJson(const Json& j);
// { season, history } (what the browser saves; busy is not saved)
Json toJson(const SeasonState& s);
SeasonState seasonStateFromJson(const Json& j);

}  // namespace t20
