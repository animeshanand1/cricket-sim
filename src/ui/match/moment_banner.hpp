#pragma once

#include "core/color.hpp"
#include <string>

namespace t20::ui {

enum class MomentKind {
  Four,       // blue gradient #1f6fd1 -> #52abff
  Six,        // gold gradient #f5b83d -> #ffdf8a (dark text)
  Wicket,     // red gradient #b3242a -> #ff5d62
  Milestone,  // purple gradient #6a4bd4 -> #a88bfa
  Result      // green gradient #0c9563 -> #37d39a
};

struct MomentEvent {
  MomentKind kind = MomentKind::Four;
  std::string title;
  std::string subtitle;
};

// Trigger a moment banner animation (1.9s slide-in, hold, slide-out with skew -8 deg)
void triggerMomentBanner(const MomentEvent& evt);

// Draw the moment banner if active
void drawMomentBanner(void* drawList, float posX, float posY);

} // namespace t20::ui
