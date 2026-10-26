#pragma once
// S05 Match: live play, step 5a (layout, scoreboard, dock) plus the crease, commentary and
// scorecard panels. 5b adds the captain's decisions (captain_panel.cpp), 5c the crease cards and Impact Player.

#include "app/match_session.hpp"
#include "ui/match/captain_panel.hpp"
#include "ui/match/impact_panel.hpp"
#include "ui/app_shell.hpp"

namespace t20::ui {

class MatchScreen : public IScreenView {
 public:
  explicit MatchScreen(app::MatchSession& session) : m_session(session) {}

  void onEnter() override {}
  void draw(float contentWidth) override;

 private:
  void drawHeader();
  void drawDock();
  void drawScoreboardArea(float width);
  void drawCommentary();
  void drawScorecard();
  void handleKeys();
  void consumeEvents();
  void newFixture();

  app::MatchSession& m_session;
  int m_speed = 1;          // Slow / Medium / Fast
  int m_scorecardInnings = -1;  // -1 = follow the current innings
  int m_fixture = 0;
  CaptainPanelState m_captain;
  ImpactPanelState m_impact;
};

}  // namespace t20::ui
