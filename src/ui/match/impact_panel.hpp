#pragma once
// S05 §7 Impact Player panel (ImpactPanel.jsx): one substitution from the 5 named substitutes,
// at an innings break, the end of an over or after a wicket.

#include "app/match_session.hpp"

namespace t20::ui {

// React component state in the JS
struct ImpactPanelState {
  bool open = false;
  PlayerId outId, inId;  // empty = not chosen
};

void drawImpactPanel(app::MatchSession& session, ImpactPanelState& state);

}  // namespace t20::ui
