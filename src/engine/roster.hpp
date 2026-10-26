#pragma once
// The league's fixed squads (roster.js). The table itself is generated into roster_data.cpp
// by tools/parity/gen_roster.mjs, so the JS file stays the single source of truth.

#include <optional>
#include <string_view>
#include <vector>

#include "engine/types.hpp"

namespace t20 {

// [name, role, hand, bowlType, bat, power, bowl, field, age, overseas, batStyle, bowlStyle]
struct RosterRow {
  std::string_view name;
  Role role;
  Hand hand;
  std::optional<BowlType> bowlType;
  int bat, power, bowl, field, age;
  bool overseas;
  std::string_view batStyle;   // code (PH, AN, ...) or empty
  std::string_view bowlStyle;  // code (SW, BT, ...) or empty
};

struct RosterTeam {
  TeamInfo info;
  std::vector<RosterRow> players;
};

const std::vector<RosterTeam>& ROSTER();

// BAT_STYLE_CODE / BOWL_STYLE_CODE: code -> style name, or empty if unknown
std::string_view batStyleFromCode(std::string_view code);
std::string_view bowlStyleFromCode(std::string_view code);

}  // namespace t20
