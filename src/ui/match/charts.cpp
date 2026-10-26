#include "ui/match/charts.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "core/str.hpp"
#include "engine/rules.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/panel.hpp"

namespace t20::ui {

namespace {

struct Frame {
  ImVec2 a, b;  // the plot area
  float x0 = 0, x1 = 1, y0 = 0, y1 = 1;
  ImVec2 at(float x, float y) const {
    return ImVec2(a.x + (x - x0) / (x1 - x0) * (b.x - a.x), b.y - (y - y0) / (y1 - y0) * (b.y - a.y));
  }
  float xOf(float px) const { return x0 + (px - a.x) / (b.x - a.x) * (x1 - x0); }
};

// The innings that count for the charts (Super Overs are left out), and the series colour of each
std::vector<const Innings*> mainInnings(const Match& m) {
  std::vector<const Innings*> out;
  for (const Innings& inn : m.innings)
    if (!inn.superOver) out.push_back(&inn);
  return out;
}

ColorRGBA seriesOf(int i) { return i == 0 ? currentTheme().series1 : currentTheme().series2; }

const std::string& teamId(const Match& m, const Innings& inn) { return m.teams[static_cast<size_t>(inn.batting)].id; }

// Runs and wickets after each over (index k = after k+1 overs; the last may be part of an over)
struct OverPoint {
  float overs;
  int runs, wkts, overRuns, overWkts;
};
std::vector<OverPoint> overPoints(const Innings& inn) {
  std::vector<OverPoint> out;
  int runs = 0, wkts = 0;
  for (size_t k = 0; k < inn.overs.size(); ++k) {
    const Over& ov = inn.overs[k];
    if (ov.balls.empty()) continue;
    runs += ov.runs;
    wkts += ov.wickets;
    int legal = 0;
    for (const Ball& b : ov.balls) legal += b.kind == BallKind::extra && (b.wide || (b.label.find("nb") != std::string::npos)) ? 0 : 1;
    const bool done = static_cast<int>(k + 1) * 6 <= inn.legalBalls;
    out.push_back({done ? static_cast<float>(k + 1) : static_cast<float>(k) + std::min(legal, 6) / 6.0f, runs, wkts, ov.runs, ov.wickets});
  }
  return out;
}

// "9.3" -> 9.5 overs
float oversFromTag(const std::string& tag) {
  const size_t dot = tag.find('.');
  if (dot == std::string::npos) return static_cast<float>(std::atoi(tag.c_str()));
  return static_cast<float>(std::atoi(tag.substr(0, dot).c_str())) + static_cast<float>(std::atoi(tag.substr(dot + 1).c_str())) / 6.0f;
}

float niceStep(float range, bool small) {
  if (small) return range <= 40 ? 5.0f : 10.0f;
  return range <= 160 ? 25.0f : 50.0f;
}

void gridAndAxes(ImDrawList* dl, const Frame& f, float xStep, float yStep, const std::string& xSuffix, bool lastSuffixOnly) {
  const auto& th = currentTheme();
  for (float y = f.y0; y <= f.y1 + 0.01f; y += yStep) {
    const ImVec2 p = f.at(f.x0, y);
    dl->AddLine(ImVec2(f.a.x, p.y), ImVec2(f.b.x, p.y), col(th.line, 0.6f));
    const std::string t = std::to_string(static_cast<int>(y));
    const ImVec2 ts = textSize(FontRole::Eyebrow, t);
    drawText(dl, FontRole::Eyebrow, ImVec2(f.a.x - 8 - ts.x, p.y - ts.y * 0.5f), col(th.muted), t);
  }
  for (float x = f.x0; x <= f.x1 + 0.01f; x += xStep) {
    const ImVec2 p = f.at(x, f.y0);
    const bool last = x + xStep > f.x1 + 0.01f;
    const std::string t = std::to_string(static_cast<int>(x)) + (last || !lastSuffixOnly ? xSuffix : "");
    const ImVec2 ts = textSize(FontRole::Eyebrow, t);
    drawText(dl, FontRole::Eyebrow, ImVec2(p.x - ts.x * 0.5f, f.b.y + 6), col(th.muted), t);
  }
  dl->AddLine(ImVec2(f.a.x, f.b.y), ImVec2(f.b.x, f.b.y), col(th.lineStrong));
}

bool hoverArea(const char* id, const ImVec2& a, const ImVec2& b) {
  ImGui::SetCursorScreenPos(a);
  ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, b.x - a.x), std::max(1.0f, b.y - a.y)));
  return ImGui::IsItemHovered();
}

}  // namespace

// ---------------------------------------------------------------------------
// Worm
// ---------------------------------------------------------------------------

void drawWorm(const Match& m, float width, float height) {
  const auto& th = currentTheme();
  const auto inns = mainInnings(m);
  if (inns.empty()) return;
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  int maxBalls = 0, maxRuns = 0;
  for (const Innings* inn : inns) {
    maxBalls = std::max(maxBalls, inn->maxBalls);
    maxRuns = std::max(maxRuns, std::max(inn->runs, inn->target.value_or(0)));
  }
  const float overs = std::max(1.0f, maxBalls / 6.0f);
  const float yStep = niceStep(static_cast<float>(maxRuns), false);
  Frame f;
  f.a = ImVec2(o.x + 40, o.y + 10);
  f.b = ImVec2(o.x + width - 84, o.y + height - 26);
  f.x0 = 0;
  f.x1 = overs;
  f.y0 = 0;
  f.y1 = std::max(yStep, std::ceil((maxRuns + 5) / yStep) * yStep);

  // the powerplay band (scaled for a shortened innings)
  const Innings& first = *inns.front();
  int pp = 0;
  for (int ov = 0; ov < static_cast<int>(overs); ++ov)
    if (phaseAt(&first, ov) == Phase::pp) pp = ov + 1;
  if (pp > 0) {
    const ImVec2 pa = f.at(0, f.y1), pb = f.at(static_cast<float>(pp), 0);
    dl->AddRectFilled(pa, pb, col(th.gold, 0.06f));
    const char* label = pb.x - pa.x > 70 ? "Powerplay" : "PP";
    drawText(dl, FontRole::Eyebrow, ImVec2(pa.x + 6, pa.y + 4), col(th.goldText, 0.7f), upper(label));
  }
  const float xStep = overs <= 5 ? 1.0f : overs <= 10 ? 2.0f : 5.0f;
  gridAndAxes(dl, f, xStep, yStep, " ov", true);

  // the target
  for (const Innings* inn : inns)
    if (inn->target) {
      const ImVec2 p = f.at(0, static_cast<float>(*inn->target));
      for (float x = f.a.x; x < f.b.x; x += 10) dl->AddLine(ImVec2(x, p.y), ImVec2(std::min(f.b.x, x + 5), p.y), col(th.gold, 0.7f), 1.2f);
      drawText(dl, FontRole::Eyebrow, ImVec2(f.a.x + 6, p.y - 14), col(th.goldText), "TARGET " + std::to_string(*inn->target));
    }

  // the lines, wickets and end labels
  std::vector<std::pair<float, std::string>> ends;
  std::vector<std::vector<OverPoint>> all;
  for (size_t i = 0; i < inns.size(); ++i) {
    const Innings& inn = *inns[i];
    const auto pts = overPoints(inn);
    all.push_back(pts);
    const ImU32 c = col(seriesOf(static_cast<int>(i)));
    std::vector<ImVec2> line = {f.at(0, 0)};
    for (const auto& p : pts) line.push_back(f.at(p.overs, static_cast<float>(p.runs)));
    if (line.size() > 1) dl->AddPolyline(line.data(), static_cast<int>(line.size()), c, 0, 2.4f);
    for (const FallOfWicket& w : inn.fow) {
      const ImVec2 p = f.at(oversFromTag(w.over), static_cast<float>(w.runs));
      dl->AddCircleFilled(p, 4.0f, c);
      dl->AddCircle(p, 4.0f, col(th.surface), 12, 1.5f);
    }
    if (!pts.empty()) ends.emplace_back(f.at(pts.back().overs, static_cast<float>(pts.back().runs)).y, cat(teamId(m, inn), ' ', inn.runs, '/', inn.wickets));
  }
  // end labels, nudged apart
  if (ends.size() == 2 && std::fabs(ends[0].first - ends[1].first) < 16) {
    const float mid = (ends[0].first + ends[1].first) * 0.5f;
    const bool firstUp = ends[0].first <= ends[1].first;
    ends[0].first = mid + (firstUp ? -9 : 9);
    ends[1].first = mid + (firstUp ? 9 : -9);
  }
  for (size_t i = 0; i < ends.size(); ++i)
    drawText(dl, FontRole::BodyStrong, ImVec2(f.b.x + 8, ends[i].first - 8), col(seriesOf(static_cast<int>(i))), ends[i].second);

  // hover: crosshair and "After N overs · RJR 87/2 · MUI 91/3"
  if (hoverArea("##worm", f.a, f.b)) {
    const int n = std::clamp(static_cast<int>(std::lround(f.xOf(ImGui::GetIO().MousePos.x))), 1, static_cast<int>(overs));
    const ImVec2 p = f.at(static_cast<float>(n), 0);
    dl->AddLine(ImVec2(p.x, f.a.y), ImVec2(p.x, f.b.y), col(th.ink2, 0.5f));
    std::string tip = "After " + std::to_string(n) + (n == 1 ? " over" : " overs");
    for (size_t i = 0; i < all.size(); ++i)
      for (const auto& q : all[i])
        if (std::fabs(q.overs - static_cast<float>(n)) < 0.01f) tip += cat(" \xc2\xb7 ", teamId(m, *inns[i]), ' ', q.runs, '/', q.wkts);
    ImGui::SetTooltip("%s", tip.c_str());
  }
  ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + height));
  ImGui::Dummy(ImVec2(width, 4));

  // the over-by-over table
  if (foldRow("##wormtable", "Over-by-over table", nullptr)) {
    if (ImGui::BeginTable("##obo", 1 + static_cast<int>(inns.size()), ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Over");
      for (const Innings* inn : inns) ImGui::TableSetupColumn(teamId(m, *inn).c_str());
      ImGui::TableHeadersRow();
      for (int n = 1; n <= static_cast<int>(std::ceil(overs)); ++n) {
        bool any = false;
        for (const auto& pts : all) any = any || static_cast<int>(pts.size()) >= n;
        if (!any) break;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%d", n);
        for (const auto& pts : all) {
          ImGui::TableNextColumn();
          if (static_cast<int>(pts.size()) >= n) {
            const auto& q = pts[static_cast<size_t>(n - 1)];
            ImGui::Text("%d%s  (%d/%d)", q.overRuns, q.overWkts ? cat(", ", q.overWkts, 'w').c_str() : "", q.runs, q.wkts);
          }
        }
      }
      ImGui::EndTable();
    }
    endFoldRow();
  }
}

// ---------------------------------------------------------------------------
// Manhattan
// ---------------------------------------------------------------------------

void drawManhattan(const Match& m, float width, float height) {
  const auto& th = currentTheme();
  const auto inns = mainInnings(m);
  if (inns.empty()) return;
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  std::vector<std::vector<OverPoint>> all;
  int maxOver = 0, maxBalls = 0;
  for (const Innings* inn : inns) {
    all.push_back(overPoints(*inn));
    maxBalls = std::max(maxBalls, inn->maxBalls);
    for (const auto& p : all.back()) maxOver = std::max(maxOver, p.overRuns);
  }
  const int overs = std::max(1, (maxBalls + 5) / 6);
  const float yStep = niceStep(static_cast<float>(maxOver), true);
  Frame f;
  f.a = ImVec2(o.x + 34, o.y + 10);
  f.b = ImVec2(o.x + width - 10, o.y + height - 26);
  f.x0 = 0;
  f.x1 = static_cast<float>(overs);
  f.y0 = 0;
  f.y1 = std::max(yStep, std::ceil((maxOver + 2) / yStep) * yStep);
  // y grid; x labels under each over (every 2nd or 5th when crowded)
  for (float y = 0; y <= f.y1 + 0.01f; y += yStep) {
    const ImVec2 p = f.at(0, y);
    dl->AddLine(ImVec2(f.a.x, p.y), ImVec2(f.b.x, p.y), col(th.line, 0.6f));
    const std::string t = std::to_string(static_cast<int>(y));
    const ImVec2 ts = textSize(FontRole::Eyebrow, t);
    drawText(dl, FontRole::Eyebrow, ImVec2(f.a.x - 8 - ts.x, p.y - ts.y * 0.5f), col(th.muted), t);
  }
  const float slot = (f.b.x - f.a.x) / overs;
  const int every = slot >= 24 ? 1 : slot >= 12 ? 2 : 5;
  for (int k = 0; k < overs; ++k)
    if ((k + 1) % every == 0 || k == 0) {
      const std::string t = std::to_string(k + 1);
      const ImVec2 ts = textSize(FontRole::Eyebrow, t);
      drawText(dl, FontRole::Eyebrow, ImVec2(f.a.x + slot * (k + 0.5f) - ts.x * 0.5f, f.b.y + 6), col(th.muted), t);
    }
  dl->AddLine(ImVec2(f.a.x, f.b.y), ImVec2(f.b.x, f.b.y), col(th.lineStrong));
  const int series = static_cast<int>(all.size());
  const float barW = std::max(2.0f, (slot - 6) / std::max(1, series));
  for (int s = 0; s < series; ++s)
    for (size_t k = 0; k < all[static_cast<size_t>(s)].size() && static_cast<int>(k) < overs; ++k) {
      const OverPoint& p = all[static_cast<size_t>(s)][k];
      const float x = f.a.x + slot * k + 3 + barW * s;
      const ImVec2 top = f.at(0, static_cast<float>(p.overRuns));
      dl->AddRectFilled(ImVec2(x, top.y), ImVec2(x + barW - 1, f.b.y), col(seriesOf(s), 0.9f), std::min(4.0f, barW * 0.4f),
                        ImDrawFlags_RoundCornersTop);
      for (int w = 0; w < p.overWkts; ++w)
        dl->AddCircleFilled(ImVec2(x + (barW - 1) * 0.5f, top.y - 6 - w * 9), 3.5f, col(th.red));
    }
  if (hoverArea("##manhattan", f.a, f.b)) {
    const int k = std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - f.a.x) / slot), 0, overs - 1);
    dl->AddRectFilled(ImVec2(f.a.x + slot * k, f.a.y), ImVec2(f.a.x + slot * (k + 1), f.b.y), col(th.ink2, 0.08f));
    std::string tip = "Over " + std::to_string(k + 1);
    for (int s = 0; s < series; ++s)
      if (k < static_cast<int>(all[static_cast<size_t>(s)].size())) {
        const OverPoint& p = all[static_cast<size_t>(s)][static_cast<size_t>(k)];
        tip += cat(" \xc2\xb7 ", teamId(m, *inns[static_cast<size_t>(s)]), ' ', p.overRuns, p.overRuns == 1 ? " run" : " runs",
                   p.overWkts ? cat(", ", p.overWkts, p.overWkts == 1 ? " wkt" : " wkts") : std::string());
      }
    ImGui::SetTooltip("%s", tip.c_str());
  }
  ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + height));
  ImGui::Dummy(ImVec2(width, 4));
}

// ---------------------------------------------------------------------------
// Win chance
// ---------------------------------------------------------------------------

void drawWinChart(const Match& m, const std::vector<app::MatchSession::WinPoint>& points, float width, float height) {
  const auto& th = currentTheme();
  if (points.size() < 2) {
    FontScope fs(FontRole::Hint);
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(vec(th.muted), "Win probability builds up while you play ball by ball or over by over: a point at the end "
                                      "of each over, at each wicket and every ball near the end of a chase. Stretches you sim "
                                      "through are skipped.");
    ImGui::PopTextWrapPos();
    return;
  }
  const int firstSide = m.battingFirst.value_or(0);
  const std::string& a = m.teams[static_cast<size_t>(firstSide)].id;
  const std::string& b = m.teams[static_cast<size_t>(1 - firstSide)].id;
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  Frame f;
  f.a = ImVec2(o.x + 70, o.y + 10);
  f.b = ImVec2(o.x + width - 14, o.y + height - 26);
  f.x0 = 0;
  f.x1 = 2;
  f.y0 = 0;
  f.y1 = 1;
  auto xOfPoint = [](const app::MatchSession::WinPoint& p) {
    return static_cast<float>(p.innings) + static_cast<float>(p.balls) / static_cast<float>(std::max(1, p.maxBalls));
  };
  // axis: top = the side batting first certain, bottom = the other side certain
  const std::pair<float, std::string> yTicks[] = {{1.0f, a + " 100%"}, {0.75f, "75%"}, {0.5f, "50%"}, {0.25f, "75%"}, {0.0f, b + " 100%"}};
  for (const auto& [y, label] : yTicks) {
    const ImVec2 p = f.at(0, y);
    dl->AddLine(ImVec2(f.a.x, p.y), ImVec2(f.b.x, p.y), col(y == 0.5f ? th.lineStrong : th.line, y == 0.5f ? 1.0f : 0.5f));
    const ImVec2 ts = textSize(FontRole::Eyebrow, label);
    drawText(dl, FontRole::Eyebrow, ImVec2(f.a.x - 8 - ts.x, p.y - ts.y * 0.5f),
             col(y > 0.5f ? th.series1 : y < 0.5f ? th.series2 : th.muted), label);
  }
  const int half = std::max(1, points.front().maxBalls / 6);
  const std::pair<float, std::string> xTicks[] = {
      {0.0f, "Start"}, {0.5f, std::to_string(half / 2) + " ov"}, {1.0f, std::to_string(half) + " ov"}, {1.5f, std::to_string(half / 2) + " ov"}, {2.0f, "End"}};
  for (const auto& [x, label] : xTicks) {
    const ImVec2 p = f.at(x, 0);
    const ImVec2 ts = textSize(FontRole::Eyebrow, label);
    drawText(dl, FontRole::Eyebrow, ImVec2(std::clamp(p.x - ts.x * 0.5f, f.a.x, f.b.x - ts.x), f.b.y + 6), col(th.muted), label);
  }
  // innings break
  {
    const ImVec2 p = f.at(1, 0);
    for (float y = f.a.y; y < f.b.y; y += 8) dl->AddLine(ImVec2(p.x, y), ImVec2(p.x, std::min(f.b.y, y + 4)), col(th.muted, 0.7f));
    drawText(dl, FontRole::Eyebrow, ImVec2(p.x + 4, f.a.y + 2), col(th.muted), "INNINGS BREAK");
  }
  // shaded areas either side of 50% (each segment split where it crosses), then the line
  const float midY = f.at(0, 0.5f).y;
  const ImU32 above = col(th.series1, 0.28f), below = col(th.series2, 0.28f);
  const ImDrawListFlags flags = dl->Flags;
  dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;  // no seams between neighbouring slices
  for (size_t i = 1; i < points.size(); ++i) {
    const ImVec2 p0 = f.at(xOfPoint(points[i - 1]), static_cast<float>(points[i - 1].first));
    const ImVec2 p1 = f.at(xOfPoint(points[i]), static_cast<float>(points[i].first));
    if (p1.x - p0.x < 0.01f) continue;
    const float d0 = p0.y - midY, d1 = p1.y - midY;
    if (d0 * d1 >= 0) {
      const ImVec2 quad[4] = {ImVec2(p0.x, midY), p0, p1, ImVec2(p1.x, midY)};
      dl->AddConvexPolyFilled(quad, 4, (d0 + d1) < 0 ? above : below);
    } else {
      const float t = d0 / (d0 - d1);
      const ImVec2 x(p0.x + (p1.x - p0.x) * t, midY);
      const ImVec2 a[3] = {ImVec2(p0.x, midY), p0, x}, b[3] = {x, p1, ImVec2(p1.x, midY)};
      dl->AddConvexPolyFilled(a, 3, d0 < 0 ? above : below);
      dl->AddConvexPolyFilled(b, 3, d1 < 0 ? above : below);
    }
  }
  dl->Flags = flags;
  std::vector<ImVec2> line;
  for (const auto& p : points) line.push_back(f.at(xOfPoint(p), static_cast<float>(p.first)));
  dl->AddPolyline(line.data(), static_cast<int>(line.size()), col(th.ink), 0, 2.0f);

  if (hoverArea("##winchart", f.a, f.b)) {
    const float mx = ImGui::GetIO().MousePos.x;
    size_t best = 0;
    for (size_t i = 1; i < line.size(); ++i)
      if (std::fabs(line[i].x - mx) < std::fabs(line[best].x - mx)) best = i;
    dl->AddCircleFilled(line[best], 4.5f, col(th.gold));
    const auto& p = points[best];
    ImGui::SetTooltip("%s\n%s %d%% \xc2\xb7 %s %d%%", p.label.c_str(), a.c_str(), static_cast<int>(std::lround(p.first * 100)), b.c_str(),
                      100 - static_cast<int>(std::lround(p.first * 100)));
  }
  ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + height));
  ImGui::Dummy(ImVec2(width, 4));
  if (foldRow("##wintable", "Win chance table", nullptr)) {
    if (ImGui::BeginTable("##wint", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("When");
      ImGui::TableSetupColumn(a.c_str());
      ImGui::TableSetupColumn(b.c_str());
      ImGui::TableHeadersRow();
      for (const auto& p : points) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(p.label.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d%%", static_cast<int>(std::lround(p.first * 100)));
        ImGui::TableNextColumn();
        ImGui::Text("%d%%", 100 - static_cast<int>(std::lround(p.first * 100)));
      }
      ImGui::EndTable();
    }
    endFoldRow();
  }
}

}  // namespace t20::ui
