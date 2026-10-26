#include "ui/widgets/controls.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"

#include <imgui_internal.h>

namespace t20::ui {

namespace {

bool buttonImpl(const char* label, ButtonKind kind, bool enabled, bool fullWidth) {
  const auto& th = currentTheme();
  const auto& met = metrics();
  const bool small = kind == ButtonKind::Small;
  const FontRole fr = FontRole::BodyStrong;
  const char* labelEnd = ImGui::FindRenderedTextEnd(label);
  const ImVec2 ts = textSize(fr, std::string_view(label, static_cast<size_t>(labelEnd - label)));
  const float h = small ? met.buttonSmallHeight : met.buttonMinHeight;
  const float padX = small ? 12.0f : met.buttonPaddingX;
  ImVec2 size(ts.x + padX * 2, h);
  const float avail = ImGui::GetContentRegionAvail().x;
  if (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) enabled = false;
  if (fullWidth || (size.x > avail && avail > 40)) size.x = avail;

  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(label);
  if (!enabled) ImGui::BeginDisabled();
  const bool clicked = ImGui::InvisibleButton("##btn", size);
  if (!enabled) ImGui::EndDisabled();
  ImGui::PopID();
  const bool hovered = enabled && ImGui::IsItemHovered();
  const bool held = enabled && ImGui::IsItemActive();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 b(p.x + size.x, p.y + size.y);
  const float alpha = enabled ? 1.0f : 0.42f;
  const float r = met.buttonRadius;
  ImU32 fg = col(th.ink, alpha);
  switch (kind) {
    case ButtonKind::Primary: {
      // gold gradient (#ffd06a -> gold), gold-ink text, soft glow
      if (enabled && hovered) dl->AddRectFilled(ImVec2(p.x - 2, p.y - 1), ImVec2(b.x + 2, b.y + 3), col(th.gold, 0.18f), r + 2);
      rectGradient(dl, p, b, hexCol("#ffd06a", alpha), col(th.gold, alpha), r, true);
      if (held) dl->AddRectFilled(p, b, IM_COL32(0, 0, 0, 30), r);
      fg = col(th.goldInk, alpha);
      break;
    }
    case ButtonKind::Quiet:
      if (hovered) dl->AddRectFilled(p, b, col(th.surface3, 0.6f), r);
      fg = col(hovered ? th.ink : th.ink2, alpha);
      break;
    case ButtonKind::Danger:
      dl->AddRectFilled(p, b, col(th.red, 0.14f * alpha), r);
      dl->AddRect(p, b, col(th.red, alpha), r);
      fg = col(th.red, alpha);
      break;
    default:
      dl->AddRectFilled(p, b, col(held ? th.surface3 : th.surface2, alpha), r);
      dl->AddRect(p, b, col(hovered ? th.gold : th.lineStrong, alpha), r);
      break;
  }
  drawText(dl, fr, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), fg,
           std::string_view(label, static_cast<size_t>(labelEnd - label)));
  if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  return clicked && enabled;
}

}  // namespace

bool button(const char* label, ButtonKind kind, bool enabled) { return buttonImpl(label, kind, enabled, false); }
bool buttonFull(const char* label, ButtonKind kind, bool enabled) { return buttonImpl(label, kind, enabled, true); }

bool iconButton(const char* iconName, const char* tooltip, bool enabled) {
  if (!enabled) return false;
  return iconWidget(iconName, 16.0f, currentTheme().ink, tooltip);
}

int tabs(const std::vector<TabItem>& items, int currentIdx, bool small) {
  // Inset strip: bg surface-2, radius 11, padding 4; selected tab: bg surface, rounded 8
  const auto& th = currentTheme();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float h = small ? 30.0f : 38.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  float w = 8.0f;
  for (const auto& it : items) w += textSize(FontRole::BodyStrong, it.label).x + 28.0f;
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h + 8), col(th.surface2), 11.0f);
  float x = p.x + 4;
  int out = currentIdx;
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    const float tw = textSize(FontRole::BodyStrong, items[i].label).x + 28.0f;
    ImGui::SetCursorScreenPos(ImVec2(x, p.y + 4));
    ImGui::PushID(i);
    if (ImGui::InvisibleButton("##tab", ImVec2(tw, h))) out = i;
    ImGui::PopID();
    const bool sel = i == currentIdx;
    if (sel) dl->AddRectFilled(ImVec2(x, p.y + 4), ImVec2(x + tw, p.y + 4 + h), col(th.surface), 8.0f);
    const ImVec2 ts = textSize(FontRole::BodyStrong, items[i].label);
    drawText(dl, FontRole::BodyStrong, ImVec2(x + 14, p.y + 4 + (h - ts.y) * 0.5f), col(sel ? th.ink : th.muted), items[i].label);
    x += tw;
  }
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(w, h + 8));
  return out;
}

bool chip(const char* label, bool selected) {
  // Pill radio button: min-height 38 (small 30); selected = gold border + gold-soft background
  const auto& th = currentTheme();
  // "PBK##inn1": show the part before ##, use the whole string as the id
  const std::string_view shown(label, static_cast<size_t>(ImGui::FindRenderedTextEnd(label) - label));
  const ImVec2 ts = textSize(FontRole::BodyStrong, shown);
  const ImVec2 size(ts.x + 28.0f, metrics().chipSmallHeight);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(label);
  const bool clicked = ImGui::InvisibleButton("##chip", size);
  ImGui::PopID();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 b(p.x + size.x, p.y + size.y);
  dl->AddRectFilled(p, b, selected ? col(th.goldSoft) : col(th.surface2), size.y * 0.5f);
  dl->AddRect(p, b, col(selected ? th.gold : (hovered ? th.lineStrong : th.line)), size.y * 0.5f);
  drawText(dl, FontRole::BodyStrong, ImVec2(p.x + 14, p.y + (size.y - ts.y) * 0.5f), col(selected ? th.goldText : th.ink2), shown);
  return clicked;
}

int segmented(const std::vector<std::string>& options, int currentIdx) {
  // Equal cell grid in an inset strip; selected cell gold fill, gold-ink text
  const auto& th = currentTheme();
  const int n = static_cast<int>(options.size());
  if (n == 0) return currentIdx;
  const float w = ImGui::GetContentRegionAvail().x;
  const float h = 34.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(th.surface2), 10.0f);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), col(th.line), 10.0f);
  const float cw = (w - 6) / n;
  int out = currentIdx;
  for (int i = 0; i < n; ++i) {
    const ImVec2 a(p.x + 3 + cw * i, p.y + 3);
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(i);
    if (ImGui::InvisibleButton("##seg", ImVec2(cw, h - 6))) out = i;
    ImGui::PopID();
    const bool sel = i == currentIdx;
    if (sel) rectGradient(dl, a, ImVec2(a.x + cw, a.y + h - 6), hexCol("#ffd06a"), col(th.gold), 7.0f, true);
    else if (ImGui::IsItemHovered()) dl->AddRectFilled(a, ImVec2(a.x + cw, a.y + h - 6), col(th.surface3), 7.0f);
    const ImVec2 ts = textSize(FontRole::BodyStrong, options[i]);
    drawText(dl, FontRole::BodyStrong, ImVec2(a.x + (cw - ts.x) * 0.5f, a.y + (h - 6 - ts.y) * 0.5f),
             col(sel ? th.goldInk : th.ink2), options[i]);
  }
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(w, h));
  return out;
}

bool selectBox(const char* id, const char* label, const std::vector<std::string>& options, int* index,
               const char* placeholder, float width) {
  const auto& th = currentTheme();
  const float w = width > 0 ? width : ImGui::GetContentRegionAvail().x;
  ImGui::BeginGroup();
  {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.muted), "%s", upper(label).c_str());
  }
  const bool has = index && *index >= 0 && *index < static_cast<int>(options.size());
  ImGui::PushID(id);
  ImGui::SetNextItemWidth(w);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 9));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, metrics().smallRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, metrics().smallRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
  ImGui::PushStyleColor(ImGuiCol_Border, vec(th.lineStrong));
  ImGui::PushStyleColor(ImGuiCol_Text, vec(has ? th.ink : th.muted));
  bool changed = false;
  if (ImGui::BeginCombo("##select", has ? options[static_cast<size_t>(*index)].c_str() : placeholder)) {
    ImGui::PushStyleColor(ImGuiCol_Text, vec(th.ink));
    for (int i = 0; i < static_cast<int>(options.size()); ++i) {
      const bool sel = has && *index == i;
      ImGui::PushID(i);
      if (ImGui::Selectable(options[static_cast<size_t>(i)].c_str(), sel)) {
        changed = *index != i;
        *index = i;
      }
      if (sel) ImGui::SetItemDefaultFocus();
      ImGui::PopID();
    }
    ImGui::PopStyleColor();
    ImGui::EndCombo();
  }
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(4);
  ImGui::PopID();
  ImGui::EndGroup();
  return changed;
}

bool switchToggle(const char* label, bool* value) {
  // Custom switch with gold accent; returns true when toggled
  if (!value) return false;
  const auto& th = currentTheme();
  const std::string_view shown(label, static_cast<size_t>(ImGui::FindRenderedTextEnd(label) - label));
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImVec2 ts = textSize(FontRole::Body, shown);
  const float tw = 36.0f, thh = 20.0f;
  const ImVec2 size(tw + 10 + ts.x, ImMax(thh, ts.y));
  ImGui::PushID(label);
  const bool clicked = ImGui::InvisibleButton("##sw", size);
  ImGui::PopID();
  if (clicked) *value = !*value;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float y = p.y + (size.y - thh) * 0.5f;
  dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + tw, y + thh), *value ? col(th.gold) : col(th.surface3), thh * 0.5f);
  const float kx = *value ? p.x + tw - thh * 0.5f : p.x + thh * 0.5f;
  dl->AddCircleFilled(ImVec2(kx, y + thh * 0.5f), thh * 0.5f - 3, *value ? col(th.goldInk) : col(th.ink2));
  drawText(dl, FontRole::Body, ImVec2(p.x + tw + 10, p.y + (size.y - ts.y) * 0.5f), col(th.ink2), shown);
  return clicked;
}

void notice(const std::string& text, NoticeKind kind) {
  // Radius 10 banner: ok green tint, warn gold-soft tint, error red tint
  const auto& th = currentTheme();
  const ColorRGBA& c = kind == NoticeKind::Ok ? th.green : kind == NoticeKind::Warn ? th.gold : th.red;
  const float w = ImGui::GetContentRegionAvail().x;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - 28);
  ImGui::SetCursorScreenPos(ImVec2(p.x + 14, p.y + 10));
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->ChannelsSplit(2);
  dl->ChannelsSetCurrent(1);
  ImGui::TextUnformatted(text.c_str());
  ImGui::PopTextWrapPos();
  const float h = ImGui::GetItemRectSize().y + 20;
  dl->ChannelsSetCurrent(0);
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(c, 0.12f), 10.0f);
  dl->AddRect(p, ImVec2(p.x + w, p.y + h), col(c, 0.45f), 10.0f);
  dl->ChannelsMerge();
  ImGui::SetCursorScreenPos(p);
  ImGui::Dummy(ImVec2(w, h));
}

bool confirmInline(const char* message, const char* confirmLabel, const char* cancelLabel) {
  notice(message, NoticeKind::Warn);
  const bool ok = button(confirmLabel, ButtonKind::Primary);
  ImGui::SameLine();
  button(cancelLabel, ButtonKind::Quiet);
  return ok;
}

void beginStickyActions() {
  ImGui::Separator();
}

void endStickyActions() {}

} // namespace t20::ui
