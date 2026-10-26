#pragma once
// Ball-by-ball commentary lines (commentary.js). The line pickers draw from the match stream,
// so the pools must stay identical to the JS or replays diverge.

#include <optional>
#include <string>
#include <string_view>

#include "engine/match_types.hpp"
#include "engine/rng.hpp"
#include "engine/shots.hpp"

namespace t20 {

// "Virat Kohli" -> "Kohli"; a one-word name stays as it is
std::string surname(const std::string& name);

struct CatchAt {
  std::string_view label;
  int zone;  // 0 close, 1 ring, 2 deep (Zone)
  bool rope;
};

struct BallTextArgs {
  const Player* bowler = nullptr;
  const Player* striker = nullptr;
  int runs = 0;
  bool spin = false;
  enum class Kind { normal, wide, noball } kind = Kind::normal;
  const Player* fielder = nullptr;
  std::optional<Out> wicketType;
  bool legBye = false;
  std::optional<double> kph;
  int over = 10;
  std::optional<CatchAt> catchAt;
};
std::string ballText(RandFn rng, const BallTextArgs& a);

struct Saver {
  std::string name;
  std::string_view label;
  int zone;  // 0 close, 1 ring, 2 deep
};
// A scoring shot (or one the field stopped). hit: the runs the shot was worth; runs: what they got.
std::string shotText(RandFn rng, const Player& bowler, const Player& striker, Region region, int hit, int runs,
                     const std::optional<Saver>& saver);

std::string byeText(const Player& bowler, const Player& striker, int runs);
std::string overthrowText(int n);

}  // namespace t20
