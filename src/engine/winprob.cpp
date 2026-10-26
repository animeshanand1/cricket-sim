#include "engine/winprob.hpp"

#include <algorithm>

#include "core/jsmath.hpp"
#include "engine/match.hpp"

namespace t20 {

namespace {
// A copy of the match ready to sim: no commentary, AI in charge of both sides
Match baseForSims(const Match& match) {
  Match c = match;
  for (Innings& inn : c.innings) inn.commentary.clear();
  c.userTeam.reset();
  c.quiet = true;
  return c;
}

int32_t reseed(int32_t state, int i) {
  return js_add32(js_imul(state ^ static_cast<int32_t>(0x9e3779b9u), kGolden), js_imul(i + 1, 40503));
}
}  // namespace

WinChanceRun::WinChanceRun(const Match& match, int n) : base_(baseForSims(match)), state_(match.rngState), n_(n) {}

bool WinChanceRun::step(int k) {
  for (const int end = std::min(n_, i_ + k); i_ < end; ++i_) {
    Match c = base_;
    c.rngState = reseed(state_, i_);
    simMatch(c);
    const std::optional<int> w = c.result ? c.result->winner : std::nullopt;
    if (w) {
      wins_[static_cast<size_t>(*w)] += 1;
    } else {
      wins_[0] += 0.5;
      wins_[1] += 0.5;
    }
  }
  return done();
}

std::array<double, 2> WinChanceRun::result() const {
  const double n = std::max(1, i_);
  return {wins_[0] / n, wins_[1] / n};
}

std::array<double, 2> winChance(const Match& match, int n) {
  WinChanceRun run(match, n);
  run.step(n);
  return run.result();
}

}  // namespace t20
