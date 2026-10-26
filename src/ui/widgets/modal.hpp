#pragma once

#include <string>
#include <functional>

namespace t20::ui {

// Universal modal dialog host:
// Centered over dimmed backdrop rgba(3,7,12,0.72)
// Smooth pop-in animation (scale 0.98 -> 1.0, 12px lower offset, 0.22s)
// Max width 720px, radius 18px
// Esc key to close
bool beginModal(const char* modalId, bool* isOpen, const char* title = nullptr, float maxWidth = 720.0f);
void endModal();

} // namespace t20::ui
