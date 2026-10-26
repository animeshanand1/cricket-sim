#include "ui/widgets/stepper.hpp"
#include "ui/theme.hpp"

namespace t20::ui {

void stepper(Screen currentScreen) {
  // Only display on xi, training, and toss
  if (currentScreen != Screen::xi &&
      currentScreen != Screen::training &&
      currentScreen != Screen::toss) {
    return;
  }

  struct StepInfo {
    Screen screen;
    const char* label;
  };

  const StepInfo steps[] = {
    { Screen::home,     "Teams" },
    { Screen::xi,       "Playing XI" },
    { Screen::training, "Training" },
    { Screen::toss,     "Toss" },
    { Screen::match,    "Match" }
  };

  int currentIdx = 0;
  for (int i = 0; i < 5; ++i) {
    if (steps[i].screen == currentScreen) {
      currentIdx = i;
      break;
    }
  }

  // Draw 5 pills in a horizontal flex layout:
  // i < currentIdx: is-done (green disc with checkmark/number)
  // i == currentIdx: is-current (gold-soft bg, gold border, gold disc with number)
  // i > currentIdx: pending (muted disc)
  (void)currentIdx;
}

} // namespace t20::ui
