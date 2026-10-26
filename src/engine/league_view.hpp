#pragma once
// The league as screens and matches see it (data.js): the world's squads with each player's
// match-by-match development on top, injured players marked unavailable.

#include <optional>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

// The league exactly as generated from the roster, before any seasons are played
const std::vector<Team>& LEAGUE();
// A roster player as first generated (roster ids only)
const Player* playerById(const PlayerId& id);
const Team* teamById(const std::vector<Team>& league, const TeamId& id);

// The world's squads, developed. devBase records the skills development is measured from.
std::vector<Team> effectiveLeague(const World& world, const OrderedMap<PlayerId, Progress>& progress);
// Every player the world knows (squads, free agents, retired), developed
OrderedMap<PlayerId, Player> allPlayersIn(const World& world, const OrderedMap<PlayerId, Progress>& progress);

}  // namespace t20
