#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// The UI shares the engine's ids and enums (Role, TossDecision, ...) instead of redefining them
#include "engine/types.hpp"
#include "engine/match_types.hpp"

namespace t20 {

// Screen navigation identifier (matches setup.screen from JS)
enum class Screen {
  home,
  xi,
  training,
  toss,
  match,
  season,
  offseason,
  draft,
  sacked,
  club,
  players,
  gallery  // the widget gallery (A6), only from the command line
};

// High-level view router resolution
enum class View {
  home,
  xi,
  training,
  toss,
  match,
  season,
  offseason,
  sacked,
  club,
  players,
  gallery
};

inline const char* roleToString(Role r) { return name(r).data(); }

// Lightweight structures for UI representation
struct TeamMeta {
  TeamId id;
  std::string name;
  std::string shortName;
  std::string color;      // hex code e.g. "#004ba0"
  std::string secondaryColor;
};

struct PlayerMeta {
  PlayerId id;
  std::string name;
  Role role = Role::BAT;
  std::string teamId;
  int rating = 70;
  int batSkill = 70;
  int bowlSkill = 20;
  float fitness = 100.0f;
  int age = 24;
  bool overseas = false;
  std::string batStyle;   // "RHB" or "LHB"
  std::string bowlStyle;  // "RF", "RM", "OB", "LB", "SLA"
  std::string specialDelivery;
  struct Injury {
    std::string label;
    int matches = 0;
  };
  std::optional<Injury> injury;
};

struct BallPipData {
  std::string text;       // "•", "1", "4", "6", "W", "1nb", "wd"
  int runs = 0;
  bool isWicket = false;
  bool isFour = false;
  bool isSix = false;
  bool isExtra = false;
  bool isSpecial = false; // special delivery
  bool isNewest = false;
};

enum class Coin { heads, tails };
enum class FieldChoice { attacking, balanced, defensive, boundaryRiders, slipRing };

} // namespace t20
