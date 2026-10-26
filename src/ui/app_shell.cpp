#include "ui/app_shell.hpp"
#include "ui/theme.hpp"
#include "ui/gfx.hpp"
#include "ui/anim.hpp"
#include "ui/widgets/stepper.hpp"
#include "ui/widgets/modal.hpp"
#include "ui/screens/ui_context.hpp"
#include "engine/season.hpp"
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
    case Screen::gallery:
      return View::gallery;
    case Screen::home:
    default:
      return View::home;
  }
}

void AppShell::enter(Screen screen) {
  const View view = resolveView(screen, m_ctx ? m_ctx->hasMatch() : m_hasMatch);
  if (m_screen == screen && m_view == view) return;
  m_screen = screen;
  const View prevView = m_view;
  m_view = view;
  // A new view starts at the top and gets its onEnter
  if (prevView != m_view) {
    m_scrollTop = true;
    auto it = m_views.find(static_cast<int>(m_view));
    if (it != m_views.end() && it->second) it->second->onEnter();
  }
}

void AppShell::goTo(Screen screen) {
  if (m_ctx) {
    m_ctx->goTo(screen);
    syncFromGame();
    return;
  }
  enter(screen);
}

void AppShell::openPlayers() {
  if (m_ctx) {
    m_ctx->game.openPlayers();
    syncFromGame();
    return;
  }
  m_returnTo = m_screen;
  enter(Screen::players);
}

void AppShell::closePlayers() {
  if (m_ctx) {
    m_ctx->game.closePlayers();
    syncFromGame();
    return;
  }
  enter(m_returnTo);
}

void AppShell::openPlayerCard(const std::string& playerId) {
  if (m_ctx) {
    m_ctx->openCard(playerId);
    return;
  }
  m_cardPlayerId = playerId;
  startTween("modal_in", 0.0f, 1.0f, 0.22, EaseType::EaseOutQuad);
}

void AppShell::closePlayerCard() {
  if (m_ctx) m_ctx->game.closeCard();
  m_cardPlayerId.clear();
}

void AppShell::syncFromGame() {
  if (!m_ctx) return;
  const auto& st = m_ctx->game.state();
  m_hasMatch = m_ctx->hasMatch();
  m_isMatchLive = m_ctx->matchIsLive();
  m_cardPlayerId = st.ui.cardPlayerId.value_or("");
  // The view also changes when a match appears or goes (toss / match need one)
  enter(m_ctx->screen());
  // Season · position · points in the nav bar
  static uint64_t builtAt = UINT64_MAX;
  if (builtAt != m_ctx->revision()) {
    builtAt = m_ctx->revision();
    m_seasonInfo.reset();
    if (const auto& season = st.season.season) {
      SeasonNavInfo info;
      info.seasonNumber = season->number;
      for (const auto& r : pointsTable(*season))
        if (r.team == season->userTeam) {
          info.tablePosition = r.pos;
          info.points = r.pts;
        }
      m_seasonInfo = info;
    }
  }
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

  syncFromGame();

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
        if (m_ctx && m_ctx->game.state().world.manager.sacked) goTo(Screen::sacked);
        else if (m_ctx && m_ctx->game.state().world.offseason) goTo(Screen::offseason);
        else goTo(Screen::season);
        break;
      case View::club:
        goTo(Screen::club);
        break;
      case View::match:
        if (m_ctx && !m_ctx->session.hasMatch() && m_ctx->game.state().match &&
            m_ctx->game.state().match->status == MatchStatus::toss) {
          goTo(Screen::toss);
        } else if (m_hasMatch) {
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
  if (m_scrollTop) {
    ImGui::SetScrollY(0);
    m_scrollTop = false;
  }
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
  if (beginModal("##player_card", &isOpen, nullptr, 820.0f)) {
    if (m_card) m_card->draw(std::min(820.0f, ImGui::GetIO().DisplaySize.x - 48.0f) - 44.0f);
    endModal();
  }
  if (!isOpen) closePlayerCard();
}

} // namespace t20::ui
