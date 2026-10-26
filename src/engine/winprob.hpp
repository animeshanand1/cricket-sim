#pragma once
// Win probability (winprob.js): play the rest of the match out many times from where it stands,
// with AI captains on both sides, and count who wins. Ties and no results count half.

#include <array>

#include "engine/match_types.hpp"

namespace t20 {

class WinChanceRun {
 public:
  WinChanceRun(const Match& match, int n = 120);
  // Run up to k more sims; returns true when all n are done
  bool step(int k = 8);
  bool done() const { return i_ >= n_; }
  std::array<double, 2> result() const;

 private:
  Match base_;
  int32_t state_;
  int n_;
  int i_ = 0;
  std::array<double, 2> wins_{0, 0};
};

// All at once (scripts and tests)
std::array<double, 2> winChance(const Match& match, int n = 120);

}  // namespace t20
