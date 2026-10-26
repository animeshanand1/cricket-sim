#pragma once
// Player popularity 0-100% (popularity.js): moves after every completed match on what he did with
// bat, ball and in the field. Display only.

#include <functional>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/match_types.hpp"

namespace t20 {

double startPopularity(const Player* p);

struct Appeal {
  double bat = 0, bowl = 0, field = 0, bonus = 0, total = 0;
};
OrderedMap<PlayerId, Appeal> matchAppeal(const Match& match);
void updatePopularity(Career& career, const Match& match);

struct PopularRow {
  PlayerId id;
  double v, d;
};
std::vector<PopularRow> topPopular(const OrderedMap<PlayerId, Popularity>& popularity, size_t n = 5,
                                   const std::function<bool(const PlayerId&)>& keep = nullptr);

}  // namespace t20
