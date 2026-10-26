#pragma once

#include "core/color.hpp"
#include <string>

namespace t20::ui {

// Look up SVG path definition by icon name
const char* getIconSvgPath(const char* name);

// Draw an icon using ImDrawList vector commands or cached texture atlas
void drawIcon(void* drawList, const char* name, float posX, float posY, float size, ColorRGBA color);

// ImGui widget helper: reserves space and draws icon inline with text / buttons
bool iconWidget(const char* name, float size = 18.0f, ColorRGBA color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), const char* tooltip = nullptr);

} // namespace t20::ui
