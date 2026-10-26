#pragma once

#include "core/types.hpp"
#include "ui/widgets/nav_bar.hpp"
#include <string>
#include <optional>
#include <memory>
#include <unordered_map>

namespace t20::ui {

class UiContext;

// Abstract screen view interface
class IScreenView {
public:
  virtual ~IScreenView() = default;
  virtual void onEnter() {}
  virtual void draw(float contentWidth) = 0;
};

// Application shell and screen view state machine. Bound to a UiContext, it follows the Game's
// setup.screen and sends navigation to the Game; unbound (the gallery, tests) it keeps its own.
class AppShell {
public:
  AppShell();
  ~AppShell() = default;

  // Resolves setup.screen to the appropriate high-level view
  static View resolveView(Screen screen, bool hasMatch);

  void bind(UiContext* ctx) { m_ctx = ctx; }

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

  // Game state the nav bar reflects (unbound shells only; a bound shell reads the Game)
  void setMatchState(bool hasMatch, bool isLive) {
    m_hasMatch = hasMatch;
    m_isMatchLive = isLive;
  }
  void setSeasonInfo(std::optional<SeasonNavInfo> info) { m_seasonInfo = info; }

  // Main per-frame drawing routine
  void renderFrame(float viewportWidth, float viewportHeight, double deltaTime);

  // Register screen views; the player card is drawn inside the modal while one is open
  void registerView(View view, std::unique_ptr<IScreenView> screen);
  void setPlayerCard(std::unique_ptr<IScreenView> card) { m_card = std::move(card); }

private:
  Screen m_screen = Screen::home;
  View m_view = View::home;
  Screen m_returnTo = Screen::home; // return target for universal Players screen
  std::string m_cardPlayerId;       // open player modal id (empty if closed)
  bool m_hasMatch = false;
  bool m_isMatchLive = false;
  std::optional<SeasonNavInfo> m_seasonInfo;
  UiContext* m_ctx = nullptr;
  bool m_scrollTop = false;

  std::unordered_map<int, std::unique_ptr<IScreenView>> m_views;
  std::unique_ptr<IScreenView> m_card;

  void syncFromGame();
  void enter(Screen screen);
  void drawNavBar(float viewportWidth);
  void drawCurrentView(float viewportWidth, float viewportHeight);
  void drawPlayerCardModal();
};

} // namespace t20::ui
