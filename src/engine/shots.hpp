#pragma once
// Where each ball went (shots.js).
//  - shotDirection(): the engine's call on where a scoring shot is played (match stream).
//  - shotFor(): a cosmetic placement for balls the field can't affect, from a hash of the ball's
//    position in the match, so it never touches the random stream.
// Angles: degrees from straight (0), positive = off side for a right-hander; mirrored for
// left-handers when stored on the ball. Distance is a share of the boundary radius.

#include <cstdint>
#include <optional>
#include <string_view>

#include "engine/match_types.hpp"
#include "engine/rng.hpp"

namespace t20 {

struct Placement {
  double dir, dist;
};
// kind: "wicket" when wicketType is set, else a run (or dot)
Placement shotFor(int32_t key, int runs, std::optional<Out> wicketType, Hand hand);

enum class Region : uint8_t { straight, offDrive, cover, point, third, midwicket, square, fine };
Region regionOf(double dir);
double planLean(Plan plan, Region region);

struct BatterLean {
  double off;
  bool puller;
};
BatterLean batterLean(const Player& p);

// Where a scoring shot goes, batter-relative (not yet mirrored for a left-hander). runs: 1-6.
double shotDirection(RandFn r, int runs, const Player& batter, Plan plan, bool spin);

// A stable key for a ball: seed, innings, legal balls so far, balls in this over
int32_t ballKey(int32_t seed, int innings, int legalBalls, int idxInOver);

}  // namespace t20
