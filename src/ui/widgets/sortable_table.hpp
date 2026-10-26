#pragma once
// Sortable stats table (the web game's SortTable): click a header to sort by it, click again to
// flip. Columns where low is good (economy, average) sort ascending first. A `#` rank column comes
// first, the user's rows are tinted gold, and long tables show their top rows with "Show all".

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace t20::ui {

struct TableColumn {
  std::string key;
  std::string label;
  std::string tooltip;
  bool isNumeric = false;
  bool lowIsGood = false;  // first sort is ascending (economy, average)
  std::string qualificationHint;  // e.g. "min 60 balls faced", shown under the table when sorted by it
  float weight = 1.0f;            // share of the width
  bool sortable = true;
};

enum class CellTone { Normal, Muted, Good, Bad, Gold, Strong };

struct TableCell {
  std::string text;
  double sort = std::nan("");  // numeric sort key; NaN sorts by text (and missing values go last)
  CellTone tone = CellTone::Normal;
  std::string link;            // a player id: the cell is a link that opens the player card
  std::string badgeTeam;       // a team id: a small team badge before the text
};

struct TableRowData {
  std::vector<TableCell> cells;
  bool mine = false;   // the user's team: gold tint
  bool dim = false;    // out of contention, injured, ...
  std::string id;      // returned when the row is clicked
};

struct SortableTableState {
  int sortColumnIdx = 0;
  bool sortAscending = false;
  bool showTop10Only = true;
  bool initialised = false;
};

struct TableClick {
  int row = -1;          // index into the rows passed in
  std::string rowId;
  std::string link;      // a link cell was clicked
  explicit operator bool() const { return row >= 0 || !link.empty(); }
};

// Column and cell shorthands
inline TableColumn textColumn(const std::string& label, float weight = 1.0f, bool sortable = true) {
  TableColumn c;
  c.key = c.label = label;
  c.weight = weight;
  c.sortable = sortable;
  return c;
}
inline TableColumn numColumn(const std::string& label, const std::string& tooltip = "", bool lowIsGood = false,
                             const std::string& qualification = "") {
  TableColumn c;
  c.key = c.label = label;
  c.tooltip = tooltip;
  c.isNumeric = true;
  c.lowIsGood = lowIsGood;
  c.qualificationHint = qualification;
  return c;
}
inline TableCell textCell(const std::string& text, CellTone tone = CellTone::Normal) {
  TableCell c;
  c.text = text;
  c.tone = tone;
  return c;
}
inline TableCell dashCell() { return textCell("\xe2\x80\x93"); }

// Draws the table. topN > 0 shows only that many rows until "Show all" is clicked.
// rank: draw the "#" column (position after sorting).
TableClick sortableTable(const char* tableId, const std::vector<TableColumn>& columns, const std::vector<TableRowData>& rows,
                         SortableTableState& state, int topN = 0, bool rank = true, const std::string& footerHint = "");

}  // namespace t20::ui
