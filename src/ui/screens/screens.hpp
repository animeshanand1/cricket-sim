#pragma once
// Every screen of the game (A6): Home, Playing XI, Training, Toss, Match, Season, Off-season,
// Sacked, Club, Players, and the player card. registerScreens() puts them all in the shell.

#include <memory>

#include "ui/app_shell.hpp"
#include "ui/screens/ui_context.hpp"

namespace t20::ui {

std::unique_ptr<IScreenView> makeHomeScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeXiScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeTrainingScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeTossScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeSeasonScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeOffseasonScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeSackedScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makeClubScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makePlayersScreen(UiContext& ctx);
std::unique_ptr<IScreenView> makePlayerCard(UiContext& ctx);

void registerScreens(AppShell& shell, UiContext& ctx);

}  // namespace t20::ui
