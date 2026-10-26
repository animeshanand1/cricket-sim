#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace t20::ui {

struct TableColumn {
  std::string key;
  std::string label;
  std::string tooltip;
  bool isNumeric = false;
  bool lowIsGood = false; // if true, first sort is ascending (e.g. bowling economy/average)
  std::string qualificationHint; // e.g. "min 60 balls faced"
};

// Generic sortable table metadata
struct SortableTableState {
  int sortColumnIdx = 0;
  bool sortAscending = false;
  bool showTop10Only = true;
};

// Begin sortable table
bool beginSortableTable(const char* tableId,
                        const std::vector<TableColumn>& columns,
                        SortableTableState& state,
                        int totalRows);

void endSortableTable(const std::string& footerHint = "");

} // namespace t20::ui
