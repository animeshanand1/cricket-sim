#pragma once

#include "core/types.hpp"
#include <string>
#include <functional>

namespace t20::ui {

// Begin a raised panel container (surface, 14px radius, 1px border, 18px padding)
bool beginPanel(const char* id, const char* title = nullptr);
void endPanel();

// Page header banner with optional team identity, eyebrow, title, and right actions
void pageHeader(const std::string& title,
                const std::string& subtitle = "",
                const std::string& eyebrow = "",
                const TeamMeta* team = nullptr,
                const std::function<void()>& rightActions = nullptr);

// Collapsed panel: one-line summary bar with chevron and "Show" button
// Returns true when expanded
bool collapsedPanel(const char* title, const char* summary, bool* isExpanded);

// Collapsible fold row with chevron toggle; remembers its state for the session.
// When it returns true, draw the body and then call endFoldRow().
bool foldRow(const char* id, const char* title, const char* summary, bool defaultOpen = false);
void endFoldRow();

// A full-width clickable row holding plain content (avatar, text, ratings): the bowler and
// batter pickers. Returns true from endPickRow() when clicked. Rows must not contain other
// interactive widgets.
void beginPickRow(const char* id, bool enabled = true);
bool endPickRow();

} // namespace t20::ui
