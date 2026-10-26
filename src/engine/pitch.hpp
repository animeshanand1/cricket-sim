#pragma once
// Pitch conditions and how they bend the ball-by-ball probabilities (pitch.js).
// `par` is a typical first-innings score. `wear` describes how the surface changes as the match
// goes on: spin = extra spin-wicket factor at full wear, paceDecay = share of the seam movement
// that is gone, boundary = change in boundary rate at full wear.

#include <array>
#include <string_view>

#include "engine/types.hpp"

namespace t20 {

struct PitchWear {
  double spin = 0, paceDecay = 0, boundary = 0;
};

struct PitchDef {
  Pitch key;
  std::string_view label, report;
  int par;
  double boundary, dot, wicket, paceWicket, spinWicket;
  PitchWear wear;
};

const PitchDef& pitchDef(Pitch p);

inline constexpr std::array<Pitch, 5> PITCH_KEYS = {Pitch::balanced, Pitch::flat, Pitch::green, Pitch::dusty,
                                                    Pitch::slow};

}  // namespace t20
