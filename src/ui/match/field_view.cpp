#include "ui/match/field_view.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

#include "engine/field.hpp"
#include "engine/rules.hpp"
#include "ui/anim.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/panel.hpp"

namespace t20::ui {

namespace {

// Virtual geometry (S05 §8)
constexpr float kC = 210.0f, kRope = 168.0f, kInner = 86.0f;
constexpr float kBatY = kC + 30.0f;

ImVec2 landing(float dir, float dist) {
  const float a = dir * IM_PI / 180.0f;
  const float ux = std::sin(a), uy = -std::cos(a);
  const float offX = 0.0f, offY = kBatY - kC;
  const float b = offX * ux + offY * uy;
  const float c = offX * offX + offY * offY - kRope * kRope;
  const float t = -b + std::sqrt(std::max(0.0f, b * b - c));
  return ImVec2(kC + ux * t * dist, kBatY + uy * t * dist);
}

void dashedLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c, float thick, float dash) {
  const float len = std::hypot(b.x - a.x, b.y - a.y);
  if (len < 1) return;
  const ImVec2 u((b.x - a.x) / len, (b.y - a.y) / len);
  for (float s = 0; s < len; s += dash * 2) {
    const float e = std::min(len, s + dash);
    dl->AddLine(ImVec2(a.x + u.x * s, a.y + u.y * s), ImVec2(a.x + u.x * e, a.y + u.y * e), c, thick);
  }
}

void dashedCircle(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick, int dashes) {
  for (int i = 0; i < dashes; ++i) {
    const float a0 = 2 * IM_PI * i / dashes, a1 = a0 + IM_PI / dashes;
    dl->PathArcTo(c, r, a0, a1, 6);
    dl->PathStroke(col, 0, thick);
  }
}

}  // namespace

void drawFieldView(void* drawList, const FieldViewData& data, float canvasSize) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  const float k = canvasSize / 420.0f;
  auto P = [&](ImVec2 v) { return ImVec2(o.x + v.x * k, o.y + v.y * k); };
  const auto& th = currentTheme();

  // Outfield: a radial gradient with mown rings, the rope, the inner circle and the pitch
  const ImVec2 c = P(ImVec2(kC, kC));
  const int rings = 24;
  for (int i = rings; i >= 1; --i) {
    const float r = (kRope + 30) * k * i / rings;
    const float t = static_cast<float>(i) / rings;
    const ColorRGBA inner = ColorRGBA::fromHex("#1f6b3a"), outer = ColorRGBA::fromHex("#0c2c19");
    ColorRGBA g(inner.r + (outer.r - inner.r) * t, inner.g + (outer.g - inner.g) * t, inner.b + (outer.b - inner.b) * t, 1.0f);
    if (i % 2 == 0) g = ColorRGBA(g.r * 1.06f, g.g * 1.06f, g.b * 1.06f, 1.0f);
    dl->AddCircleFilled(c, r, g.toU32(), 96);
  }
  dl->AddCircle(c, kRope * k, IM_COL32(255, 255, 255, 140), 128, 2.0f * k);
  dashedCircle(dl, c, kInner * k, IM_COL32(255, 255, 255, 90), 1.2f * k, 40);
  dl->AddRectFilled(P(ImVec2(kC - 6, kC - 32)), P(ImVec2(kC + 6, kC + 32)), hexCol("#c9b27a", 0.85f), 2.0f * k);
  dl->AddLine(P(ImVec2(kC - 8, kBatY)), P(ImVec2(kC + 8, kBatY)), IM_COL32(255, 255, 255, 160), 1.2f * k);
  dl->AddLine(P(ImVec2(kC - 8, kC - 30)), P(ImVec2(kC + 8, kC - 30)), IM_COL32(255, 255, 255, 160), 1.2f * k);

  // OFF / LEG: off side on the right for a right-hander
  const ImVec2 offPos = P(ImVec2(data.isLeftHander ? 34 : 362, 30)), legPos = P(ImVec2(data.isLeftHander ? 362 : 34, 30));
  drawText(dl, FontRole::Eyebrow, offPos, IM_COL32(255, 255, 255, 120), "OFF");
  drawText(dl, FontRole::Eyebrow, legPos, IM_COL32(255, 255, 255, 120), "LEG");

  const ImVec2 bat = P(ImVec2(kC, kBatY));
  const float draw = sampleTween("shot_draw", 1.0f);
  for (const FieldShot& s : data.shots) {
    const float t = s.isLatest ? draw : 1.0f;
    const ImVec2 end = P(landing(s.dir, s.dist));
    if (s.isWicket) {
      const ImVec2 x = s.dist <= 0.01f ? bat : end;
      if (s.dist > 0.01f) dl->AddLine(bat, ImVec2(bat.x + (x.x - bat.x) * t, bat.y + (x.y - bat.y) * t), col(th.red, 0.5f), 1.4f * k);
      const float r = 6 * k;
      dl->AddLine(ImVec2(x.x - r, x.y - r), ImVec2(x.x + r, x.y + r), col(th.red), 2.6f * k);
      dl->AddLine(ImVec2(x.x - r, x.y + r), ImVec2(x.x + r, x.y - r), col(th.red), 2.6f * k);
      continue;
    }
    if (s.saved > 0 && s.fielderDir) {
      const ImVec2 f = P(landing(*s.fielderDir, s.fielderDist.value_or(0.8f)));
      dashedLine(dl, bat, ImVec2(bat.x + (f.x - bat.x) * t, bat.y + (f.y - bat.y) * t), IM_COL32(255, 255, 255, 170), 1.6f * k, 5 * k);
      dl->AddCircle(f, 7 * k, col(th.green), 24, 2.0f * k);
      continue;
    }
    const ImVec2 to(bat.x + (end.x - bat.x) * t, bat.y + (end.y - bat.y) * t);
    if (s.isSix) {
      // quadratic arc, gold with a glow
      const float dx = end.x - bat.x, dy = end.y - bat.y, len = std::hypot(dx, dy);
      const float side = s.dir >= 0 ? 1.0f : -1.0f;
      const ImVec2 ctrl((bat.x + end.x) * 0.5f - dy / std::max(1.0f, len) * len * 0.18f * side,
                        (bat.y + end.y) * 0.5f + dx / std::max(1.0f, len) * len * 0.18f * side);
      const int n = 24;
      ImVec2 pts[25];
      const int upto = std::max(1, static_cast<int>(n * t));
      for (int i = 0; i <= upto; ++i) {
        const float u = static_cast<float>(i) / n, v = 1 - u;
        pts[i] = ImVec2(v * v * bat.x + 2 * v * u * ctrl.x + u * u * end.x, v * v * bat.y + 2 * v * u * ctrl.y + u * u * end.y);
      }
      dl->AddPolyline(pts, upto + 1, col(th.gold, 0.25f), 0, 6.0f * k);
      dl->AddPolyline(pts, upto + 1, col(th.gold), 0, 2.6f * k);
      if (t >= 1.0f) dl->AddCircleFilled(end, 3.2f * k, col(th.gold));
    } else {
      const ImU32 lc = s.isFour ? col(th.blue) : s.runs == 0 ? IM_COL32(255, 255, 255, 70) : IM_COL32(235, 240, 245, 150);
      dl->AddLine(bat, to, lc, (s.isFour ? 2.6f : 1.6f) * k);
      if (s.isFour && t >= 1.0f) dl->AddCircleFilled(end, 2.6f * k, col(th.blue));
    }
    if (s.isLatest && t >= 1.0f) dl->AddCircle(end, 6 * k, IM_COL32(255, 255, 255, 200), 20, 1.4f * k);
  }

  for (const FielderPos& f : data.fielders) {
    const ImVec2 q = P(landing(f.dir, f.dist));
    dl->AddCircleFilled(q, 6.0f * k, hexCol("#0b1622"));
    dl->AddCircle(q, 6.0f * k, col(th.gold), 16, 1.6f * k);
  }
}

void fieldViewCanvas(const FieldViewData& data, float size) {
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##fieldcanvas", ImVec2(size, size));
  const bool hovered = ImGui::IsItemHovered();
  ImGui::SetCursorScreenPos(o);
  drawFieldView(ImGui::GetWindowDrawList(), data, size);
  ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + size));
  ImGui::Dummy(ImVec2(0, 0));
  if (!hovered) return;
  // Tooltips: the nearest fielder or saved shot under the mouse
  const float k = size / 420.0f;
  const ImVec2 m = ImGui::GetIO().MousePos;
  float best = 10.0f * k;
  std::string tip;
  for (const auto& f : data.fielders) {
    const ImVec2 v = landing(f.dir, f.dist);
    const float d = std::hypot(o.x + v.x * k - m.x, o.y + v.y * k - m.y);
    if (d < best) {
      best = d;
      tip = f.label;
    }
  }
  for (const auto& s : data.shots) {
    if (s.tip.empty()) continue;
    const ImVec2 v = s.saved > 0 && s.fielderDir ? landing(*s.fielderDir, s.fielderDist.value_or(0.8f)) : landing(s.dir, s.dist);
    const float d = std::hypot(o.x + v.x * k - m.x, o.y + v.y * k - m.y);
    if (d < best) {
      best = d;
      tip = s.tip;
    }
  }
  if (!tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
}

// ---------------------------------------------------------------------------
// The panel
// ---------------------------------------------------------------------------

namespace {

const char* kModes[] = {"This over", "Last 5 overs", "Innings", "Wagon wheel"};

bool isBatRun(const Ball& b) {
  return (b.kind == BallKind::run || b.kind == BallKind::b4 || b.kind == BallKind::b6) && !b.legBye && !b.bye;
}

std::string fielderName(const Match& m, const Innings& inn, Pos p) {
  if (!inn.fieldNow) return "";
  const PlayerId& id = inn.fieldNow->assign[static_cast<size_t>(p)];
  const Player* pl = id.empty() ? nullptr : m.players.find(id);
  return pl ? pl->name : "";
}

}  // namespace

void drawFieldViewPanel(const Match& m, FieldViewState& st) {
  const auto& th = currentTheme();
  if (m.innings.empty()) return;
  const Innings& inn = inningsOf(m);
  if (st.batter.empty() && inn.striker) st.batter = *inn.striker;

  // Which balls: this over (or the last one), the last five, the innings, or one batter's
  std::vector<const Over*> overs;
  for (const Over& ov : inn.overs)
    if (!ov.balls.empty()) overs.push_back(&ov);
  std::vector<const Over*> pick;
  if (st.mode == 0 && !overs.empty()) pick.push_back(overs.back());
  else if (st.mode == 1) pick.assign(overs.end() - std::min<ptrdiff_t>(5, static_cast<ptrdiff_t>(overs.size())), overs.end());
  else if (st.mode >= 2) pick = overs;

  const Ball* newest = overs.empty() ? nullptr : &overs.back()->balls.back();
  size_t total = 0;
  for (const Over* ov : overs) total += ov->balls.size();
  if (total != st.lastBalls) {
    if (total > st.lastBalls && total - st.lastBalls == 1) startTween("shot_draw", 0.0f, 1.0f, 0.45, EaseType::EaseOutCubic);
    st.lastBalls = total;
  }

  FieldViewData data;
  const Player* striker = inn.striker ? m.players.find(*inn.striker) : nullptr;
  const Player* wheel = st.mode == 3 ? m.players.find(st.batter) : nullptr;
  data.isLeftHander = (wheel ? wheel : striker) && (wheel ? wheel : striker)->hand == Hand::L;
  int runs = 0, fours = 0, sixes = 0, offRuns = 0, cutOff = 0;
  for (const Over* ov : pick)
    for (const Ball& b : ov->balls) {
      if (st.mode == 3 && b.faced != std::optional<PlayerId>(st.batter)) continue;
      if (isBatRun(b)) {
        runs += b.runs;
        fours += b.kind == BallKind::b4 ? 1 : 0;
        sixes += b.kind == BallKind::b6 ? 1 : 0;
        if (b.dir && *b.dir > 0) offRuns += b.runs;
      }
      cutOff += b.saved > 0 ? 1 : 0;
      if (!b.dir) continue;
      const bool wicket = b.kind == BallKind::wicket;
      const double dist = b.dist.value_or(0);
      if (!wicket && dist < 0.2) continue;
      const Player* faced = b.faced ? m.players.find(*b.faced) : nullptr;
      const float mirror = faced && faced->hand == Hand::L ? -1.0f : 1.0f;
      FieldShot s;
      s.dir = static_cast<float>(*b.dir) * mirror;
      s.dist = static_cast<float>(dist);
      s.runs = b.runs;
      s.isFour = b.kind == BallKind::b4;
      s.isSix = b.kind == BallKind::b6;
      s.isWicket = wicket;
      s.saved = b.saved;
      if (b.saved > 0 && b.pos) {
        const PositionDef& pd = position(*b.pos);
        s.fielderDir = static_cast<float>(pd.angle) * mirror;
        s.fielderDist = static_cast<float>(pd.dist);
        s.tip = std::string(b.saved == 4 ? "A four" : b.saved == 6 ? "A six" : std::to_string(b.saved) + " runs") + " cut off: " +
                std::to_string(b.runs) + (b.runs == 1 ? " run" : " runs");
      }
      s.isLatest = &b == newest;
      data.shots.push_back(s);
    }
  // The field as set for this ball, mirrored for the striker (or the wagon wheel's batter)
  std::string presetLabel;
  if (inn.fieldNow && m.status == MatchStatus::live) {
    presetLabel = std::string(presetByKey(inn.fieldNow->preset).label);
    const float mirror = data.isLeftHander ? -1.0f : 1.0f;
    if (st.showFielders)
      for (Pos p : inn.fieldNow->positions) {
        const PositionDef& pd = position(p);
        const std::string who = fielderName(m, inn, p);
        data.fielders.push_back({static_cast<float>(pd.angle) * mirror, static_cast<float>(pd.dist),
                                 std::string(pd.label) + (who.empty() ? "" : ": " + who)});
      }
  }

  const int legal = runs;
  const std::string summary = std::string(kModes[st.mode]) + ": " + std::to_string(legal) + " bat runs · " + std::to_string(fours) +
                              " fours · " + std::to_string(sixes) + " sixes";
  if (!collapsedPanel("Field view", summary.c_str(), &st.open)) return;
  beginPanel("##fieldview", "Field view");
  // header: mode tabs, minimise, the batter for the wagon wheel
  st.mode = tabs({{"over", kModes[0], ""}, {"last5", kModes[1], ""}, {"inns", kModes[2], ""}, {"wheel", kModes[3], ""}}, st.mode, true);
  ImGui::SameLine();
  rightAlign(buttonWidth("Hide", ButtonKind::Small));
  if (button("Hide##fv", ButtonKind::Small)) st.open = false;
  if (st.mode == 3) {
    std::vector<std::string> names;
    std::vector<PlayerId> ids;
    int sel = -1;
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings* bi = inn.batters.find(id);
      const Player* pl = m.players.find(id);
      if (!bi || !pl) continue;
      if (id == st.batter) sel = static_cast<int>(ids.size());
      names.push_back(pl->name + " · " + std::to_string(bi->runs) + " (" + std::to_string(bi->balls) + ")");
      ids.push_back(id);
    }
    ImGui::Dummy(ImVec2(0, 2));
    if (selectBox("##wheelbat", "Batter", names, &sel, "Choose a batter", 280) && sel >= 0) st.batter = ids[static_cast<size_t>(sel)];
  }
  ImGui::Dummy(ImVec2(0, 4));
  if (!presetLabel.empty()) switchToggle(("Show fielders (" + presetLabel + ")##fvf").c_str(), &st.showFielders);

  const float size = std::min(ImGui::GetContentRegionAvail().x, 420.0f);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - size) * 0.5f);
  fieldViewCanvas(data, size);
  ImGui::Dummy(ImVec2(0, 4));

  // legend and the stats strip
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto key = [&](ImU32 c, const char* label, bool dashed = false) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float y = p.y + ImGui::GetTextLineHeight() * 0.5f;
    if (dashed) dashedLine(dl, ImVec2(p.x, y), ImVec2(p.x + 18, y), c, 2.0f, 3.0f);
    else dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + 18, y), c, 2.4f);
    ImGui::Dummy(ImVec2(22, 0));
    ImGui::SameLine(0, 0);
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "%s", label);
    ImGui::SameLine(0, 14);
  };
  key(IM_COL32(235, 240, 245, 150), "1-3");
  key(col(th.blue), "Four");
  key(col(th.gold), "Six");
  key(col(th.red), "Wicket");
  key(IM_COL32(255, 255, 255, 170), "Cut off", true);
  ImGui::NewLine();
  if (beginGrid("##fvstats", 5, 6)) {
    auto stat = [&](const char* l, const std::string& v) {
      gridNext();
      FontScope fs(FontRole::Eyebrow);
      ImGui::TextColored(vec(th.muted), "%s", upper(l).c_str());
      FontScope fv(FontRole::H3);
      ImGui::TextUnformatted(v.c_str());
    };
    stat("Bat runs", std::to_string(runs));
    stat("Fours", std::to_string(fours));
    stat("Sixes", std::to_string(sixes));
    stat("Off side", runs > 0 ? std::to_string(static_cast<int>(std::lround(100.0 * offRuns / runs))) + "%" : "–");
    stat("Cut off", std::to_string(cutOff));
    endGrid();
  }
  if (data.shots.empty()) {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "No scoring shots to show yet.");
  }
  endPanel();
}

}  // namespace t20::ui
