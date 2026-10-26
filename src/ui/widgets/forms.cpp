#include "ui/widgets/forms.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

// A small square +/- button
bool stepButton(const char* id, const char* glyph, bool enabled) {
  const auto& th = currentTheme();
  const float s = 30.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(id);
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::InvisibleButton("##step", ImVec2(s, s));
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopID();
  const bool hovered = enabled && ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float a = enabled ? 1.0f : 0.4f;
  dl->AddRectFilled(p, ImVec2(p.x + s, p.y + s), col(hovered ? th.surface3 : th.surface2, a), 8.0f);
  dl->AddRect(p, ImVec2(p.x + s, p.y + s), col(hovered ? th.gold : th.lineStrong, a), 8.0f);
  drawIcon(dl, glyph, p.x + (s - 16) * 0.5f, p.y + (s - 16) * 0.5f, 16.0f, ColorRGBA(th.ink.r, th.ink.g, th.ink.b, a));
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked && enabled;
}

void stepperValue(const std::string& text, float minW) {
  const ImVec2 ts = textSize(FontRole::BodyStrong, text);
  const float w = std::max(minW, ts.x + 16);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(w, 30));
  drawText(ImGui::GetWindowDrawList(), FontRole::BodyStrong, ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (30 - ts.y) * 0.5f),
           col(currentTheme().ink), text);
}

}  // namespace

bool numberStepper(const char* id, int* value, int min, int max, int step, const char* text) {
  if (!value) return false;
  ImGui::PushID(id);
  bool changed = false;
  if (stepButton("-", "minus", *value > min)) {
    *value = std::max(min, *value - step);
    changed = true;
  }
  ImGui::SameLine(0, 4);
  stepperValue(text ? std::string(text) : std::to_string(*value), 44);
  ImGui::SameLine(0, 4);
  if (stepButton("+", "plus", *value < max)) {
    *value = std::min(max, *value + step);
    changed = true;
  }
  ImGui::PopID();
  return changed;
}

bool numberStepperD(const char* id, double* value, double min, double max, double step, const std::string& text) {
  if (!value) return false;
  ImGui::PushID(id);
  bool changed = false;
  if (stepButton("-", "minus", *value > min)) {
    *value = std::max(min, *value - step);
    changed = true;
  }
  ImGui::SameLine(0, 4);
  stepperValue(text, 80);
  ImGui::SameLine(0, 4);
  if (stepButton("+", "plus", *value < max)) {
    *value = std::min(max, *value + step);
    changed = true;
  }
  ImGui::PopID();
  return changed;
}

bool textInput(const char* id, const char* label, std::string* value, size_t maxLen, float width) {
  if (!value) return false;
  const auto& th = currentTheme();
  ImGui::PushID(id);
  ImGui::BeginGroup();
  if (label && *label) {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.muted), "%s", upper(label).c_str());
  }
  std::vector<char> buf(maxLen + 1, 0);
  std::copy_n(value->begin(), std::min(value->size(), maxLen), buf.begin());
  ImGui::SetNextItemWidth(width > 0 ? width : ImGui::GetContentRegionAvail().x);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 9));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, metrics().smallRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, vec(th.lineStrong));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, vec(th.surface2));
  const bool changed = ImGui::InputText("##text", buf.data(), buf.size());
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(3);
  if (changed) *value = buf.data();
  ImGui::EndGroup();
  ImGui::PopID();
  return changed;
}

bool sliderInt(const char* id, const char* label, int* value, int min, int max, float width) {
  if (!value) return false;
  const auto& th = currentTheme();
  ImGui::PushID(id);
  const float w = width > 0 ? width : ImGui::GetContentRegionAvail().x;
  const float labelW = label && *label ? std::min(160.0f, w * 0.4f) : 0.0f;
  const float numW = 36.0f;
  const float trackW = std::max(40.0f, w - labelW - numW - 12);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float h = 24.0f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (labelW > 0) {
    const ImVec2 ts = textSize(FontRole::Body, label);
    drawText(dl, FontRole::Body, ImVec2(p.x, p.y + (h - ts.y) * 0.5f), col(th.ink2), label);
  }
  const ImVec2 ta(p.x + labelW, p.y);
  ImGui::SetCursorScreenPos(ta);
  ImGui::InvisibleButton("##slider", ImVec2(trackW, h));
  bool changed = false;
  if (ImGui::IsItemActive() && max > min) {
    const float t = std::clamp((ImGui::GetIO().MousePos.x - ta.x) / trackW, 0.0f, 1.0f);
    const int v = min + static_cast<int>(std::lround(t * (max - min)));
    if (v != *value) {
      *value = v;
      changed = true;
    }
  }
  const bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
  const float frac = max > min ? static_cast<float>(*value - min) / static_cast<float>(max - min) : 0.0f;
  const float cy = ta.y + h * 0.5f;
  dl->AddRectFilled(ImVec2(ta.x, cy - 3), ImVec2(ta.x + trackW, cy + 3), col(th.surface3), 3);
  const ColorRGBA& fc = *value >= 70 ? th.green : *value >= 45 ? th.gold : th.red;
  dl->AddRectFilled(ImVec2(ta.x, cy - 3), ImVec2(ta.x + trackW * frac, cy + 3), col(fc), 3);
  dl->AddCircleFilled(ImVec2(ta.x + trackW * frac, cy), hovered ? 8.0f : 7.0f, col(th.ink));
  const std::string num = std::to_string(*value);
  const ImVec2 ns = textSize(FontRole::BodyStrong, num);
  drawText(dl, FontRole::BodyStrong, ImVec2(ta.x + trackW + 12 + numW - ns.x, p.y + (h - ns.y) * 0.5f), col(th.ink), num);
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(w, h));
  ImGui::PopID();
  return changed;
}

bool checkbox(const char* label, bool* value, bool enabled) {
  if (!value) return false;
  const auto& th = currentTheme();
  const std::string_view shown(label, static_cast<size_t>(ImGui::FindRenderedTextEnd(label) - label));
  const ImVec2 ts = textSize(FontRole::Body, shown);
  const float box = 18.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImVec2 size(box + (shown.empty() ? 0 : 8 + ts.x), std::max(box, ts.y));
  ImGui::PushID(label);
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::InvisibleButton("##cb", size);
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopID();
  if (clicked && enabled) *value = !*value;
  const bool hovered = enabled && ImGui::IsItemHovered();
  const float a = enabled ? 1.0f : 0.45f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 b0(p.x, p.y + (size.y - box) * 0.5f), b1(b0.x + box, b0.y + box);
  dl->AddRectFilled(b0, b1, *value ? col(th.gold, a) : col(th.surface2, a), 5.0f);
  dl->AddRect(b0, b1, col(*value ? th.gold : hovered ? th.gold : th.lineStrong, a), 5.0f);
  if (*value) {
    dl->AddLine(ImVec2(b0.x + 4, b0.y + 9), ImVec2(b0.x + 7.5f, b0.y + 13), col(th.goldInk), 2.0f);
    dl->AddLine(ImVec2(b0.x + 7.5f, b0.y + 13), ImVec2(b0.x + 14, b0.y + 5), col(th.goldInk), 2.0f);
  }
  if (!shown.empty()) drawText(dl, FontRole::Body, ImVec2(b1.x + 8, p.y + (size.y - ts.y) * 0.5f), col(th.ink2, a), shown);
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked && enabled;
}

bool teamCard(const TeamMeta& team, const std::string& sub, bool selected, float width, bool enabled) {
  const auto& th = currentTheme();
  const float h = 74.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(team.id.c_str());
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::InvisibleButton("##team", ImVec2(width, h));
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopID();
  const bool hovered = enabled && ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float a = enabled ? 1.0f : 0.4f;
  const ImVec2 b(p.x + width, p.y + h);
  dl->AddRectFilled(p, b, col(selected ? th.goldSoft : hovered ? th.surface2 : th.surface, a), metrics().panelRadius);
  // team colour wash on the left edge
  rectGradient(dl, p, ImVec2(p.x + width * 0.45f, b.y), hexCol(team.color, 0.22f * a), hexCol(team.color, 0.0f), metrics().panelRadius);
  dl->AddRect(p, b, col(selected ? th.gold : hovered ? th.lineStrong : th.line, a), metrics().panelRadius, 0, selected ? 2.0f : 1.0f);
  drawTeamBadge(dl, team, p.x + 14, p.y + (h - 40 * 1.1f) * 0.5f, 40.0f);
  const float tx = p.x + 66;
  const ImVec2 ns = textSize(FontRole::H3, team.name);
  drawText(dl, FontRole::H3, ImVec2(tx, p.y + 16), col(th.ink, a), team.name);
  if (!sub.empty()) drawText(dl, FontRole::Hint, ImVec2(tx, p.y + 18 + ns.y), col(th.muted, a), sub);
  if (selected) {
    const ImVec2 c(b.x - 18, p.y + 18);
    dl->AddCircleFilled(c, 9, col(th.gold));
    dl->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x - 1, c.y + 3), col(th.goldInk), 2.0f);
    dl->AddLine(ImVec2(c.x - 1, c.y + 3), ImVec2(c.x + 4, c.y - 3), col(th.goldInk), 2.0f);
  }
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked && enabled;
}

int columnsFor(float width, float minWidth, int maxCols) {
  return std::clamp(static_cast<int>(width / std::max(1.0f, minWidth)), 1, std::max(1, maxCols));
}

bool beginGrid(const char* id, int columns, float gap) {
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(gap * 0.5f, gap * 0.5f));
  const bool open = ImGui::BeginTable(id, std::max(1, columns), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX);
  if (!open) ImGui::PopStyleVar();
  return open;
}

void gridNext() {
  ImGui::TableNextColumn();
  // Tables align a cell's text with framed widgets in other cells of the row; grid cells are
  // independent blocks, so every cell starts at the top
  ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0;
}

void endGrid() {
  ImGui::EndTable();
  ImGui::PopStyleVar();
}

void keyValue(const std::string& label, const std::string& value, bool strongValue) {
  const auto& th = currentTheme();
  const float w = ImGui::GetContentRegionAvail().x;
  const float x0 = ImGui::GetCursorPosX();
  ImGui::TextColored(vec(th.muted), "%s", label.c_str());
  const FontRole fr = strongValue ? FontRole::BodyStrong : FontRole::Body;
  const float lw = textSize(FontRole::Body, label).x, vw = textSize(fr, value).x;
  ImGui::SameLine();
  ImGui::SetCursorPosX(x0 + std::max(lw + 10, w - vw));
  FontScope fs(fr);
  ImGui::TextUnformatted(value.c_str());
}

void progressBar(float fraction, const std::string& text, float width) {
  const auto& th = currentTheme();
  const float avail = ImGui::GetContentRegionAvail().x;
  const float tw = text.empty() ? 0 : textSize(FontRole::Hint, text).x + 10;
  const float w = (width > 0 ? width : avail) - tw;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float y = p.y + 5;
  dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + 6), col(th.surface3), 3);
  dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w * std::clamp(fraction, 0.0f, 1.0f), y + 6), col(th.gold), 3);
  if (!text.empty()) drawText(dl, FontRole::Hint, ImVec2(p.x + w + 10, p.y), col(th.muted), text);
  ImGui::Dummy(ImVec2(w + tw, 16));
}

void emptyState(const std::string& text) {
  const auto& th = currentTheme();
  const float w = ImGui::GetContentRegionAvail().x;
  const ImVec2 ts = textSize(FontRole::Body, text);
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - ts.x) * 0.5f));
  ImGui::TextColored(vec(th.muted), "%s", text.c_str());
  ImGui::Dummy(ImVec2(0, 10));
}

void iconText(const char* icon, const std::string& text) {
  const auto& th = currentTheme();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float lh = ImGui::GetTextLineHeight();
  drawIcon(ImGui::GetWindowDrawList(), icon, p.x, p.y + (lh - 16) * 0.5f, 16.0f, th.goldText);
  ImGui::SetCursorScreenPos(ImVec2(p.x + 22, p.y));
  ImGui::TextUnformatted(text.c_str());
}

}  // namespace t20::ui
