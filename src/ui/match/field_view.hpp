#pragma once

#include "core/types.hpp"
#include <vector>

namespace t20::ui {

struct FieldShot {
  float targetX = 0.0f;
  float targetY = 0.0f;
  int runs = 0;
  bool isSix = false;
  bool isWicket = false;
  bool isSaved = false;
  bool isLatest = false;
};

struct FielderPos {
  float x = 0.0f;
  float y = 0.0f;
  std::string label;
};

struct FieldViewData {
  bool isLeftHander = false;
  std::vector<FielderPos> fielders;
  std::vector<FieldShot> recentShots;
};

// Draw 420x420 virtual field canvas scaled to panel
void drawFieldView(void* drawList, const FieldViewData& data, float canvasSize = 420.0f);

} // namespace t20::ui
