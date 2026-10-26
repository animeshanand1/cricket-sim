#pragma once

#include "core/types.hpp"

namespace t20::ui {

// Setup progress stepper for match:
// 1 Teams -> 2 Playing XI -> 3 Training -> 4 Toss -> 5 Match
// Done steps: green disc
// Current step: gold border + gold-soft background + gold disc
// Only visible on xi, training, and toss screens
void stepper(Screen currentScreen);

} // namespace t20::ui
