#pragma once

#include "core/color.hpp"
#include <string>
#include <vector>
#include <functional>

namespace t20::ui {

enum class ButtonKind {
  Default,  // surface-2, line-strong border, gold border on hover
  Primary,  // gold gradient, gold-ink text, glow
  Quiet,    // transparent background
  Small,
  Danger
};

// Button with night-broadcast styling
bool button(const char* label, ButtonKind kind = ButtonKind::Default, bool enabled = true);
// The same, stretched to the available width (dock rail, sticky actions)
bool buttonFull(const char* label, ButtonKind kind = ButtonKind::Default, bool enabled = true);

// Square icon button (minimise, close, swap, arrows, edit)
bool iconButton(const char* iconName, const char* tooltip = nullptr, bool enabled = true);

// Tab item definition
struct TabItem {
  std::string id;
  std::string label;
  std::string icon; // optional icon name
};

// Inset tab strip (surface-2, radius 11, 4px padding; selected tab raised with shadow)
// Returns index of newly selected tab or current index
int tabs(const std::vector<TabItem>& items, int currentIdx, bool small = false);

// Pill radio button: selected = gold border + gold-soft background
bool chip(const char* label, bool selected);

// Inset segmented control grid
int segmented(const std::vector<std::string>& options, int currentIdx);

// Labelled dropdown (an HTML <select>): `label` above, `placeholder` while *index < 0.
// Returns true when the choice changes.
bool selectBox(const char* id, const char* label, const std::vector<std::string>& options, int* index,
               const char* placeholder = "Choose", float width = 0.0f);

// Switch toggle checkbox with gold accent
bool switchToggle(const char* label, bool* value);

enum class NoticeKind {
  Ok,    // green tint
  Warn,  // gold-soft tint
  Error  // red tint
};

// Notice banner
void notice(const std::string& text, NoticeKind kind = NoticeKind::Ok);

// In-place confirmation banner with confirm/cancel buttons: 1 = confirmed, -1 = cancelled, 0 = neither
int confirmInline(const char* message, const char* confirmLabel = "Confirm", const char* cancelLabel = "Cancel");

// The action bar at the end of a screen (its forward / back buttons) on a raised strip
void beginStickyActions();
void endStickyActions();

// Move the cursor so an item `width` wide ends at the right edge; returns the width there was
float rightAlign(float width);
// How wide button(label, kind) will be (for right-aligning a row of buttons)
float buttonWidth(const char* label, ButtonKind kind = ButtonKind::Default);

} // namespace t20::ui
