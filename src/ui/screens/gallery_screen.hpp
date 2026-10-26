#pragma once
// A6: the widget gallery. Every widget in the design system (08-ui-design-system.md) on one
// screen, in five tabs: identity, values and status, controls and forms, tables and panels, and
// the match graphics. Open it with `t20coach --screen gallery`.

#include <memory>
#include <string>

#include "app/match_session.hpp"
#include "ui/app_shell.hpp"
#include "ui/widgets/sortable_table.hpp"

namespace t20::ui {

class GalleryScreen : public IScreenView {
 public:
  GalleryScreen() = default;
  ~GalleryScreen() override = default;

  void onEnter() override;
  void draw(float contentWidth) override;

 private:
  void drawIdentity(float width);
  void drawValues(float width);
  void drawControls(float width);
  void drawTables(float width);
  void drawMatch(float width);

  int m_activeTab = 0;
  bool m_switchState = true, m_check = true, m_modal = false, m_fold = false;
  int m_chip = 1, m_segment = 1, m_select = -1, m_stepper = 3, m_slider = 72;
  double m_money = 250;
  std::string m_text = "Mumbay Islanders";
  SortableTableState m_table;
  std::unique_ptr<app::MatchSession> m_demo;  // a simmed match for the scoreboard and charts
};

}  // namespace t20::ui
