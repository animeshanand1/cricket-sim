#include "ui/icons.hpp"
#include <unordered_map>
#include <cstring>

namespace t20::ui {

// Exact 34 SVG path strings from ui.jsx (24px viewBox grid, 2px stroke, round caps/joins)
static const std::unordered_map<std::string, const char*> s_iconPaths = {
  {"home",     "M3 10.5 12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z"},
  {"trophy",   "M8 21h8M12 17v4M7 4h10v5a5 5 0 0 1-10 0zM17 5h3v2a3 3 0 0 1-3 3M7 5H4v2a3 3 0 0 0 3 3"},
  {"users",    "M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M9 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM22 21v-2a4 4 0 0 0-3-3.87M16 3.13a4 4 0 0 1 0 7.75"},
  {"chart",    "M3 3v18h18M7 15l4-4 3 3 5-6"},
  {"bat",      "M14.2 3.3l6.5 6.5-8.6 8.6a2.2 2.2 0 0 1-3.1 0l-3.4-3.4a2.2 2.2 0 0 1 0-3.1zM5.6 18.4 2.8 21.2"},
  {"ball",     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM6.5 5.5c2 2.5 2 10.5 0 13M17.5 5.5c-2 2.5-2 10.5 0 13"},
  {"stumps",   "M7 21V6M12 21V6M17 21V6M6 5h5M13 5h5"},
  {"play",     "M7 4l13 8-13 8z"},
  {"pause",    "M7 4h3v16H7zM14 4h3v16h-3z"},
  {"eye",      "M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12zM12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z"},
  {"forward",  "M4 5l8 7-8 7zM12 5l8 7-8 7z"},
  {"skip",     "M5 5l10 7-10 7zM19 5v14"},
  {"swap",     "M7 7h13l-4-4M17 17H4l4 4"},
  {"grip",     "M9 5h.01M9 12h.01M9 19h.01M15 5h.01M15 12h.01M15 19h.01"},
  {"dumbbell", "M6 6v12M18 6v12M3 9v6M21 9v6M6 12h12"},
  {"coin",     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 7v10M9 9.5h4.5a1.8 1.8 0 0 1 0 3.5H10a1.8 1.8 0 0 0 0 3.5H15"},
  {"calendar", "M4 5h16v16H4zM4 10h16M9 3v4M15 3v4"},
  {"list",     "M9 6h12M9 12h12M9 18h12M4 6h.01M4 12h.01M4 18h.01"},
  {"star",     "M12 3l2.8 5.7 6.2.9-4.5 4.4 1 6.2-5.5-2.9-5.5 2.9 1-6.2L3 9.6l6.2-.9z"},
  {"x",        "M6 6l12 12M18 6 6 18"},
  {"minus",    "M5 12h14"},
  {"edit",     "M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4"},
  {"check",    "M5 12.5l4.5 4.5L19 7"},
  {"undo",     "M9 14 4 9l5-5M4 9h11a5 5 0 0 1 0 10h-3"},
  {"chevron",  "M9 6l6 6-6 6"},
  {"back",     "M15 6l-6 6 6 6"},
  {"droplet",  "M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z"},
  {"target",   "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM12 12h.01"},
  {"shield",   "M12 2l8 3v6c0 5-3.5 9-8 11-4.5-2-8-6-8-11V5z"},
  {"pitch",    "M9 2h6v20H9zM9 6h6M9 18h6"},
  {"flame",    "M12 22a7 7 0 0 0 7-7c0-4-3-6-4-9-1.5 2-2.5 3-3 5-1-1-1.5-2-2-3-2 2.5-5 5.5-5 7a7 7 0 0 0 7 7z"},
  {"bolt",     "M13 2 4 14h7l-1 8 9-12h-7z"},
  {"field",    "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM11 10h2v4h-2z"},
  {"whistle",  "M3 11a5 5 0 0 0 10 1h8V8h-9.5A5 5 0 0 0 3 11zM8 11h.01"}
};

const char* getIconSvgPath(const char* name) {
  if (!name) return nullptr;
  auto it = s_iconPaths.find(name);
  if (it != s_iconPaths.end()) {
    return it->second;
  }
  return nullptr;
}

void drawIcon(void* /*drawList*/, const char* /*name*/, float /*posX*/, float /*posY*/, float /*size*/, ColorRGBA /*color*/) {
  // Rasterised atlas or ImDrawList path commands
}

bool iconWidget(const char* /*name*/, float /*size*/, ColorRGBA /*color*/, const char* /*tooltip*/) {
  // ImGui button or image widget
  return false;
}

} // namespace t20::ui
