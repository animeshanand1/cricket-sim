#pragma once
// S05 §8 Field view (FieldView.jsx): where the runs went. A 420×420 virtual canvas scaled to the
// panel: outfield, rope, inner circle, pitch, OFF / LEG (mirrored for a left-hander), shot lines
// from the striker's end, sixes as arcs, wickets as red crosses, saved shots dashed to the fielder
// with a green ring, and the fielders as gold-ringed dots. The newest shot draws itself.

#include <optional>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace t20::ui {

// Angles are degrees from straight, positive to the right of the canvas (already mirrored for a
// left-hander); distances are a share of the way from the striker's end to the rope
struct FieldShot {
  float dir = 0.0f, dist = 0.0f;
  int runs = 0;
  bool isFour = false, isSix = false, isWicket = false;
  int saved = 0;                       // runs the shot was worth before a fielder cut it off
  std::optional<float> fielderDir, fielderDist;  // where it was cut off
  bool isLatest = false;
  std::string tip;                     // hover text
};

struct FielderPos {
  float dir = 0.0f, dist = 0.0f;
  std::string label;  // "Deep point: Name"
};

struct FieldViewData {
  bool isLeftHander = false;  // OFF / LEG labels swap
  std::vector<FielderPos> fielders;
  std::vector<FieldShot> shots;
};

// The canvas as an ImGui item (hover tooltips on fielders and saved shots); size in pixels
void fieldViewCanvas(const FieldViewData& data, float size);
// The same, drawn at a position without taking part in the layout
void drawFieldView(void* drawList, const FieldViewData& data, float canvasSize = 420.0f);

// The panel on the Match screen: mode tabs, the batter for the wagon wheel, the fielders switch,
// the canvas, the legend and the stats strip; collapsible
struct FieldViewState {
  int mode = 0;          // This over | Last 5 overs | Innings | Wagon wheel
  PlayerId batter;       // wagon wheel (empty = the striker)
  bool showFielders = true;
  bool open = true;
  size_t lastBalls = 0;  // to animate a new shot
};
void drawFieldViewPanel(const Match& m, FieldViewState& state);

}  // namespace t20::ui
