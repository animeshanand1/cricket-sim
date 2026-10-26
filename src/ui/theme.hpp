#pragma once

#include "core/color.hpp"
#include <string>

namespace t20::ui {

struct ThemeColors {
  ColorRGBA bg;
  ColorRGBA glowA;
  ColorRGBA glowB;
  ColorRGBA surface;
  ColorRGBA surface2;
  ColorRGBA surface3;
  ColorRGBA line;
  ColorRGBA lineStrong;
  ColorRGBA ink;
  ColorRGBA ink2;
  ColorRGBA muted;
  ColorRGBA gold;
  ColorRGBA goldInk;
  ColorRGBA goldText;
  ColorRGBA goldSoft;
  ColorRGBA blue;
  ColorRGBA green;
  ColorRGBA red;
  ColorRGBA purple;
  ColorRGBA orange;
  ColorRGBA series1;
  ColorRGBA series2;
  ColorRGBA boardBg;
  ColorRGBA boardBg2;
  ColorRGBA boardInk;
  ColorRGBA boardMuted;
  ColorRGBA boardLine;
  ColorRGBA navBg;
};

// Returns the active theme palette (Dark is default)
const ThemeColors& darkTheme();
const ThemeColors& lightTheme();
const ThemeColors& currentTheme();
void setTheme(bool isDark);
bool isDarkTheme();

struct ThemeMetrics {
  float panelRadius = 14.0f;
  float smallRadius = 9.0f;
  float scoreboardRadius = 18.0f;
  float modalRadius = 18.0f;

  float buttonMinHeight = 42.0f;
  float buttonSmallHeight = 34.0f;
  float buttonRadius = 10.0f;
  float buttonPaddingX = 18.0f;

  float chipMinHeight = 38.0f;
  float chipSmallHeight = 30.0f;

  float stackGap = 12.0f;
  float largeStackGap = 26.0f;

  float maxContentWidth = 1240.0f;
  float desktopSidePadding = 28.0f;
  float maxModalWidth = 720.0f;

  float navBarHeight = 56.0f;
};

const ThemeMetrics& metrics();

enum class FontRole {
  Body,           // Barlow 400/500/600 15px
  Hint,           // Barlow 400 13.5px
  Eyebrow,        // Barlow 700 11.5px (uppercase, tracked)
  PanelTitle,     // Barlow Condensed 800 italic 20px
  H1,             // Barlow Condensed 800 italic 36px
  HeroH1,         // Barlow Condensed 800 italic 54px
  H2,             // Barlow Condensed 700 23px
  H3,             // Barlow Condensed 700 18px
  ScoreboardScore,// Barlow Condensed 800 italic 56px (tabular)
  BoardStat,      // Barlow Condensed 800 28px
  Kpi,            // Barlow Condensed 800 italic 32px
  Moment,         // Barlow Condensed 800 italic 44px
  BodyStrong,     // Barlow 600 15px (names, buttons)
  BoardLabel,     // Barlow 700 10.5px (uppercase labels on the scoreboard)
  COUNT
};

// Opaque font pointer holder compatible with ImFont*
struct FontAtlas {
  void* fonts[static_cast<int>(FontRole::COUNT)] = {nullptr};
  float dpiScale = 1.0f;
};

FontAtlas& getFontAtlas();
void* getFont(FontRole role);

// Load the Barlow / Barlow Condensed faces (embedded in the exe) into the ImGui atlas, one per role.
// Call once after ImGui::CreateContext().
void loadFonts(float dpiScale = 1.0f);

// Map the theme tokens onto ImGui's style (call after loadFonts and after every setTheme)
void applyImGuiStyle();

// Background radial glow rendering for windows
void drawBackgroundGlows(void* drawList, float screenWidth, float screenHeight);

// Draw soft panel drop shadow (3-4 translucent stacked rounded rects)
void drawPanelShadow(void* drawList, float x1, float y1, float x2, float y2, float radius, bool large = false);

} // namespace t20::ui
