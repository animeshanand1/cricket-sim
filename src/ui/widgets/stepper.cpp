#include "ui/widgets/stepper.hpp"

#include <imgui.h>

#include <string>

#include "ui/gfx.hpp"
#include "ui/theme.hpp"

namespace t20::ui {

void stepper(Screen currentScreen) {
  // Only on the match setup screens
  if (currentScreen != Screen::xi && currentScreen != Screen::training && currentScreen != Screen::toss) return;

  struct StepInfo {
    Screen screen;
    const char* label;
  };
  static const StepInfo steps[] = {{Screen::home, "Teams"},
                                   {Screen::xi, "Playing XI"},
                                   {Screen::training, "Training"},
                                   {Screen::toss, "Toss"},
                                   {Screen::match, "Match"}};
  int current = 0;
  for (int i = 0; i < 5; ++i)
    if (steps[i].screen == currentScreen) current = i;

  // Five pills in a row joined by short lines: done = green disc with a tick, current = gold
  // border on gold-soft with a gold disc, the rest muted
  const auto& th = currentTheme();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float h = 34.0f, disc = 22.0f, gap = 22.0f;
  float x = o.x;
  for (int i = 0; i < 5; ++i) {
    const bool done = i < current, cur = i == current;
    const ImVec2 ts = textSize(FontRole::BodyStrong, steps[i].label);
    const float w = 6 + disc + 8 + ts.x + 14;
    const ImVec2 a(x, o.y), b(x + w, o.y + h);
    dl->AddRectFilled(a, b, cur ? col(th.goldSoft) : col(th.surface2), h * 0.5f);
    dl->AddRect(a, b, cur ? col(th.gold) : col(th.line), h * 0.5f);
    const ImVec2 dc(a.x + 6 + disc * 0.5f, a.y + h * 0.5f);
    const ImU32 discCol = done ? col(th.green) : cur ? col(th.gold) : col(th.surface3);
    dl->AddCircleFilled(dc, disc * 0.5f, discCol);
    if (done) {
      dl->AddLine(ImVec2(dc.x - 5, dc.y), ImVec2(dc.x - 1.5f, dc.y + 4), col(th.goldInk), 2.0f);
      dl->AddLine(ImVec2(dc.x - 1.5f, dc.y + 4), ImVec2(dc.x + 5, dc.y - 4), col(th.goldInk), 2.0f);
    } else {
      const std::string n = std::to_string(i + 1);
      const ImVec2 ns = textSize(FontRole::Eyebrow, n);
      drawText(dl, FontRole::Eyebrow, ImVec2(dc.x - ns.x * 0.5f, dc.y - ns.y * 0.5f), cur ? col(th.goldInk) : col(th.muted), n);
    }
    drawText(dl, FontRole::BodyStrong, ImVec2(a.x + 6 + disc + 8, a.y + (h - ts.y) * 0.5f),
             cur ? col(th.goldText) : done ? col(th.ink) : col(th.muted), steps[i].label);
    x += w;
    if (i < 4) {
      dl->AddLine(ImVec2(x + 4, o.y + h * 0.5f), ImVec2(x + gap - 4, o.y + h * 0.5f), col(done ? th.green : th.line), 2.0f);
      x += gap;
    }
  }
  ImGui::Dummy(ImVec2(x - o.x, h));
  ImGui::Dummy(ImVec2(0, 10));
}

}  // namespace t20::ui
