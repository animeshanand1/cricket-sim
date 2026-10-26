#pragma once
// Building the two sides for a match (sides.js): current (developed) players, fatigue, starting
// confidence from form, training boosts, the captain and the 5 named Impact Player substitutes.

#include <optional>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/development.hpp"
#include "engine/match.hpp"
#include "engine/season_types.hpp"

namespace t20 {

using CareerPlayers = OrderedMap<PlayerId, CareerStats>;

// captainId: the player who leads the side; if he isn't named or isn't playing, the club captain
// (team.captainId), and failing that the natural leader of the XI.
SideInput buildSide(const Team& team, const std::vector<PlayerId>& ids, const PlayerId& keeperId,
                    const CareerPlayers* careerPlayers = nullptr, const std::vector<TrainingSlot>& training = {},
                    const std::optional<PlayerId>& captainId = std::nullopt);

// The AI's full preparation for a side: best XI for the pitch plus a training plan
SideInput aiSide(const Team& team, Pitch pitch, const CareerPlayers* careerPlayers);

// Simulate a fixture between two AI-managed sides (home first)
Match simFixture(const Fixture& fixture, const std::vector<Team>& league, const CareerPlayers* careerPlayers,
                 const MatchupBook* matchups = nullptr);

}  // namespace t20
