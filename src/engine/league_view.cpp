#include "engine/league_view.hpp"

#include "engine/development.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/world.hpp"

namespace t20 {

const std::vector<Team>& LEAGUE() {
  static const std::vector<Team> league = generateLeague();
  return league;
}

const Player* playerById(const PlayerId& id) {
  for (const auto& t : LEAGUE())
    for (const auto& p : t.squad)
      if (p.id == id) return &p;
  return nullptr;
}

const Team* teamById(const std::vector<Team>& league, const TeamId& id) {
  for (const auto& t : league)
    if (t.id == id) return &t;
  return nullptr;
}

std::vector<Team> effectiveLeague(const World& world, const OrderedMap<PlayerId, Progress>& progress) {
  std::vector<Team> league = worldTeams(world);
  for (auto& t : league) {
    for (auto& p : t.squad) {
      Player q = p;
      q.devBase = p.skills;
      q.unavailable = isInjured(p);
      p = applyProgress(q, progress.find(p.id));
    }
  }
  return league;
}

OrderedMap<PlayerId, Player> allPlayersIn(const World& world, const OrderedMap<PlayerId, Progress>& progress) {
  OrderedMap<PlayerId, Player> out;
  out.reserve(world.players.size());
  for (const auto& [id, p] : world.players) {
    Player q = p;
    q.devBase = p.skills;
    out.set(id, applyProgress(q, progress.find(id)));
  }
  return out;
}

}  // namespace t20
