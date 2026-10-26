#pragma once
// S05 §9 Commentary (Commentary.jsx): the ball-by-ball feed, newest first. Off ("Commentary is
// off..." + Turn on), collapsed (the latest line) or open: key-moments-only switch, Turn off,
// innings tabs (Super Overs marked SO), badges per line, the latest 20 with Show 20 more / Show all
// / Show fewer.

#include "engine/match_types.hpp"

namespace t20::ui {

struct CommentaryState {
  bool on = true;
  bool open = true;
  bool keyOnly = false;
  int innings = -1;  // -1 = follow the current innings
  int shown = 20;
};

void drawCommentaryPanel(const Match& m, CommentaryState& state);

}  // namespace t20::ui
