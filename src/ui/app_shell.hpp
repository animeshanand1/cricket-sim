#pragma once

#include "core/types.hpp"
#include "ui/widgets/nav_bar.hpp"
#include <string>
#include <optional>
#include <memory>
#include <unordered_map>

namespace t20::ui {

// Abstract screen view interface
class IScreenView {
public:
  virtual ~IScreenView() = default;
  virtual void onEnter() {}
  virtual void draw(float contentWidth) = 0;
};

// Application shell and screen view state machine
class AppShell {
public:
  AppShell();
  ~AppShell() = default;

  // Resolves setup.screen to the appropriate high-level view
  static View resolveView(Screen screen, bool hasMatch);

  // Navigation commands matching the user's screen view architecture
  void goTo(Screen screen);
  void openPlayers();
  void closePlayers();
  void openPlayerCard(const std::string& playerId);
  void closePlayerCard();

  // State inspection
  Screen currentScreen() const { return m_screen; }
  View currentView() const { return m_view; }
  const std::string& cardPlayerId() const { return m_cardPlayerId; }

  // Game state the nav bar reflects
  void setMatchState(bool hasMatch, bool isLive) {
    m_hasMatch = hasMatch;
    m_isMatchLive = isLive;
  }
  void setSeasonInfo(std::optional<SeasonNavInfo> info) { m_seasonInfo = info; }

  // Main per-frame drawing routine
  void renderFrame(float viewportWidth, float viewportHeight, double deltaTime);

  // Register screen views
  void registerView(View view, std::unique_ptr<IScreenView> screen);

private:
  Screen m_screen = Screen::home;
  View m_view = View::home;
  Screen m_returnTo = Screen::home; // return target for universal Players screen
  std::string m_cardPlayerId;       // open player modal id (empty if closed)
  bool m_hasMatch = false;
  bool m_isMatchLive = false;
  std::optional<SeasonNavInfo> m_seasonInfo;

  std::unordered_map<int, std::unique_ptr<IScreenView>> m_views;

  void drawNavBar(float viewportWidth);
  void drawCurrentView(float viewportWidth, float viewportHeight);
  void drawPlayerCardModal();
};

} // namespace t20::ui
