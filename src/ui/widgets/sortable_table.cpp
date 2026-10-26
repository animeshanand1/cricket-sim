#include "ui/widgets/sortable_table.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <numeric>

#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/view_data.hpp"

namespace t20::ui {

namespace {

ImVec4 toneColor(CellTone t) {
  const auto& th = currentTheme();
  switch (t) {
    case CellTone::Muted: return vec(th.muted);
    case CellTone::Good: return vec(th.green);
    case CellTone::Bad: return vec(th.red);
    case CellTone::Gold: return vec(th.goldText);
    default: return vec(th.ink);
  }
}

}  // namespace

TableClick sortableTable(const char* tableId, const std::vector<TableColumn>& columns, const std::vector<TableRowData>& rows,
                         SortableTableState& state, int topN, bool rank, const std::string& footerHint) {
  const auto& th = currentTheme();
  TableClick click;
  const int nCols = static_cast<int>(columns.size());
  if (nCols == 0) return click;
  if (state.sortColumnIdx < 0 || state.sortColumnIdx >= nCols) state.sortColumnIdx = 0;

  // Sort order (stable, so ties keep the caller's order)
  std::vector<int> order(rows.size());
  std::iota(order.begin(), order.end(), 0);
  const int sc = state.sortColumnIdx;
  if (columns[static_cast<size_t>(sc)].sortable) {
    const bool asc = state.sortAscending;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
      const auto& ca = rows[static_cast<size_t>(a)].cells;
      const auto& cb = rows[static_cast<size_t>(b)].cells;
      if (static_cast<size_t>(sc) >= ca.size() || static_cast<size_t>(sc) >= cb.size()) return false;
      const TableCell& x = ca[static_cast<size_t>(sc)];
      const TableCell& y = cb[static_cast<size_t>(sc)];
      const bool xn = !std::isnan(x.sort), yn = !std::isnan(y.sort);
      if (xn && yn) {
        if (x.sort == y.sort) return false;
        return asc ? x.sort < y.sort : x.sort > y.sort;
      }
      if (xn != yn) return xn;  // missing numbers last, whichever way
      if (columns[static_cast<size_t>(sc)].isNumeric) return false;
      const int c = x.text.compare(y.text);
      return asc ? c < 0 : c > 0;
    });
  }

  const int shown = topN > 0 && state.showTop10Only ? std::min<int>(topN, static_cast<int>(order.size()))
                                                    : static_cast<int>(order.size());

  ImGui::PushID(tableId);
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, vec(ColorRGBA(th.surface2.r, th.surface2.g, th.surface2.b, 0.55f)));
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight, vec(th.line));
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 6));
  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("##t", nCols + (rank ? 1 : 0), flags)) {
    if (rank) ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26.0f);
    for (const auto& c : columns)
      ImGui::TableSetupColumn(c.label.c_str(), ImGuiTableColumnFlags_WidthStretch, c.isNumeric ? 0.8f * c.weight : 2.0f * c.weight);

    // Header row: our own, so the sort arrow and the first-click direction follow the web game
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    if (rank) {
      ImGui::TableNextColumn();
      FontScope fs(FontRole::Eyebrow);
      ImGui::TextColored(vec(th.muted), "#");
    }
    for (int i = 0; i < nCols; ++i) {
      ImGui::TableNextColumn();
      const TableColumn& c = columns[static_cast<size_t>(i)];
      const bool active = i == sc;
      std::string label = upper(c.label);
      if (active && c.sortable) label += state.sortAscending ? " \xe2\x96\xb2" : " \xe2\x96\xbc";
      FontScope fs(FontRole::Eyebrow);
      ImGui::PushID(i);
      ImGui::PushStyleColor(ImGuiCol_Text, vec(active ? th.goldText : th.muted));
      const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
      if (c.isNumeric) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - ts.x));
      if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_None, ts) && c.sortable) {
        if (active) {
          state.sortAscending = !state.sortAscending;
        } else {
          state.sortColumnIdx = i;
          state.sortAscending = c.isNumeric ? c.lowIsGood : true;
        }
      }
      ImGui::PopStyleColor();
      if (!c.tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.tooltip.c_str());
      ImGui::PopID();
    }

    for (int r = 0; r < shown; ++r) {
      const int ri = order[static_cast<size_t>(r)];
      const TableRowData& row = rows[static_cast<size_t>(ri)];
      ImGui::TableNextRow(ImGuiTableRowFlags_None, 30.0f);
      if (row.mine) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, col(th.goldSoft));
      ImGui::PushID(ri);
      if (row.dim) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
      if (rank) {
        ImGui::TableNextColumn();
        ImGui::TextColored(vec(th.muted), "%d", r + 1);
      }
      for (int i = 0; i < nCols && i < static_cast<int>(row.cells.size()); ++i) {
        ImGui::TableNextColumn();
        const TableCell& cell = row.cells[static_cast<size_t>(i)];
        const TableColumn& c = columns[static_cast<size_t>(i)];
        if (!cell.badgeTeam.empty()) {
          teamInline(cell.badgeTeam, false, 16.0f);
          ImGui::SameLine(0, 6);
        }
        const FontRole fr = cell.tone == CellTone::Strong || (i == sc && c.isNumeric) ? FontRole::BodyStrong : FontRole::Body;
        if (!cell.link.empty()) {
          ImGui::PushID(i);
          if (playerLink(cell.link, cell.text)) click.link = cell.link;
          ImGui::PopID();
          continue;
        }
        FontScope fs(fr);
        const ImVec2 ts = ImGui::CalcTextSize(cell.text.c_str());
        if (c.isNumeric) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - ts.x));
        ImGui::TextColored(toneColor(cell.tone), "%s", cell.text.c_str());
      }
      if (row.dim) ImGui::PopStyleVar();
      ImGui::PopID();
      if (!row.id.empty() && click.link.empty() && ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex() &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        click.row = ri;
        click.rowId = row.id;
      }
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);

  // Footer: show all / top N, the sorted column's qualification, the caller's hint
  if (topN > 0 && static_cast<int>(rows.size()) > topN) {
    const std::string label = state.showTop10Only ? "Show all " + std::to_string(rows.size()) : "Show top " + std::to_string(topN);
    FontScope fs(FontRole::BodyStrong);
    ImGui::PushStyleColor(ImGuiCol_Text, vec(th.goldText));
    if (ImGui::Selectable(label.c_str(), false, 0, ImGui::CalcTextSize(label.c_str()))) state.showTop10Only = !state.showTop10Only;
    ImGui::PopStyleColor();
  }
  const std::string& q = columns[static_cast<size_t>(sc)].qualificationHint;
  const std::string hint = q.empty() ? footerHint : footerHint.empty() ? q : q + " · " + footerHint;
  if (!hint.empty()) {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "%s", hint.c_str());
  }
  ImGui::PopID();
  return click;
}

}  // namespace t20::ui
