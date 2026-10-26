#include "ui/screens/ui_context.hpp"

#include "engine/league_view.hpp"
#include "ui/anim.hpp"

namespace t20::ui {

void UiContext::refresh() {
  if (builtAt_ == game.revision()) return;
  league_ = game.league();
  players_ = allPlayersIn(game.state().world, game.state().career.progress);
  builtAt_ = game.revision();
}

const std::vector<Team>& UiContext::league() {
  refresh();
  return league_;
}

const Team* UiContext::team(const TeamId& id) {
  for (const auto& t : league())
    if (t.id == id) return &t;
  return nullptr;
}

const Player* UiContext::player(const PlayerId& id) {
  refresh();
  return players_.find(id);
}

const OrderedMap<PlayerId, Player>& UiContext::allPlayers() {
  refresh();
  return players_;
}

std::string UiContext::playerName(const PlayerId& id) {
  const Player* p = player(id);
  return p ? p->name : id;
}

TeamId UiContext::userTeam() const {
  const auto& s = game.state();
  if (s.season.season) return s.season.season->userTeam;
  return s.setup.userTeamId;
}

Screen UiContext::parseScreen(const std::string& s) {
  if (s == "xi") return Screen::xi;
  if (s == "training") return Screen::training;
  if (s == "toss") return Screen::toss;
  if (s == "match") return Screen::match;
  if (s == "season") return Screen::season;
  if (s == "offseason") return Screen::offseason;
  if (s == "draft") return Screen::draft;
  if (s == "sacked") return Screen::sacked;
  if (s == "club") return Screen::club;
  if (s == "players") return Screen::players;
  return Screen::home;
}

Screen UiContext::screen() const { return parseScreen(game.state().setup.screen); }

void UiContext::goTo(Screen s) {
  static const char* names[] = {"home", "xi", "training", "toss", "match", "season",
                                "offseason", "draft", "sacked", "club", "players"};
  if (s == Screen::players) {
    game.openPlayers();
    return;
  }
  game.goTo(names[static_cast<int>(s)]);
}

bool UiContext::hasMatch() const { return session.hasMatch() || game.state().match.has_value(); }

bool UiContext::matchIsLive() const {
  if (session.hasMatch()) return session.isLive();
  const auto& m = game.state().match;
  return m && (m->status == MatchStatus::live || m->status == MatchStatus::break_);
}

void UiContext::startLiveMatch() {
  const auto& m = game.state().match;
  if (!m) return;
  session.setSavedMatchups(&game.state().career.matchups);
  session.adopt(*m, game.league());
  game.goTo("match");
}

bool UiContext::matchIsFixture() const {
  if (session.hasMatch()) return session.match().fixture.has_value();
  const auto& m = game.state().match;
  return m && m->fixture.has_value();
}

void UiContext::leaveMatch() {
  const bool fixture = matchIsFixture();
  // The session's settings (tempo, auto-bowl) carry over to the next match, as in the JS
  if (session.hasMatch() && game.state().match) game.state().match->settings = session.match().settings;
  session.clear();
  if (fixture) game.backToSeason();
  else game.newGame();
}

void UiContext::rematch() {
  if (session.hasMatch() && game.state().match) game.state().match->settings = session.match().settings;
  session.clear();
  game.rematch();
}

void UiContext::openCard(const PlayerId& id) {
  game.openCard(id);
  startTween("modal_in", 0.0f, 1.0f, 0.22, EaseType::EaseOutQuad);
}

}  // namespace t20::ui
