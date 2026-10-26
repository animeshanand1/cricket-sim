#include "ui/match/commentary.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/panel.hpp"

namespace t20::ui {

namespace {

constexpr int kPage = 20;

bool isKey(CommentKind k) {
  switch (k) {
    case CommentKind::wicket:
    case CommentKind::four:
    case CommentKind::six:
    case CommentKind::milestone:
    case CommentKind::drop:
    case CommentKind::review:
    case CommentKind::over:
    case CommentKind::result: return true;
    default: return false;
  }
}

// The badge in front of a line: W red, 4 blue ring, 6 gold, • muted, + dashed, ★ purple,
// D red dashed, R purple ring; plain runs a small neutral disc
void badge(ImDrawList* dl, ImVec2 c, CommentKind k) {
  const auto& th = currentTheme();
  const float r = 10.0f;
  auto label = [&](const char* t, ImU32 fg) {
    const ImVec2 ts = textSize(FontRole::Eyebrow, t);
    drawText(dl, FontRole::Eyebrow, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), fg, t);
  };
  auto dashed = [&](ImU32 colr) {
    for (int i = 0; i < 10; ++i) {
      const float a0 = IM_PI * 2 * i / 10, a1 = a0 + IM_PI / 10;
      dl->PathArcTo(c, r - 0.5f, a0, a1, 4);
      dl->PathStroke(colr, 0, 1.4f);
    }
  };
  switch (k) {
    case CommentKind::wicket:
      dl->AddCircleFilled(c, r, col(th.red));
      label("W", IM_COL32_WHITE);
      break;
    case CommentKind::four:
      dl->AddCircle(c, r - 0.5f, col(th.blue), 0, 1.6f);
      label("4", col(th.blue));
      break;
    case CommentKind::six:
      dl->AddCircleFilled(c, r, col(th.gold));
      label("6", col(th.goldInk));
      break;
    case CommentKind::dot: dl->AddCircle(c, 4.0f, col(th.muted), 12, 1.4f); break;
    case CommentKind::extra:
      dashed(col(th.orange));
      label("+", col(th.orange));
      break;
    case CommentKind::milestone:
      dl->AddCircleFilled(c, r, col(th.purple, 0.25f));
      label("\xe2\x98\x85", col(th.purple));
      break;
    case CommentKind::drop:
      dashed(col(th.red));
      label("D", col(th.red));
      break;
    case CommentKind::review:
      dl->AddCircle(c, r - 0.5f, col(th.purple), 0, 1.4f);
      label("R", col(th.purple));
      break;
    case CommentKind::run: dl->AddCircleFilled(c, 4.0f, col(th.ink2, 0.7f)); break;
    default: break;
  }
}

std::string tabLabel(const Match& m, const Innings& inn) {
  std::string s = m.teams[static_cast<size_t>(inn.batting)].id;
  if (inn.superOver) s += inn.superOver > 1 ? " SO" + std::to_string(inn.superOver) : " SO";
  return s;
}

}  // namespace

void drawCommentaryPanel(const Match& m, CommentaryState& st) {
  const auto& th = currentTheme();
  const int n = static_cast<int>(m.innings.size());
  if (n == 0) return;

  if (!st.on) {
    beginPanel("##commentary", "Commentary");
    ImGui::TextColored(vec(th.muted), "Commentary is off \xe2\x80\x94 the scoreboard and field view still show every ball.");
    if (button("Turn on", ButtonKind::Small)) st.on = true;
    endPanel();
    return;
  }

  // Collapsed: the latest line that isn't scene-setting
  std::string latest;
  for (int i = n - 1; i >= 0 && latest.empty(); --i) {
    const auto& lines = m.innings[static_cast<size_t>(i)].commentary;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
      if (it->kind != CommentKind::info) {
        latest = (it->over.empty() ? "" : it->over + "  ") + it->text;
        break;
      }
  }
  if (!collapsedPanel("Commentary", latest.c_str(), &st.open)) return;

  beginPanel("##commentary", "Commentary");
  const int sel = st.innings < 0 || st.innings >= n ? m.current : st.innings;
  // controls: innings tabs, key moments, turn off, minimise
  if (n > 1) {
    for (int i = 0; i < n; ++i) {
      if (chip((tabLabel(m, m.innings[static_cast<size_t>(i)]) + "##comm" + std::to_string(i)).c_str(), sel == i)) {
        st.innings = i == m.current ? -1 : i;
        st.shown = kPage;
      }
      ImGui::SameLine(0, 6);
    }
    ImGui::SameLine(0, 10);
  }
  switchToggle("Key moments only##comm", &st.keyOnly);
  ImGui::SameLine();
  rightAlign(buttonWidth("Turn off", ButtonKind::Small) + buttonWidth("Hide", ButtonKind::Small) + 8);
  if (button("Turn off##comm", ButtonKind::Small)) st.on = false;
  ImGui::SameLine(0, 8);
  if (button("Hide##comm", ButtonKind::Small)) st.open = false;
  ImGui::Dummy(ImVec2(0, 4));

  const auto& all = m.innings[static_cast<size_t>(sel)].commentary;
  std::vector<const CommentaryLine*> lines;
  for (auto it = all.rbegin(); it != all.rend(); ++it)
    if (!st.keyOnly || isKey(it->kind)) lines.push_back(&*it);
  const int total = static_cast<int>(lines.size());
  const int shown = std::min(total, std::max(kPage, st.shown));

  ImDrawList* dl = ImGui::GetWindowDrawList();
  for (int i = 0; i < shown; ++i) {
    const CommentaryLine& c = *lines[static_cast<size_t>(i)];
    ImGui::PushID(c.id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (c.kind == CommentKind::over) {
      // an over summary reads as a divider
      ImGui::Dummy(ImVec2(0, 2));
      FontScope fs(FontRole::Hint);
      ImGui::PushTextWrapPos(0);
      ImGui::TextColored(vec(th.ink2), "%s", c.text.c_str());
      ImGui::PopTextWrapPos();
      const float y = ImGui::GetItemRectMax().y + 3;
      dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + ImGui::GetContentRegionAvail().x, y), col(th.line));
      ImGui::Dummy(ImVec2(0, 4));
      ImGui::PopID();
      continue;
    }
    const bool tagged = c.kind != CommentKind::info && c.kind != CommentKind::result && !c.over.empty();
    float x = p.x;
    if (tagged) {
      FontScope fs(FontRole::Eyebrow);
      drawText(dl, FontRole::Eyebrow, ImVec2(x, p.y + 3), col(th.muted), c.over);
    }
    x += 34;
    badge(dl, ImVec2(x + 10, p.y + 9), c.kind);
    x += 28;
    ImGui::SetCursorScreenPos(ImVec2(x, p.y));
    const bool strong = c.kind == CommentKind::wicket || c.kind == CommentKind::six || c.kind == CommentKind::result ||
                        c.kind == CommentKind::milestone;
    {
      FontScope fs(strong ? FontRole::BodyStrong : FontRole::Body);
      ImGui::PushTextWrapPos(0);
      const ColorRGBA& tc = c.kind == CommentKind::result ? th.green : c.kind == CommentKind::info ? th.ink2 : th.ink;
      ImGui::TextColored(vec(tc), "%s", c.text.c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, std::max(ImGui::GetCursorScreenPos().y, p.y + 20)));
    ImGui::Dummy(ImVec2(0, 1));
    ImGui::PopID();
  }
  if (total == 0) {
    ImGui::TextColored(vec(th.muted), st.keyOnly ? "No key moments yet." : "Play a ball to start the commentary.");
  } else if (total > kPage) {
    ImGui::Dummy(ImVec2(0, 4));
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "Showing the latest %d of %d.", shown, total);
    if (shown < total) {
      ImGui::SameLine();
      if (button("Show 20 more", ButtonKind::Small)) st.shown = shown + kPage;
      ImGui::SameLine();
      if (button("Show all", ButtonKind::Small)) st.shown = total;
    }
    if (shown > kPage) {
      ImGui::SameLine();
      if (button("Show fewer", ButtonKind::Small)) st.shown = kPage;
    }
  }
  endPanel();
}

}  // namespace t20::ui
