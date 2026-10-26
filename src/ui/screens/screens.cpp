#include "ui/screens/screens.hpp"

#include "ui/screens/match_screen.hpp"

namespace t20::ui {

void registerScreens(AppShell& shell, UiContext& ctx) {
  shell.registerView(View::home, makeHomeScreen(ctx));
  shell.registerView(View::xi, makeXiScreen(ctx));
  shell.registerView(View::training, makeTrainingScreen(ctx));
  shell.registerView(View::toss, makeTossScreen(ctx));
  shell.registerView(View::match, std::make_unique<MatchScreen>(ctx));
  shell.registerView(View::season, makeSeasonScreen(ctx));
  shell.registerView(View::offseason, makeOffseasonScreen(ctx));
  shell.registerView(View::sacked, makeSackedScreen(ctx));
  shell.registerView(View::club, makeClubScreen(ctx));
  shell.registerView(View::players, makePlayersScreen(ctx));
  shell.setPlayerCard(makePlayerCard(ctx));
}

}  // namespace t20::ui
