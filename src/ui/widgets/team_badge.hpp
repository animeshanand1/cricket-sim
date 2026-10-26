#pragma once

#include "core/types.hpp"
#include "core/color.hpp"

namespace t20::ui {

enum class BadgeSize {
  xs = 18,
  sm = 24,
  md = 40,
  lg = 56,
  xl = 84
};

// Draw team shield badge with diagonal team gradient, highlight chevron and team code
void drawTeamBadge(void* drawList, const TeamMeta& team, float posX, float posY, float size = 40.0f);

// ImGui widget for TeamBadge (reserves space and renders badge)
void teamBadge(const TeamMeta& team, float size = 40.0f);
inline void crest(const TeamMeta& team, float size = 40.0f) { teamBadge(team, size); }

} // namespace t20::ui
