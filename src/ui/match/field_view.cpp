#include "ui/match/field_view.hpp"
#include "ui/theme.hpp"
#include "ui/anim.hpp"
#include <cmath>

namespace t20::ui {

void drawFieldView(void* /*drawList*/, const FieldViewData& data, float canvasSize) {
  // Virtual coordinate system: 420x420, center at (210, 210)
  float scale = canvasSize / 420.0f;
  float cx = 210.0f * scale;
  float cy = 210.0f * scale;

  // In ImGui backend:
  // 1. Outfield radial gradient (dark stadium grass) with alternating mowing concentric rings
  // 2. Boundary rope circle: radius 168.0f * scale
  // 3. 30-yard inner circle: dashed circle radius 86.0f * scale
  // 4. Central pitch rectangle
  // 5. OFF and LEG labels (mirrored horizontally if data.isLeftHander)
  // 6. Draw fielders: gold-ringed dots at data.fielders positions
  // 7. Draw shots from striker's crease:
  //    - Four/dot/single: lines to target (targetX, targetY)
  //    - Six: quadratic bezier arc with gold glow
  //    - Wicket: red cross 'X' at target
  //    - Saved: dashed line with green ring
  //    - Newest shot animated via sampleTween("shot_draw", 1.0f)

  (void)cx;
  (void)cy;
  (void)scale;
  (void)data;
}

} // namespace t20::ui
