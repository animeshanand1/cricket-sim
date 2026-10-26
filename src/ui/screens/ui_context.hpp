#pragma once
// What every screen works with: the Game (state and actions), the live MatchSession, and caches of
// what the screens derive from the state (the developed league, every player), rebuilt only when
// the Game's revision moves.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/game.hpp"
#include "app/match_session.hpp"
#include "core/types.hpp"

namespace t20::ui {

class UiContext {
 public:
  UiContext(app::Game& game, app::MatchSession& session) : game(game), session(session) {}

  app::Game& game;
  app::MatchSession& session;

  // The league as screens see it (squads developed, injured players unavailable)
  const std::vector<Team>& league();
  const Team* team(const TeamId& id);
  // Any player the world knows (squads, free agents, retired), developed
  const Player* player(const PlayerId& id);
  const OrderedMap<PlayerId, Player>& allPlayers();
  std::string playerName(const PlayerId& id);

  // The team the user manages right now (the season's, else the friendly's)
  TeamId userTeam() const;
  bool seasonActive() const { return game.state().season.season.has_value(); }

  Screen screen() const;
  static Screen parseScreen(const std::string& s);
  void goTo(Screen s);

  // ---- the match flow
  bool hasMatch() const;
  bool matchIsLive() const;
  // After the toss: the session plays the Game's match and the Match screen shows it
  void startLiveMatch();
  // Leave a match: back to the season for a fixture, else home (a new game)
  void leaveMatch();
  // Same two sides again (friendlies): back to training
  void rematch();
  bool matchIsFixture() const;

  void openCard(const PlayerId& id);

  // Revision of the state the caches were built from
  uint64_t revision() const { return game.revision(); }

 private:
  void refresh();
  void handBack();
  uint64_t builtAt_ = UINT64_MAX;
  std::vector<Team> league_;
  OrderedMap<PlayerId, Player> players_;
};

}  // namespace t20::ui
