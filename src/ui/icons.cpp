#include "ui/icons.hpp"
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "ui/theme.hpp"

namespace t20::ui {

// Exact 34 SVG path strings from ui.jsx (24px viewBox grid, 2px stroke, round caps/joins)
static const std::unordered_map<std::string, const char*> s_iconPaths = {
  {"home",     "M3 10.5 12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z"},
  {"trophy",   "M8 21h8M12 17v4M7 4h10v5a5 5 0 0 1-10 0zM17 5h3v2a3 3 0 0 1-3 3M7 5H4v2a3 3 0 0 0 3 3"},
  {"users",    "M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M9 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM22 21v-2a4 4 0 0 0-3-3.87M16 3.13a4 4 0 0 1 0 7.75"},
  {"chart",    "M3 3v18h18M7 15l4-4 3 3 5-6"},
  {"bat",      "M14.2 3.3l6.5 6.5-8.6 8.6a2.2 2.2 0 0 1-3.1 0l-3.4-3.4a2.2 2.2 0 0 1 0-3.1zM5.6 18.4 2.8 21.2"},
  {"ball",     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM6.5 5.5c2 2.5 2 10.5 0 13M17.5 5.5c-2 2.5-2 10.5 0 13"},
  {"stumps",   "M7 21V6M12 21V6M17 21V6M6 5h5M13 5h5"},
  {"play",     "M7 4l13 8-13 8z"},
  {"pause",    "M7 4h3v16H7zM14 4h3v16h-3z"},
  {"eye",      "M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12zM12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z"},
  {"forward",  "M4 5l8 7-8 7zM12 5l8 7-8 7z"},
  {"skip",     "M5 5l10 7-10 7zM19 5v14"},
  {"swap",     "M7 7h13l-4-4M17 17H4l4 4"},
  {"grip",     "M9 5h.01M9 12h.01M9 19h.01M15 5h.01M15 12h.01M15 19h.01"},
  {"dumbbell", "M6 6v12M18 6v12M3 9v6M21 9v6M6 12h12"},
  {"coin",     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 7v10M9 9.5h4.5a1.8 1.8 0 0 1 0 3.5H10a1.8 1.8 0 0 0 0 3.5H15"},
  {"calendar", "M4 5h16v16H4zM4 10h16M9 3v4M15 3v4"},
  {"list",     "M9 6h12M9 12h12M9 18h12M4 6h.01M4 12h.01M4 18h.01"},
  {"star",     "M12 3l2.8 5.7 6.2.9-4.5 4.4 1 6.2-5.5-2.9-5.5 2.9 1-6.2L3 9.6l6.2-.9z"},
  {"x",        "M6 6l12 12M18 6 6 18"},
  {"minus",    "M5 12h14"},
  {"edit",     "M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4"},
  {"check",    "M5 12.5l4.5 4.5L19 7"},
  {"undo",     "M9 14 4 9l5-5M4 9h11a5 5 0 0 1 0 10h-3"},
  {"chevron",  "M9 6l6 6-6 6"},
  {"back",     "M15 6l-6 6 6 6"},
  {"droplet",  "M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z"},
  {"target",   "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM12 12h.01"},
  {"shield",   "M12 2l8 3v6c0 5-3.5 9-8 11-4.5-2-8-6-8-11V5z"},
  {"pitch",    "M9 2h6v20H9zM9 6h6M9 18h6"},
  {"flame",    "M12 22a7 7 0 0 0 7-7c0-4-3-6-4-9-1.5 2-2.5 3-3 5-1-1-1.5-2-2-3-2 2.5-5 5.5-5 7a7 7 0 0 0 7 7z"},
  {"bolt",     "M13 2 4 14h7l-1 8 9-12h-7z"},
  {"field",    "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM11 10h2v4h-2z"},
  {"whistle",  "M3 11a5 5 0 0 0 10 1h8V8h-9.5A5 5 0 0 0 3 11zM8 11h.01"}
};

const char* getIconSvgPath(const char* name) {
  if (!name) return nullptr;
  auto it = s_iconPaths.find(name);
  if (it != s_iconPaths.end()) {
    return it->second;
  }
  return nullptr;
}

namespace {

// A tiny SVG path reader for the icon set: M L H V C S Q A Z (absolute and relative), flattened to
// polylines on the 24px grid. Results are cached per icon.
struct SubPath {
  std::vector<ImVec2> pts;
  bool closed = false;
};

struct PathReader {
  const char* s;
  bool atNumber() {
    while (*s == ' ' || *s == ',') ++s;
    return *s == '-' || *s == '.' || (*s >= '0' && *s <= '9');
  }
  float num() {
    atNumber();
    char* end = nullptr;
    const float v = std::strtof(s, &end);
    s = end;
    return v;
  }
  // arc flags can be written without separators ("0 0 1" or "011")
  float flag() {
    while (*s == ' ' || *s == ',') ++s;
    const float v = *s == '1' ? 1.0f : 0.0f;
    ++s;
    return v;
  }
};

void arcTo(std::vector<ImVec2>& out, ImVec2 p0, float rx, float ry, float phiDeg, bool large, bool sweep, ImVec2 p1) {
  if (rx == 0 || ry == 0) {
    out.push_back(p1);
    return;
  }
  const float phi = phiDeg * IM_PI / 180.0f, c = std::cos(phi), sn = std::sin(phi);
  const float dx = (p0.x - p1.x) * 0.5f, dy = (p0.y - p1.y) * 0.5f;
  const float x1 = c * dx + sn * dy, y1 = -sn * dx + c * dy;
  rx = std::fabs(rx);
  ry = std::fabs(ry);
  const float lam = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
  if (lam > 1) {
    rx *= std::sqrt(lam);
    ry *= std::sqrt(lam);
  }
  const float num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
  const float den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
  float k = den == 0 ? 0 : std::sqrt(std::max(0.0f, num / den));
  if (large == sweep) k = -k;
  const float cx1 = k * rx * y1 / ry, cy1 = -k * ry * x1 / rx;
  const float cx = c * cx1 - sn * cy1 + (p0.x + p1.x) * 0.5f, cy = sn * cx1 + c * cy1 + (p0.y + p1.y) * 0.5f;
  auto ang = [](float ux, float uy, float vx, float vy) {
    return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
  };
  const float t1 = ang(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
  float dt = ang((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
  if (!sweep && dt > 0) dt -= 2 * IM_PI;
  if (sweep && dt < 0) dt += 2 * IM_PI;
  const int n = std::max(4, static_cast<int>(std::fabs(dt) / (IM_PI / 12)));
  for (int i = 1; i <= n; ++i) {
    const float t = t1 + dt * i / n;
    out.push_back(ImVec2(cx + rx * std::cos(t) * c - ry * std::sin(t) * sn, cy + rx * std::cos(t) * sn + ry * std::sin(t) * c));
  }
}

void cubicTo(std::vector<ImVec2>& out, ImVec2 p0, ImVec2 c1, ImVec2 c2, ImVec2 p1) {
  for (int i = 1; i <= 12; ++i) {
    const float t = i / 12.0f, u = 1 - t;
    out.push_back(ImVec2(u * u * u * p0.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * p1.x,
                         u * u * u * p0.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * p1.y));
  }
}

std::vector<SubPath> parsePath(const char* d) {
  std::vector<SubPath> out;
  PathReader r{d};
  ImVec2 cur(0, 0), start(0, 0), lastCtrl(0, 0);
  char cmd = 0, prev = 0;
  auto sub = [&]() -> SubPath& {
    if (out.empty()) out.push_back({{cur}, false});
    return out.back();
  };
  while (*r.s) {
    while (*r.s == ' ' || *r.s == ',') ++r.s;
    if (!*r.s) break;
    if (std::isalpha(static_cast<unsigned char>(*r.s))) cmd = *r.s++;
    else if (cmd == 'M') cmd = 'L';  // extra pairs after a move are lines
    else if (cmd == 'm') cmd = 'l';
    const bool rel = std::islower(static_cast<unsigned char>(cmd));
    const ImVec2 base = rel ? cur : ImVec2(0, 0);
    switch (std::toupper(static_cast<unsigned char>(cmd))) {
      case 'M': {
        const float x = r.num(), y = r.num();
        cur = ImVec2(base.x + x, base.y + y);
        start = cur;
        out.push_back({{cur}, false});
        break;
      }
      case 'L': {
        const float x = r.num(), y = r.num();
        cur = ImVec2(base.x + x, base.y + y);
        sub().pts.push_back(cur);
        break;
      }
      case 'H': cur.x = (rel ? cur.x : 0) + r.num(); sub().pts.push_back(cur); break;
      case 'V': cur.y = (rel ? cur.y : 0) + r.num(); sub().pts.push_back(cur); break;
      case 'C': {
        const float a = r.num(), b = r.num(), c = r.num(), e = r.num(), f = r.num(), g = r.num();
        const ImVec2 c1(base.x + a, base.y + b), c2(base.x + c, base.y + e), p1(base.x + f, base.y + g);
        cubicTo(sub().pts, cur, c1, c2, p1);
        lastCtrl = c2;
        cur = p1;
        break;
      }
      case 'S': {
        const float c = r.num(), e = r.num(), f = r.num(), g = r.num();
        const bool smooth = prev == 'C' || prev == 'S';
        const ImVec2 c1 = smooth ? ImVec2(2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y) : cur;
        const ImVec2 c2(base.x + c, base.y + e), p1(base.x + f, base.y + g);
        cubicTo(sub().pts, cur, c1, c2, p1);
        lastCtrl = c2;
        cur = p1;
        break;
      }
      case 'Q': {
        const float a = r.num(), b = r.num(), f = r.num(), g = r.num();
        const ImVec2 q(base.x + a, base.y + b), p1(base.x + f, base.y + g);
        cubicTo(sub().pts, cur, ImVec2(cur.x + 2.0f / 3 * (q.x - cur.x), cur.y + 2.0f / 3 * (q.y - cur.y)),
                ImVec2(p1.x + 2.0f / 3 * (q.x - p1.x), p1.y + 2.0f / 3 * (q.y - p1.y)), p1);
        cur = p1;
        break;
      }
      case 'A': {
        const float rx = r.num(), ry = r.num(), rot = r.num();
        const bool large = r.flag() != 0, sw = r.flag() != 0;
        const float x = r.num(), y = r.num();
        const ImVec2 p1(base.x + x, base.y + y);
        arcTo(sub().pts, cur, rx, ry, rot, large, sw, p1);
        cur = p1;
        break;
      }
      case 'Z':
        sub().closed = true;
        cur = start;
        break;
      default:
        ++r.s;  // unknown: skip
        break;
    }
    prev = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
  }
  return out;
}

const std::vector<SubPath>& iconPaths(const char* name) {
  static std::unordered_map<std::string, std::vector<SubPath>> cache;
  static const std::vector<SubPath> none;
  const char* d = getIconSvgPath(name);
  if (!d) return none;
  auto it = cache.find(name);
  if (it == cache.end()) it = cache.emplace(name, parsePath(d)).first;
  return it->second;
}

}  // namespace

void drawIcon(void* drawList, const char* name, float posX, float posY, float size, ColorRGBA color) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  const float k = size / 24.0f;
  const float thick = std::max(1.2f, 2.0f * k);
  const ImU32 c = color.toU32();
  std::vector<ImVec2> pts;
  for (const SubPath& sp : iconPaths(name)) {
    pts.clear();
    for (const ImVec2& p : sp.pts) pts.push_back(ImVec2(posX + p.x * k, posY + p.y * k));
    if (pts.size() == 2 && std::fabs(pts[0].x - pts[1].x) + std::fabs(pts[0].y - pts[1].y) < 0.5f) {
      dl->AddCircleFilled(pts[0], thick * 0.6f, c);  // "h.01": a dot
      continue;
    }
    if (pts.size() < 2) continue;
    dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), c, sp.closed ? ImDrawFlags_Closed : ImDrawFlags_None, thick);
  }
}

bool iconWidget(const char* name, float size, ColorRGBA color, const char* tooltip) {
  // A square hit area a little larger than the glyph
  const float pad = 5.0f;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::PushID(name);
  const bool clicked = ImGui::InvisibleButton("##icon", ImVec2(size + pad * 2, size + pad * 2));
  ImGui::PopID();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (hovered) {
    const auto& th = currentTheme();
    dl->AddRectFilled(p, ImVec2(p.x + size + pad * 2, p.y + size + pad * 2), ColorRGBA(th.surface3.r, th.surface3.g, th.surface3.b, 0.9f).toU32(), 8.0f);
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (tooltip) ImGui::SetTooltip("%s", tooltip);
  }
  drawIcon(dl, name, p.x + pad, p.y + pad, size, color);
  return clicked;
}

} // namespace t20::ui
