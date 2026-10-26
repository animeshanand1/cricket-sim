#include "ui/app_shell.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/anim.hpp"
#include "ui/widgets/stepper.hpp"
#include "ui/widgets/modal.hpp"
#include <algorithm>

namespace t20::ui {

AppShell::AppShell() {
  m_screen = Screen::home;
  m_view = View::home;
}

View AppShell::resolveView(Screen screen, bool hasMatch) {
  switch (screen) {
    case Screen::draft:
    case Screen::offseason:
      return View::offseason;
    case Screen::club:
      return View::club;
    case Screen::sacked:
      return View::sacked;
    case Screen::season:
      return View::season;
    case Screen::players:
      return View::players;
    case Screen::match:
      return hasMatch ? View::match : View::home;
    case Screen::toss:
      return hasMatch ? View::toss : View::home;
    case Screen::xi:
      return View::xi;
    case Screen::training:
      return View::training;
    case Screen::home:
    default:
      return View::home;
  }
}

void AppShell::goTo(Screen screen) {
  if (m_screen == screen) return;
  m_screen = screen;
  View prevView = m_view;
  m_view = resolveView(m_screen, m_hasMatch);

  // When view changes, notify onEnter (which also resets scroll to top)
  if (prevView != m_view) {
    auto it = m_views.find(static_cast<int>(m_view));
    if (it != m_views.end() && it->second) {
      it->second->onEnter();
    }
  }
}

void AppShell::openPlayers() {
  m_returnTo = m_screen;
  goTo(Screen::players);
}

void AppShell::closePlayers() {
  goTo(m_returnTo);
}

void AppShell::openPlayerCard(const std::string& playerId) {
  m_cardPlayerId = playerId;
  startTween("modal_in", 0.0f, 1.0f, 0.22, EaseType::EaseOutQuad);
}

void AppShell::closePlayerCard() {
  m_cardPlayerId.clear();
}

void AppShell::registerView(View view, std::unique_ptr<IScreenView> screen) {
  m_views[static_cast<int>(view)] = std::move(screen);
}

void AppShell::renderFrame(float viewportWidth, float viewportHeight, double deltaTime) {
  // 1. Advance animation clock
  static double s_elapsedTime = 0.0;
  s_elapsedTime += deltaTime;
  AnimationManager::instance().update(s_elapsedTime);

  // 2. Background with the two soft radial glows
  drawBackgroundGlows(ImGui::GetBackgroundDrawList(), viewportWidth, viewportHeight);

  // 3. One borderless root window covering the whole SDL window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(viewportWidth, viewportHeight));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##root", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();

  // 4. Sticky NavBar, then the current view in a scrolling, centred max-1240px column
  drawNavBar(viewportWidth);
  drawCurrentView(viewportWidth, viewportHeight - metrics().navBarHeight);

  // 5. Modal host (PlayerCard) if active
  if (!m_cardPlayerId.empty()) {
    drawPlayerCardModal();
  }
  ImGui::End();
}

void AppShell::drawNavBar(float viewportWidth) {
  NavState navState;
  navState.currentView = m_view;
  navState.hasActiveMatch = m_hasMatch;
  navState.isMatchLive = m_isMatchLive;
  navState.seasonInfo = m_seasonInfo;

  std::optional<View> clicked = navBar(navState, viewportWidth);
  if (clicked.has_value()) {
    switch (clicked.value()) {
      case View::home:
        goTo(Screen::home);
        break;
      case View::season:
        goTo(Screen::season);
        break;
      case View::club:
        goTo(Screen::club);
        break;
      case View::match:
        if (m_hasMatch) {
          goTo(Screen::match);
        }
        break;
      case View::players:
        openPlayers();
        break;
      default:
        break;
    }
  }
}

void AppShell::drawCurrentView(float viewportWidth, float viewportHeight) {
  const auto& met = metrics();

  // Content area under NavBar: centred, max 1240px wide, with 28px side padding
  float contentWidth = std::min(viewportWidth - met.desktopSidePadding * 2.0f, met.maxContentWidth);
  float contentX = (viewportWidth - contentWidth) * 0.5f;

  ImGui::BeginChild("##scroll", ImVec2(viewportWidth, viewportHeight), 0, ImGuiWindowFlags_NoBackground);
  ImGui::SetCursorPos(ImVec2(contentX, 24.0f));
  ImGui::BeginChild("##page", ImVec2(contentWidth, 0), ImGuiChildFlags_AutoResizeY,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

  // Setup progress stepper (only visible on xi, training, and toss)
  stepper(m_screen);

  // Dispatch to the active screen view
  auto it = m_views.find(static_cast<int>(m_view));
  if (it != m_views.end() && it->second) {
    it->second->draw(contentWidth);
  }
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0, 40));
  ImGui::EndChild();
}

void AppShell::drawPlayerCardModal() {
  bool isOpen = !m_cardPlayerId.empty();
  if (beginModal("player_card_modal", &isOpen, "Player Card", metrics().maxModalWidth)) {
    // Render player card contents
    endModal();
  }
  if (!isOpen) {
    closePlayerCard();
  }
}

} // namespace t20::ui
