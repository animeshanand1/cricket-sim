#include "ui/widgets/panel.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/widgets/team_badge.hpp"
#include "ui/icons.hpp"

#include <imgui_internal.h>

namespace t20::ui {

bool beginPanel(const char* id, const char* title) {
  // surface, radius 14, 1px line border, 18px padding, soft shadow (drawn by endPanel)
  const auto& th = currentTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(th.surface));
  ImGui::PushStyleColor(ImGuiCol_Border, vec(th.line));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, metrics().panelRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 18));
  ImGui::BeginChild(id, ImVec2(ImGui::GetContentRegionAvail().x, 0),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (title && *title) {
    FontScope fs(FontRole::PanelTitle);
    ImGui::TextUnformatted(upper(title).c_str());
    ImGui::Dummy(ImVec2(0, 2));
  }
  return true;
}

void endPanel() {
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
  // The parent's draw list is rendered before the child, so the shadow lands under the panel
  const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
  drawPanelShadow(ImGui::GetWindowDrawList(), a.x, a.y, b.x, b.y, metrics().panelRadius);
}

void pageHeader(const std::string& title,
                const std::string& subtitle,
                const std::string& eyebrow,
                const TeamMeta* team,
                const std::function<void()>& rightActions) {
  const auto& th = currentTheme();
  if (team) {
    teamBadge(*team, 56.0f);
    ImGui::SameLine(0, 16);
  }
  ImGui::BeginGroup();
  if (!eyebrow.empty()) {
    FontScope fs(FontRole::Eyebrow);
    ImGui::PushStyleColor(ImGuiCol_Text, vec(th.goldText));
    ImGui::TextUnformatted(upper(eyebrow).c_str());
    ImGui::PopStyleColor();
  }
  {
    FontScope fs(FontRole::H1);
    ImGui::TextUnformatted(title.c_str());
  }
  if (!subtitle.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, vec(th.muted));
    ImGui::TextUnformatted(subtitle.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::EndGroup();
  if (rightActions) {
    ImGui::SameLine();
    rightActions();
  }
}

bool collapsedPanel(const char* title, const char* summary, bool* isExpanded) {
  // Closed: a one-line bar (title, muted summary, "Show"). Open: the caller draws the panel and
  // can call collapsedPanel again with the same flag to offer "Hide".
  if (!isExpanded) return true;
  if (*isExpanded) return true;
  const auto& th = currentTheme();
  const float w = ImGui::GetContentRegionAvail().x, h = 46.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(title);
  if (ImGui::InvisibleButton("##collapsed", ImVec2(w, h))) *isExpanded = true;
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(hovered ? th.surface2 : th.surface), metrics().panelRadius);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), col(hovered ? th.lineStrong : th.line), metrics().panelRadius);
  const std::string t = upper(title);
  const ImVec2 ts = textSize(FontRole::PanelTitle, t);
  drawText(dl, FontRole::PanelTitle, ImVec2(p.x + 18, p.y + (h - ts.y) * 0.5f), col(th.ink), t);
  if (summary && *summary) {
    const ImVec2 ss = textSize(FontRole::Hint, summary);
    drawText(dl, FontRole::Hint, ImVec2(p.x + 32 + ts.x, p.y + (h - ss.y) * 0.5f), col(th.muted), summary);
  }
  const ImVec2 bs = textSize(FontRole::BodyStrong, "Show");
  drawText(dl, FontRole::BodyStrong, ImVec2(p.x + w - 18 - bs.x, p.y + (h - bs.y) * 0.5f), col(th.goldText), "Show");
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return false;
}

bool foldRow(const char* id, const char* title, const char* summary, bool defaultOpen) {
  // A bordered bar: chevron · title · muted summary. Open state lives in ImGui's storage.
  const auto& th = currentTheme();
  ImGui::PushID(id);
  ImGuiStorage* st = ImGui::GetStateStorage();
  const ImGuiID key = ImGui::GetID("##open");
  bool open = st->GetBool(key, defaultOpen);
  const float w = ImGui::GetContentRegionAvail().x, h = 38.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  if (ImGui::InvisibleButton("##fold", ImVec2(w, h))) {
    open = !open;
    st->SetBool(key, open);
  }
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(hovered ? th.surface3 : th.surface2), metrics().smallRadius);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), col(th.line), metrics().smallRadius);
  // chevron: right when closed, down when open
  const float cx = p.x + 16, cy = p.y + h * 0.5f;
  if (open)
    dl->AddTriangleFilled(ImVec2(cx - 4, cy - 2), ImVec2(cx + 4, cy - 2), ImVec2(cx, cy + 3), col(th.muted));
  else
    dl->AddTriangleFilled(ImVec2(cx - 2, cy - 4), ImVec2(cx + 3, cy), ImVec2(cx - 2, cy + 4), col(th.muted));
  const ImVec2 ts = textSize(FontRole::BodyStrong, title);
  drawText(dl, FontRole::BodyStrong, ImVec2(p.x + 30, cy - ts.y * 0.5f), col(th.ink), title);
  if (summary && *summary) {
    const ImVec2 ss = textSize(FontRole::Hint, summary);
    drawText(dl, FontRole::Hint, ImVec2(p.x + w - 14 - ss.x, cy - ss.y * 0.5f), col(th.muted), summary);
  }
  if (open) {
    ImGui::Indent(12.0f);
    ImGui::Dummy(ImVec2(0, 2));
  }
  return open;
}

void endFoldRow() {
  ImGui::Unindent(12.0f);
  ImGui::Dummy(ImVec2(0, 2));
}

namespace {
struct PickRowState {
  ImVec2 start;
  float width = 0;
  bool enabled = true;
  ImGuiID id = 0;
};
PickRowState s_row;
constexpr float kRowPad = 10.0f;
}  // namespace

void beginPickRow(const char* id, bool enabled) {
  s_row.start = ImGui::GetCursorScreenPos();
  s_row.width = ImGui::GetContentRegionAvail().x;
  s_row.enabled = enabled;
  s_row.id = ImGui::GetID(id);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->ChannelsSplit(2);
  dl->ChannelsSetCurrent(1);
  ImGui::SetCursorScreenPos(ImVec2(s_row.start.x + kRowPad, s_row.start.y + kRowPad));
  ImGui::BeginGroup();
  if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
}

bool endPickRow() {
  const auto& th = currentTheme();
  if (!s_row.enabled) ImGui::PopStyleVar();
  ImGui::EndGroup();
  const float bottom = ImGui::GetItemRectMax().y + kRowPad;
  const ImRect bb(s_row.start, ImVec2(s_row.start.x + s_row.width, bottom));
  ImGui::SetCursorScreenPos(bb.Min);
  ImGui::ItemSize(bb.GetSize());
  bool hovered = false, held = false, clicked = false;
  if (ImGui::ItemAdd(bb, s_row.id)) {
    clicked = ImGui::ButtonBehavior(bb, s_row.id, &hovered, &held) && s_row.enabled;
  }
  if (!s_row.enabled) hovered = held = false;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->ChannelsSetCurrent(0);
  dl->AddRectFilled(bb.Min, bb.Max, col(held ? th.surface3 : hovered ? th.surface3 : th.surface2, s_row.enabled ? 1.0f : 0.5f),
                    metrics().smallRadius);
  dl->AddRect(bb.Min, bb.Max, col(hovered ? th.gold : th.line), metrics().smallRadius);
  dl->ChannelsMerge();
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked;
}

} // namespace t20::ui
