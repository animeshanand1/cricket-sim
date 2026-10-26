#include "ui/widgets/modal.hpp"
#include "ui/theme.hpp"
#include "ui/anim.hpp"

namespace t20::ui {

bool beginModal(const char* modalId, bool* isOpen, const char* /*title*/, float /*maxWidth*/) {
  if (!isOpen || !*isOpen) return false;

  // In ImGui backend:
  // Render full-screen backdrop with rgba(3,7,12,0.72)
  // Animate modal offset and scale (sampleTween)
  // Check for Esc key press to close modal
  (void)modalId;
  return true;
}

void endModal() {
  // End modal window
}

} // namespace t20::ui
