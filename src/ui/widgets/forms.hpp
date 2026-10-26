#pragma once
// Form and layout widgets for the management screens: number steppers, text input, sliders,
// checkboxes, team tiles, a column grid, key/value rows, progress bars and empty states.

#include <string>

#include "core/types.hpp"

namespace t20::ui {

// [-] value [+]. `text` replaces the number when given (e.g. "3 years"). Returns true on change.
bool numberStepper(const char* id, int* value, int min, int max, int step = 1, const char* text = nullptr);
// The same over doubles (money in lakh)
bool numberStepperD(const char* id, double* value, double min, double max, double step, const std::string& text);

// Labelled single-line text input; returns true on change
bool textInput(const char* id, const char* label, std::string* value, size_t maxLen = 64, float width = 0.0f);

// Gold slider with the value on the right; returns true on change
bool sliderInt(const char* id, const char* label, int* value, int min, int max, float width = 0.0f);

// Square checkbox with gold tick; returns true on change
bool checkbox(const char* label, bool* value, bool enabled = true);

// A selectable team tile: badge, name, a muted line under it. Selected = gold border.
bool teamCard(const TeamMeta& team, const std::string& sub, bool selected, float width, bool enabled = true);

// Grid of equal columns (cards, panels). Call gridNext() before each cell.
// columnsFor(): how many columns of at least minWidth fit (1..maxCols).
int columnsFor(float width, float minWidth, int maxCols);
bool beginGrid(const char* id, int columns, float gap = 14.0f);
void gridNext();
void endGrid();

// "Label ............ value" on one line (label muted)
void keyValue(const std::string& label, const std::string& value, bool strongValue = true);

// A thin progress bar with optional text on the right
void progressBar(float fraction, const std::string& text = "", float width = 0.0f);

// Muted centred message for empty lists
void emptyState(const std::string& text);

// Icon + text on one line
void iconText(const char* icon, const std::string& text);

}  // namespace t20::ui
