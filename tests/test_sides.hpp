#pragma once
// Sides as the JS test files build them: no captaincy style is passed, so the AI captains
// every side 'balanced' (createMatch's default). Using the team's real style would play
// different matches from the JS tests.

#include "parity/parity.hpp"

namespace t20 {

inline SideInput testSide(const Team& team, Pitch pitch, bool withBench = true) {
  SideInput s = parity::side(team, pitch, withBench);
  s.captain = Captain::balanced;
  return s;
}

}  // namespace t20
