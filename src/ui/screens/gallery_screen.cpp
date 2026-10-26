#include "ui/screens/gallery_screen.hpp"
#include "ui/theme.hpp"
#include "ui/format.hpp"
#include "ui/icons.hpp"
#include "ui/widgets/team_badge.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/tags.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/sortable_table.hpp"
#include "ui/match/scoreboard.hpp"
#include "ui/match/moment_banner.hpp"
#include "ui/match/field_view.hpp"
#include "ui/match/win_and_fans.hpp"

namespace t20::ui {

void GalleryScreen::onEnter() {
  m_activeTab = 0;
}

void GalleryScreen::draw(float contentWidth) {
  // Page Header
  TeamMeta demoTeam;
  demoTeam.id = "CHE";
  demoTeam.name = "Chennai Super Kings";
  demoTeam.color = "#f5b83d";

  pageHeader("UI Design System Gallery",
             "Night-broadcast theme, shared widgets, and custom ImDrawList graphics",
             "T20 COACH · C++ MIGRATION",
             &demoTeam);

  // Tabs: Identity & Badges | Values & Meters | Controls & Panels | Scoreboard & Match
  std::vector<TabItem> tabsList = {
    { "identity", "Identity", "trophy" },
    { "values",   "Values & Status", "chart" },
    { "controls", "Controls & Panels", "list" },
    { "match",    "Match Graphics", "ball" }
  };
  m_activeTab = tabs(tabsList, m_activeTab);

  if (m_activeTab == 0) {
    // Identity Section: Team Badges and Player Avatars
    if (beginPanel("identity_panel", "Team Badges & Player Avatars")) {
      TeamMeta teams[3] = {
        {"CHE", "Chennai", "CSK", "#f5b83d", "#0081e8"},
        {"MUM", "Mumbai", "MI", "#004ba0", "#d1ab3e"},
        {"KOL", "Kolkata", "KKR", "#3a225d", "#f3b429"}
      };
      for (const auto& tm : teams) {
        teamBadge(tm, 56.0f);
      }

      PlayerMeta samplePlayer;
      samplePlayer.id = "p1";
      samplePlayer.name = "Jasprit Bumrah";
      samplePlayer.role = Role::BOWL;
      samplePlayer.fitness = 82.0f;
      avatar(samplePlayer, "#004ba0", 40.0f, true);

      endPanel();
    }
  } else if (m_activeTab == 1) {
    // Values & Status Section: Meters, Ratings, Tags, Pips
    if (beginPanel("values_panel", "Values, Meters & Status Pills")) {
      meter(85.0f, "Confidence 85 (In the zone)");
      meter(55.0f, "Rhythm 55 (In rhythm)");
      meter(30.0f, "Fitness 30 (Exhausted)");

      rating(88, "Elite skill rating");
      rating(65, "Solid performer");
      rating(45, "Developing talent");

      phaseTag(22); // Rising
      phaseTag(28); // Prime
      phaseTag(31); // Experienced
      phaseTag(34); // Veteran
      phaseTag(37); // Twilight

      osTag();
      qTag();
      ipTag();

      styleChip("RHB");
      styleChip("148 kph");
      styleChip("Knuckleball", true);

      endPanel();
    }
  } else if (m_activeTab == 2) {
    // Controls & Panels Section
    if (beginPanel("controls_panel", "Buttons, Switches & Notifications")) {
      button("Primary Action", ButtonKind::Primary);
      button("Standard Action", ButtonKind::Default);
      button("Quiet Action", ButtonKind::Quiet);

      switchToggle("Night Broadcast Dark Theme", &m_switchState);

      notice("Match saved successfully to local storage", NoticeKind::Ok);
      notice("Two-ball rule in effect due to dew", NoticeKind::Warn);

      endPanel();
    }
  } else if (m_activeTab == 3) {
    // Match Graphics Section: Scoreboard, Field View, Win Bar
    if (beginPanel("match_panel", "Broadcast Scoreboard & Field Canvas")) {
      ScoreboardViewData sbData;
      sbData.battingTeam = demoTeam;
      sbData.runs = 178;
      sbData.wickets = 3;
      sbData.balls = 112; // 18.4 ov
      sbData.headline = "CHENNAI NEED 12 RUNS IN 8 BALLS";
      sbData.striker = {"R. Jadeja", 38, 19, 3, 2, true};
      sbData.nonStriker = {"M.S. Dhoni", 18, 9, 1, 1, false};
      sbData.currentBowler = {"J. Bumrah", 22, 0, 24, 2};
      drawScoreboard(nullptr, sbData, contentWidth);

      winBar(64.0f, "CHE", "MUM");

      FieldViewData fvData;
      fvData.isLeftHander = false;
      drawFieldView(nullptr, fvData, 420.0f);

      endPanel();
    }
  }
}

} // namespace t20::ui
