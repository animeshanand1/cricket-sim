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

// In-place confirmation banner with confirm/cancel buttons
bool confirmInline(const char* message, const char* confirmLabel = "Confirm", const char* cancelLabel = "Cancel");

// Sticky bottom action bar container for main forward/back CTAs
void beginStickyActions();
void endStickyActions();

} // namespace t20::ui
