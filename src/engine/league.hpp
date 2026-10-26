#pragma once
// The league: 10 franchises with 16-player squads from the fixed roster (league.js).
// Detailed skills are derived from each player's ratings, seeded per player.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/types.hpp"

namespace t20 {

inline constexpr int32_t LEAGUE_SEED = 2014;
// IPL rule: at most 4 overseas players in the XI
inline constexpr int MAX_OVERSEAS = 4;
// The 5 named substitutes for the Impact Player rule
inline constexpr int IMPACT_SUBS = 5;

struct BowlTypeDef {
  std::string_view label, shortName;
  bool spin;
};
const BowlTypeDef& bowlTypeDef(BowlType t);
std::string_view roleLabel(Role r);

std::vector<TeamInfo> TEAMS();

bool isSpinner(const Player& p);

std::vector<Team> generateLeague(int32_t seed = LEAGUE_SEED);

struct TeamRatings {
  double batting, bowling;
};
TeamRatings teamRatings(const Team& team);

struct BestXI {
  std::vector<PlayerId> ids;
  PlayerId keeperId;
};
// Best XI for a pitch: keeper, 4 batters, 3 all-rounders, 3 bowlers, at least 5 bowling options,
// at most 4 overseas.
BestXI pickBestXI(const Team& team, Pitch pitchType = Pitch::balanced);
// Bring a fit captain into an XI in place of the weakest player of his role (the keeper stays
// unless the captain keeps wicket; an overseas captain replaces an overseas player at the limit)
BestXI withCaptain(const Team& team, const std::vector<PlayerId>& ids, const PlayerId& keeperId,
                   const PlayerId& captainId);

// The natural leader of an XI when nobody has been named
std::optional<PlayerId> pickCaptain(const std::vector<const Player*>& players);

std::vector<PlayerId> benchOf(const Team& team, const std::vector<PlayerId>& xiIds);

// Sensible batting order: best batters first, bowlers last (stable)
std::vector<const Player*> battingOrder(std::vector<const Player*> players);

}  // namespace t20
