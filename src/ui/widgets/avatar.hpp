#pragma once

#include "core/types.hpp"
#include "core/color.hpp"

namespace t20::ui {

void drawAvatar(void* drawList, const PlayerMeta& player, const std::string& teamColor, float posX, float posY, float size = 36.0f, bool showRole = true);

void avatar(const PlayerMeta& player, const std::string& teamColor = "#4a6a8a", float size = 36.0f, bool showRole = true);

} // namespace t20::ui
