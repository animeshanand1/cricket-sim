#pragma once
// Batter v bowler head-to-head records (matchups.js), kept in the career save as
// { "batterId|bowlerId": [balls, runs, outs, fours, sixes] }. A long enough history gives one of
// them an edge in the next match.

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/match_types.hpp"

namespace t20 {

using MatchupRow = std::array<int, 5>;
using MatchupBook = OrderedMap<std::string, MatchupRow>;

inline std::string matchupKey(const PlayerId& bat, const PlayerId& bowl) { return bat + "|" + bowl; }

// Every batter v bowler pairing in one match (Super Overs don't count)
MatchupBook matchupsOf(const Match& match);
// Add one completed match to the saved records
void recordMatchups(MatchupBook& saved, const Match& match);

inline constexpr int MIN_BALLS = 12;
// The edge in each pairing of a match: -1 (the bowler owns him) .. +1 (the batter owns him)
OrderedMap<std::string, double> matchupEdges(const MatchupBook* saved,
                                             const std::array<std::vector<PlayerId>, 2>& ids);

struct EdgeEffect {
  double W, boundary;
};
EdgeEffect edgeEffect(double e);
// "batter" / "bowler" once the edge is big enough to talk about, else empty
std::string_view edgeVerdict(double e);

struct HeadToHead {
  int balls, runs, outs, fours, sixes;
  double sr;
};
std::optional<HeadToHead> headToHead(const MatchupBook* saved, const MatchupBook* live, const PlayerId& bat,
                                     const PlayerId& bowl);
std::string h2hText(const std::optional<HeadToHead>& h);

}  // namespace t20
