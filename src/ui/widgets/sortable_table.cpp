#include "ui/widgets/sortable_table.hpp"
#include "ui/theme.hpp"

namespace t20::ui {

bool beginSortableTable(const char* /*tableId*/,
                        const std::vector<TableColumn>& /*columns*/,
                        SortableTableState& /*state*/,
                        int /*totalRows*/) {
  // In ImGui backend:
  // ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY |
  //                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;
  // First column is `#` rank.
  // Active sorted column receives bold text.
  // User team rows receive is-mine gold-soft background highlight.
  return true;
}

void endSortableTable(const std::string& /*footerHint*/) {
  // If footerHint provided, render italic hint under table
}

} // namespace t20::ui
