#pragma once
// S05 Match: live play, step 5a (layout, scoreboard, dock) plus the crease, commentary and
// scorecard panels. 5b adds the captain's decisions (captain_panel.cpp), 5c the crease cards and Impact Player.

#include "app/match_session.hpp"
#include <set>
#include <string>

#include "ui/match/captain_panel.hpp"
#include "ui/match/commentary.hpp"
#include "ui/match/field_view.hpp"
#include "ui/match/impact_panel.hpp"
#include "ui/match/result_panels.hpp"
#include "ui/app_shell.hpp"

namespace t20::ui {

class UiContext;

class MatchScreen : public IScreenView {
 public:
  explicit MatchScreen(app::MatchSession& session) : m_session(session) {}
  // Bound to the game: season fixtures and friendlies set up on the Home screen
  explicit MatchScreen(UiContext& ctx);

  void onEnter() override;
  void draw(float contentWidth) override;

 private:
  void drawHeader();
  void drawDock();
  void drawScoreboardArea(float width);
  void drawCommentary();
  void drawDetails();
  void drawScorecard(int innings);
  void handleKeys();
  void consumeEvents();
  void newFixture();
  std::vector<MatchExit> exits();

  app::MatchSession& m_session;
  UiContext* m_ctx = nullptr;
  int m_speed = 1;          // Slow / Medium / Fast
  int m_scorecardInnings = -1;  // -1 = follow the current innings
  int m_fixture = 0;
  CaptainPanelState m_captain;
  ImpactPanelState m_impact;
  FieldViewState m_field;
  CommentaryState m_comment;
  DevReportState m_dev;
  int m_detailTab = 0;
  // moment banners: what was already on screen
  std::string m_momentMatch;
  int m_momentInnings = 0;
  size_t m_momentLines = 0;
  std::set<std::string> m_reached;
};

}  // namespace t20::ui
