#include "ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include "ui/gfx.hpp"
#include "ui/fonts_embedded.hpp"
#include <cmath>

namespace t20::ui {

static const ThemeColors kDarkTheme = {
  /* bg */          ColorRGBA::fromHex("#060b12"),
  /* glowA */       ColorRGBA(74.0f / 255.0f, 140.0f / 255.0f, 255.0f / 255.0f, 0.13f),
  /* glowB */       ColorRGBA(245.0f / 255.0f, 184.0f / 255.0f, 61.0f / 255.0f, 0.07f),
  /* surface */     ColorRGBA::fromHex("#0c1520"),
  /* surface2 */    ColorRGBA::fromHex("#111d2b"),
  /* surface3 */    ColorRGBA::fromHex("#182839"),
  /* line */        ColorRGBA::fromHex("#1c2d40"),
  /* lineStrong */  ColorRGBA::fromHex("#2b4260"),
  /* ink */         ColorRGBA::fromHex("#e8eef5"),
  /* ink2 */        ColorRGBA::fromHex("#b9c7d6"),
  /* muted */       ColorRGBA::fromHex("#8195aa"),
  /* gold */        ColorRGBA::fromHex("#f5b83d"),
  /* goldInk */     ColorRGBA::fromHex("#1a1203"),
  /* goldText */    ColorRGBA::fromHex("#f5c563"),
  /* goldSoft */    ColorRGBA(245.0f / 255.0f, 184.0f / 255.0f, 61.0f / 255.0f, 0.14f),
  /* blue */        ColorRGBA::fromHex("#52abff"),
  /* green */       ColorRGBA::fromHex("#37d39a"),
  /* red */         ColorRGBA::fromHex("#ff5d62"),
  /* purple */      ColorRGBA::fromHex("#a88bfa"),
  /* orange */      ColorRGBA::fromHex("#ff9f43"),
  /* series1 */     ColorRGBA::fromHex("#3987e5"),
  /* series2 */     ColorRGBA::fromHex("#d95926"),
  /* boardBg */     ColorRGBA::fromHex("#0a1622"),
  /* boardBg2 */    ColorRGBA::fromHex("#0f2031"),
  /* boardInk */    ColorRGBA::fromHex("#f2f6fa"),
  /* boardMuted */  ColorRGBA::fromHex("#93a7bb"),
  /* boardLine */   ColorRGBA::fromHex("#203650"),
  /* navBg */       ColorRGBA(6.0f / 255.0f, 11.0f / 255.0f, 18.0f / 255.0f, 0.82f)
};

static const ThemeColors kLightTheme = {
  /* bg */          ColorRGBA::fromHex("#eef2f7"),
  /* glowA */       ColorRGBA(74.0f / 255.0f, 140.0f / 255.0f, 255.0f / 255.0f, 0.10f),
  /* glowB */       ColorRGBA(245.0f / 255.0f, 184.0f / 255.0f, 61.0f / 255.0f, 0.10f),
  /* surface */     ColorRGBA::fromHex("#ffffff"),
  /* surface2 */    ColorRGBA::fromHex("#f4f7fa"),
  /* surface3 */    ColorRGBA::fromHex("#e8eef4"),
  /* line */        ColorRGBA::fromHex("#dde5ee"),
  /* lineStrong */  ColorRGBA::fromHex("#c3d0dd"),
  /* ink */         ColorRGBA::fromHex("#0c1724"),
  /* ink2 */        ColorRGBA::fromHex("#33475b"),
  /* muted */       ColorRGBA::fromHex("#5b6f84"),
  /* gold */        ColorRGBA::fromHex("#f0ad2a"),
  /* goldInk */     ColorRGBA::fromHex("#1a1203"),
  /* goldText */    ColorRGBA::fromHex("#8f5d00"),
  /* goldSoft */    ColorRGBA(240.0f / 255.0f, 173.0f / 255.0f, 42.0f / 255.0f, 0.16f),
  /* blue */        ColorRGBA::fromHex("#1c6fd0"),
  /* green */       ColorRGBA::fromHex("#0c9563"),
  /* red */         ColorRGBA::fromHex("#d6373d"),
  /* purple */      ColorRGBA::fromHex("#6a4bd4"),
  /* orange */      ColorRGBA::fromHex("#c86a0c"),
  /* series1 */     ColorRGBA::fromHex("#2a78d6"),
  /* series2 */     ColorRGBA::fromHex("#eb6834"),
  /* boardBg */     ColorRGBA::fromHex("#0a1622"), // Scoreboard stays dark
  /* boardBg2 */    ColorRGBA::fromHex("#0f2031"),
  /* boardInk */    ColorRGBA::fromHex("#f2f6fa"),
  /* boardMuted */  ColorRGBA::fromHex("#93a7bb"),
  /* boardLine */   ColorRGBA::fromHex("#203650"),
  /* navBg */       ColorRGBA(238.0f / 255.0f, 242.0f / 255.0f, 247.0f / 255.0f, 0.86f)
};

static bool s_isDark = true;
static ThemeMetrics s_metrics;
static FontAtlas s_fontAtlas;

const ThemeColors& darkTheme() { return kDarkTheme; }
const ThemeColors& lightTheme() { return kLightTheme; }

const ThemeColors& currentTheme() {
  return s_isDark ? kDarkTheme : kLightTheme;
}

void setTheme(bool isDark) {
  s_isDark = isDark;
}

bool isDarkTheme() {
  return s_isDark;
}

const ThemeMetrics& metrics() {
  return s_metrics;
}

FontAtlas& getFontAtlas() {
  return s_fontAtlas;
}

void* getFont(FontRole role) {
  int idx = static_cast<int>(role);
  if (idx >= 0 && idx < static_cast<int>(FontRole::COUNT)) {
    return s_fontAtlas.fonts[idx];
  }
  return nullptr;
}

namespace {

// Glyphs Barlow doesn't have (₹ ▲ ▼ ★) are drawn into the font atlas as custom glyphs, so money
// and sort arrows render the same on every system without another font file.
using Pt = ImVec2;
struct GlyphShape {
  std::vector<std::vector<Pt>> strokes;  // polylines in a 0..1 box
  std::vector<Pt> fill;                  // a polygon in a 0..1 box (even-odd)
  float widthEm, heightEm, strokeEm;
};

float segDist(Pt p, Pt a, Pt b) {
  const float vx = b.x - a.x, vy = b.y - a.y, wx = p.x - a.x, wy = p.y - a.y;
  const float len2 = vx * vx + vy * vy;
  const float t = len2 > 0 ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
  const float dx = p.x - (a.x + t * vx), dy = p.y - (a.y + t * vy);
  return std::sqrt(dx * dx + dy * dy);
}

bool insidePoly(Pt p, const std::vector<Pt>& poly) {
  bool in = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
        p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
      in = !in;
  }
  return in;
}

GlyphShape rupee() {
  GlyphShape g;
  g.widthEm = 0.46f;
  g.heightEm = 0.63f;
  g.strokeEm = 0.07f;
  g.strokes.push_back({{0.06f, 0.07f}, {0.94f, 0.07f}});
  g.strokes.push_back({{0.06f, 0.33f}, {0.94f, 0.33f}});
  std::vector<Pt> bowl;
  for (int i = 0; i <= 16; ++i) {
    const float a = -1.5708f + 3.14159f * i / 16.0f;
    bowl.push_back({0.36f + 0.40f * std::cos(a), 0.32f + 0.25f * std::sin(a)});
  }
  bowl.push_back({0.06f, 0.57f});
  g.strokes.push_back(bowl);
  g.strokes.push_back({{0.14f, 0.57f}, {0.84f, 0.96f}});
  return g;
}

GlyphShape triangle(bool up) {
  GlyphShape g;
  g.widthEm = 0.50f;
  g.heightEm = 0.42f;
  g.strokeEm = 0;
  g.fill = up ? std::vector<Pt>{{0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}} : std::vector<Pt>{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};
  return g;
}

GlyphShape star() {
  GlyphShape g;
  g.widthEm = 0.72f;
  g.heightEm = 0.70f;
  g.strokeEm = 0;
  for (int i = 0; i < 10; ++i) {
    const float a = -1.5708f + 3.14159f * i / 5.0f, r = i % 2 == 0 ? 0.5f : 0.2f;
    g.fill.push_back({0.5f + r * std::cos(a), 0.53f + r * std::sin(a)});
  }
  return g;
}

void rasterise(const GlyphShape& g, unsigned char* px, int pitch, int x0, int y0, int w, int h) {
  const float t = g.strokeEm > 0 ? std::max(1.1f, g.strokeEm * static_cast<float>(h) / g.heightEm) : 0.0f;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float cover = 0;
      if (!g.strokes.empty()) {
        const Pt p(x + 0.5f, y + 0.5f);
        float d = 1e9f;
        for (const auto& line : g.strokes)
          for (size_t i = 1; i < line.size(); ++i)
            d = std::min(d, segDist(p, Pt(line[i - 1].x * (w - 1), line[i - 1].y * (h - 1)), Pt(line[i].x * (w - 1), line[i].y * (h - 1))));
        cover = std::clamp(t * 0.5f - d + 0.5f, 0.0f, 1.0f);
      }
      if (!g.fill.empty()) {
        int hits = 0;
        for (int sy = 0; sy < 4; ++sy)
          for (int sx = 0; sx < 4; ++sx)
            hits += insidePoly(Pt((x + (sx + 0.5f) / 4) / w, (y + (sy + 0.5f) / 4) / h), g.fill) ? 1 : 0;
        cover = std::max(cover, hits / 16.0f);
      }
      px[(y0 + y) * pitch + x0 + x] = static_cast<unsigned char>(cover * 255.0f + 0.5f);
    }
}

void addMissingGlyphs() {
  ImFontAtlas* atlas = ImGui::GetIO().Fonts;
  atlas->Build();
  struct Job {
    int rect;
    GlyphShape shape;
  };
  std::vector<Job> jobs;
  const std::pair<ImWchar, GlyphShape> wanted[] = {{0x20B9, rupee()}, {0x25B2, triangle(true)}, {0x25BC, triangle(false)}, {0x2605, star()}};
  std::vector<ImFont*> seen;
  for (void* fp : s_fontAtlas.fonts) {
    ImFont* f = static_cast<ImFont*>(fp);
    if (!f || std::find(seen.begin(), seen.end(), f) != seen.end()) continue;
    seen.push_back(f);
    for (const auto& [cp, shape] : wanted) {
      if (f->FindGlyphNoFallback(cp)) continue;
      const float em = f->FontSize;
      const int w = std::max(3, static_cast<int>(std::lround(shape.widthEm * em)));
      const int h = std::max(3, static_cast<int>(std::lround(shape.heightEm * em)));
      // sits on the baseline, cap height tall (triangles centred on the x-height)
      const float top = shape.fill.empty() || shape.heightEm > 0.6f ? f->Ascent - h - em * 0.01f : f->Ascent - em * 0.52f;
      const int id = atlas->AddCustomRectFontGlyph(f, cp, w, h, w + em * 0.08f, ImVec2(em * 0.03f, top));
      jobs.push_back({id, shape});
    }
  }
  if (jobs.empty()) return;
  atlas->Build();
  unsigned char* pixels = nullptr;
  int tw = 0, th = 0;
  atlas->GetTexDataAsAlpha8(&pixels, &tw, &th);
  for (const Job& j : jobs) {
    const ImFontAtlasCustomRect* r = atlas->GetCustomRectByIndex(j.rect);
    if (r && r->IsPacked()) rasterise(j.shape, pixels, tw, r->X, r->Y, r->Width, r->Height);
  }
}

}  // namespace

void loadFonts(float dpiScale) {
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();
  s_fontAtlas.dpiScale = dpiScale;

  struct Spec {
    FontRole role;
    const char* face;
    float px;
  };
  // Sizes from 08-ui-design-system.md (CSS px)
  static const Spec specs[] = {
      {FontRole::Body, "Barlow-Regular", 15.0f},
      {FontRole::Hint, "Barlow-Regular", 13.5f},
      {FontRole::Eyebrow, "Barlow-Bold", 11.5f},
      {FontRole::PanelTitle, "BarlowCondensed-ExtraBoldItalic", 20.0f},
      {FontRole::H1, "BarlowCondensed-ExtraBoldItalic", 36.0f},
      {FontRole::HeroH1, "BarlowCondensed-ExtraBoldItalic", 54.0f},
      {FontRole::H2, "BarlowCondensed-Bold", 23.0f},
      {FontRole::H3, "BarlowCondensed-Bold", 18.0f},
      {FontRole::ScoreboardScore, "BarlowCondensed-ExtraBoldItalic", 56.0f},
      {FontRole::BoardStat, "BarlowCondensed-ExtraBold", 28.0f},
      {FontRole::Kpi, "BarlowCondensed-ExtraBoldItalic", 32.0f},
      {FontRole::Moment, "BarlowCondensed-ExtraBoldItalic", 44.0f},
      {FontRole::BodyStrong, "Barlow-SemiBold", 15.0f},
      {FontRole::BoardLabel, "Barlow-Bold", 10.5f},
  };
  // Latin + punctuation (• – · ▲ ▼ ₹)
  static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x20B9, 0x20B9, 0x25B2, 0x25BC, 0};

  for (const Spec& sp : specs) {
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;  // the bytes live in the exe
    cfg.OversampleH = 2;
    cfg.PixelSnapH = false;
    const EmbeddedFont ef = embeddedFont(sp.face);
    ImFont* f = nullptr;
    if (ef.data) {
      f = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(ef.data), static_cast<int>(ef.size),
                                         sp.px * dpiScale, &cfg, ranges);
    }
    if (!f) f = io.Fonts->AddFontDefault();
    s_fontAtlas.fonts[static_cast<int>(sp.role)] = f;
  }
  io.FontDefault = static_cast<ImFont*>(s_fontAtlas.fonts[static_cast<int>(FontRole::Body)]);
  addMissingGlyphs();
}

void applyImGuiStyle() {
  const ThemeColors& th = currentTheme();
  const ThemeMetrics& m = s_metrics;
  ImGuiStyle& st = ImGui::GetStyle();
  st = ImGuiStyle();
  st.WindowRounding = 0.0f;
  st.ChildRounding = m.panelRadius;
  st.FrameRounding = m.buttonRadius;
  st.PopupRounding = m.smallRadius;
  st.GrabRounding = 6.0f;
  st.TabRounding = 8.0f;
  st.ScrollbarRounding = 8.0f;
  st.WindowBorderSize = 0.0f;
  st.ChildBorderSize = 1.0f;
  st.FrameBorderSize = 1.0f;
  st.WindowPadding = ImVec2(0, 0);
  st.FramePadding = ImVec2(m.buttonPaddingX, 10.0f);
  st.ItemSpacing = ImVec2(10.0f, 10.0f);
  st.ScrollbarSize = 12.0f;

  auto c = [](const ColorRGBA& x, float a = 1.0f) { return ImVec4(x.r, x.g, x.b, x.a * a); };
  ImVec4* k = st.Colors;
  k[ImGuiCol_Text] = c(th.ink);
  k[ImGuiCol_TextDisabled] = c(th.muted);
  k[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
  k[ImGuiCol_ChildBg] = c(th.surface);
  k[ImGuiCol_PopupBg] = c(th.surface2);
  k[ImGuiCol_Border] = c(th.line);
  k[ImGuiCol_FrameBg] = c(th.surface2);
  k[ImGuiCol_FrameBgHovered] = c(th.surface3);
  k[ImGuiCol_FrameBgActive] = c(th.surface3);
  k[ImGuiCol_Button] = c(th.surface2);
  k[ImGuiCol_ButtonHovered] = c(th.surface3);
  k[ImGuiCol_ButtonActive] = c(th.lineStrong);
  k[ImGuiCol_Header] = c(th.goldSoft);
  k[ImGuiCol_HeaderHovered] = c(th.surface3);
  k[ImGuiCol_HeaderActive] = c(th.goldSoft);
  k[ImGuiCol_CheckMark] = c(th.gold);
  k[ImGuiCol_SliderGrab] = c(th.gold);
  k[ImGuiCol_SliderGrabActive] = c(th.gold);
  k[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  k[ImGuiCol_ScrollbarGrab] = c(th.lineStrong);
  k[ImGuiCol_ScrollbarGrabHovered] = c(th.muted);
  k[ImGuiCol_ScrollbarGrabActive] = c(th.ink2);
  k[ImGuiCol_Separator] = c(th.line);
  k[ImGuiCol_TableHeaderBg] = c(th.surface2);
  k[ImGuiCol_TableBorderStrong] = c(th.line);
  k[ImGuiCol_TableBorderLight] = c(th.line);
  k[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  k[ImGuiCol_TableRowBgAlt] = c(th.surface2, 0.5f);
  k[ImGuiCol_ModalWindowDimBg] = ImVec4(3 / 255.0f, 7 / 255.0f, 12 / 255.0f, 0.72f);
}

void drawBackgroundGlows(void* drawList, float screenWidth, float screenHeight) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  const ThemeColors& th = currentTheme();
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(screenWidth, screenHeight), col(th.bg));
  // body: radial-gradient(1200px 600px at 10% -10%, blue) + (900px 500px at 100% 0%, gold)
  radialGlow(dl, ImVec2(screenWidth * 0.10f, -screenHeight * 0.10f), 900.0f, col(th.glowA));
  radialGlow(dl, ImVec2(screenWidth, 0.0f), 700.0f, col(th.glowB));
}

void drawPanelShadow(void* drawList, float x1, float y1, float x2, float y2, float radius, bool large) {
  auto* dl = static_cast<ImDrawList*>(drawList);
  if (!dl) return;
  static const float smallOff[] = {2, 4, 8, 12}, smallA[] = {0.12f, 0.08f, 0.05f, 0.03f};
  static const float largeOff[] = {4, 10, 18, 24}, largeA[] = {0.15f, 0.10f, 0.07f, 0.04f};
  const float* off = large ? largeOff : smallOff;
  const float* al = large ? largeA : smallA;
  for (int i = 3; i >= 0; --i) {
    const float o = off[i];
    dl->AddRectFilled(ImVec2(x1 - o * 0.3f, y1 + o * 0.5f), ImVec2(x2 + o * 0.3f, y2 + o), IM_COL32(0, 0, 0, static_cast<int>(al[i] * 255)),
                      radius + o * 0.5f);
  }
}

} // namespace t20::ui
