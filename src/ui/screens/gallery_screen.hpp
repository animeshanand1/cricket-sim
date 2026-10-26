#pragma once

#include "ui/app_shell.hpp"

namespace t20::ui {

// Design system & UI widget gallery screen showing all tokens, widgets, and custom graphics
class GalleryScreen : public IScreenView {
public:
  GalleryScreen() = default;
  ~GalleryScreen() override = default;

  void onEnter() override;
  void draw(float contentWidth) override;

private:
  int m_activeTab = 0;
  bool m_switchState = true;
};

} // namespace t20::ui
