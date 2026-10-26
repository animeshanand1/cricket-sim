#pragma once

#include "core/types.hpp"
#include <string>
#include <functional>

namespace t20::ui {

// Clickable player link styled button with hover underline
// Returns true when clicked
bool playerLink(const std::string& playerId, const std::string& displayName);

// Rating component: right-aligned bold number
// >= 80: green, 60-79: ink, < 60: muted
void rating(int value, const char* tooltip = nullptr);

// Meter component: 7px track (surface-3), fill gradient (>= 70 green, >= 45 gold, else red)
void meter(float value, const char* label = nullptr, bool compact = false, const char* toneOverride = nullptr);

} // namespace t20::ui
