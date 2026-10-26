#pragma once
// S05 §5 Captain panel (CaptainPanel.jsx): innings break, result, and while live the captain's
// calls: DRS review, bowler picker, batter picker, batter tempo and target, bowling plan, field
// (presets and the custom editor), change bowler.

#include <functional>
#include <string>
#include <vector>

#include "app/match_session.hpp"

namespace t20::ui {

// What the panel keeps between frames (React component state in the JS)
struct CaptainPanelState {
  bool editingField = false;
  std::vector<Pos> fieldDraft;
  bool scrollToDecision = false;  // set by the dock's "Pick your bowler" button
};

// A way out of a finished match ("Rematch", "Back to season"); the first is the primary button
struct MatchExit {
  std::string label;
  std::function<void()> action;
};

void drawCaptainPanel(app::MatchSession& session, CaptainPanelState& state, const std::vector<MatchExit>& exits);

// The dock's prompt while a decision is pending ("Pick your bowler", ...), or nullptr
const char* pendingDecisionLabel(const Match& m);

}  // namespace t20::ui
