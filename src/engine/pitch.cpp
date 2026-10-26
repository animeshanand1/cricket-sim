#include "engine/pitch.hpp"

namespace t20 {

namespace {
// clang-format off
const std::array<PitchDef, 5> kPitches = {{
  {Pitch::balanced, "Good cricket wicket", "Even bounce and a bit for everyone. Par score around 175.",
   175, 1.0, 1.0, 1.0, 1.0, 1.0, {0.12, 0, 0}},
  {Pitch::flat, "Flat deck", "Hard, true surface with short boundaries. Expect 190-plus.",
   198, 1.14, 0.95, 0.9, 1.0, 0.95, {0, 0, 0}},
  {Pitch::green, "Green top", "Tinge of grass and early movement. Seamers will fancy it early; it should flatten out as it dries.",
   165, 0.92, 1.04, 1.0, 1.25, 0.85, {0, 0.7, 0.06}},
  {Pitch::dusty, "Dusty turner", "Dry and cracked. Spinners will grip it from the first over, and it will only turn more.",
   158, 0.94, 1.04, 1.0, 0.9, 1.32, {0.4, 0, -0.06}},
  {Pitch::slow, "Slow and low", "Two-paced and holding in the surface. Timing will be hard, and harder still later on.",
   155, 0.87, 1.08, 1.0, 1.0, 1.1, {0.1, 0, -0.08}},
}};
// clang-format on
}  // namespace

const PitchDef& pitchDef(Pitch p) { return kPitches[static_cast<size_t>(p)]; }

}  // namespace t20
