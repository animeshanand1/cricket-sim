#include "ui/widgets/tags.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/format.hpp"
#include "ui/anim.hpp"
#include <imgui_internal.h>
#include <cmath>

namespace t20::ui {

namespace {

// An inline pill that takes part in the layout
void inlinePill(const std::string& text, ImU32 bg, ImU32 fg, ImU32 border, FontRole role = FontRole::Eyebrow) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImVec2 size = pill(ImGui::GetWindowDrawList(), p, role, text, bg, fg, border);
  ImGui::Dummy(size);
}

}  // namespace

void fitnessTag(const PlayerMeta& player) {
  const auto& th = currentTheme();
  if (player.injury.has_value() && player.injury->matches > 0) {
    inlinePill("Injured · " + player.injury->label + " (" + std::to_string(player.injury->matches) + ")",
               col(th.red, 0.15f), col(th.red), col(th.red, 0.5f));
    return;
  }
  float f = std::round(player.fitness);
  if (f >= 90.0f) {
    return;
  }
  const ColorRGBA& c = f >= 75 ? th.green : f >= 60 ? th.gold : th.red;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  dl->AddRectFilled(ImVec2(p.x, p.y + 6), ImVec2(p.x + 40, p.y + 11), col(th.surface3), 3);
  dl->AddRectFilled(ImVec2(p.x, p.y + 6), ImVec2(p.x + 40 * f / 100, p.y + 11), col(c), 3);
  const std::string label = (f >= 75 ? "Fit " : f >= 60 ? "Tired " : "Exhausted ") + std::to_string(static_cast<int>(f));
  drawText(dl, FontRole::Hint, ImVec2(p.x + 46, p.y), col(th.muted), label);
  ImGui::Dummy(ImVec2(46 + textSize(FontRole::Hint, label).x, 17));
}

void phaseTag(int age) {
  const auto& th = currentTheme();
  std::string phase = agePhase(age);
  ColorRGBA c = th.green;
  if (phase == "Prime") c = th.blue;
  else if (phase == "Experienced") c = th.gold;
  else if (phase == "Veteran") c = th.orange;
  else if (phase == "Twilight") c = th.red;
  inlinePill(upper(phase), col(c, 0.15f), col(c), col(c, 0.6f));
}

void osTag() {
  const auto& th = currentTheme();
  inlinePill("OS", 0, col(th.blue), col(th.blue));
}

void qTag() {
  const auto& th = currentTheme();
  inlinePill("Q", col(th.green), col(th.goldInk), 0);
}

void ipTag() {
  const auto& th = currentTheme();
  inlinePill("IP", col(th.goldSoft), col(th.goldText), col(th.gold, 0.6f));
}

void styleChip(const std::string& text, bool isGold) {
  const auto& th = currentTheme();
  inlinePill(text, col(th.surface3), col(isGold ? th.goldText : th.ink2), 0, FontRole::Hint);
}

void tag(const std::string& text, TagTone tone) {
  const auto& th = currentTheme();
  ColorRGBA c = th.ink2;
  switch (tone) {
    case TagTone::Hot:    c = th.gold; break;
    case TagTone::Strong: c = th.green; break;
    case TagTone::Weak:   c = th.red; break;
    case TagTone::Muted:  c = th.muted; break;
    default:              c = th.ink2; break;
  }
  inlinePill(upper(text), col(c, 0.13f), tone == TagTone::Hot ? col(th.goldText) : col(c), col(c, 0.35f));
}

void formPips(const std::vector<char>& results) {
  const auto& th = currentTheme();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  float x = p.x;
  for (char r : results) {
    const ImVec2 a(x, p.y), b(x + 20, p.y + 20);
    if (r == 'W') dl->AddRectFilled(a, b, col(th.green), 5);
    else if (r == 'L') dl->AddRectFilled(a, b, col(th.surface3), 5);
    else dl->AddRect(a, b, col(th.muted), 5);
    const char s[2] = {r, 0};
    const ImVec2 ts = textSize(FontRole::Eyebrow, s);
    drawText(dl, FontRole::Eyebrow, ImVec2(a.x + (20 - ts.x) * 0.5f, a.y + (20 - ts.y) * 0.5f),
             r == 'W' ? col(th.goldInk) : col(th.ink2), s);
    x += 24;
  }
  ImGui::Dummy(ImVec2(x - p.x, 20));
}

void kpiTile(const std::string& label, const std::string& value, TagTone tone) {
  const auto& th = currentTheme();
  ColorRGBA c = tone == TagTone::Hot ? th.goldText : tone == TagTone::Strong ? th.green : tone == TagTone::Weak ? th.red : th.ink;
  ImGui::BeginGroup();
  {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.muted), "%s", upper(label).c_str());
  }
  {
    FontScope fs(FontRole::Kpi);
    ImGui::TextColored(vec(c), "%s", value.c_str());
  }
  ImGui::EndGroup();
}

void drawBallPip(void* drawList, const BallPipData& ball, float cx, float cy, float size) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  const auto& th = currentTheme();
  float scale = 1.0f;
  if (ball.isNewest) scale = sampleTween("pip_pop", 1.0f);
  const float r = size * 0.5f * (0.4f + 0.6f * scale);
  const ImVec2 c(cx, cy);
  const ImVec2 a(cx - r, cy - r), b(cx + r, cy + r);
  ImU32 fg = col(th.boardInk);
  if (ball.isWicket) {
    dl->AddCircleFilled(c, r + 4, col(th.red, 0.25f));
    rectGradient(dl, a, b, hexCol("#b3242a"), hexCol("#ff5d62"), r, true);
  } else if (ball.isSix) {
    dl->AddCircleFilled(c, r + 4, col(th.gold, 0.28f));
    rectGradient(dl, a, b, hexCol("#ffdf8a"), hexCol("#f5b83d"), r, true);
    fg = col(th.goldInk);
  } else if (ball.isFour) {
    dl->AddCircleFilled(c, r, col(th.blue, 0.22f));
    dl->AddCircle(c, r - 0.5f, col(th.blue), 0, 1.6f);
    fg = hexCol("#9fd0ff");
  } else if (ball.isExtra) {
    // dashed ring
    for (int i = 0; i < 12; ++i) {
      const float a0 = i * IM_PI / 6, a1 = a0 + IM_PI / 10;
      dl->PathArcTo(c, r - 0.5f, a0, a1, 4);
      dl->PathStroke(col(th.orange), 0, 1.4f);
    }
    fg = col(th.orange);
  } else if (ball.runs == 0) {
    dl->AddCircleFilled(c, r, col(th.boardLine, 0.6f));
    fg = col(th.boardMuted);
  } else {
    dl->AddCircleFilled(c, r, col(th.boardLine));
  }
  if (ball.isSpecial) dl->AddCircle(c, r + 2.5f, col(th.gold), 0, 1.5f);
  if (scale > 0.6f) {
    const FontRole fr = ball.text.size() > 2 ? FontRole::Eyebrow : FontRole::H3;
    const ImVec2 ts = textSize(fr, ball.text);
    drawText(dl, fr, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), fg, ball.text);
  }
}

void ballPip(const BallPipData& ball) {
  // 32-px pill on the dark board; newest pip pops in (scale 0.4 -> 1.0 over 0.35s)
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(32, 32));
  drawBallPip(ImGui::GetWindowDrawList(), ball, p.x + 16, p.y + 16, 32);
}

} // namespace t20::ui
