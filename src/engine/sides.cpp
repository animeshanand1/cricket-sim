#include "engine/sides.hpp"

#include <algorithm>

#include "core/str.hpp"
#include "engine/career.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/mindset.hpp"

namespace t20 {

SideInput buildSide(const Team& team, const std::vector<PlayerId>& ids, const PlayerId& keeperId,
                    const CareerPlayers* careerPlayers, const std::vector<TrainingSlot>& training,
                    const std::optional<PlayerId>& captainId) {
  auto drillOf = [&](const PlayerId& id) -> const std::string* {
    const std::string* d = nullptr;
    for (const auto& t : training)
      if (t.playerId == id) d = &t.drill;  // Object.fromEntries: the last one wins
    return d;
  };
  OrderedMap<PlayerId, Player> byId;
  for (const auto& p : team.squad) {
    Player ready = applyFatigue(p);
    ready.startConf = startConfidence(formOf(careerPlayers ? careerPlayers->find(p.id) : nullptr).bonus);
    const std::string* drill = drillOf(p.id);
    byId.set(p.id, drill ? applyTraining(ready, *drill) : ready);
  }
  auto has = [&](const std::optional<PlayerId>& id) {
    return id && std::find(ids.begin(), ids.end(), *id) != ids.end();
  };
  SideInput side;
  side.id = team.id;
  side.name = team.name;
  side.color = team.color;
  side.keeperId = keeperId;
  side.captain = team.captain;
  if (has(captainId)) {
    side.captainId = captainId;
  } else if (has(team.captainId)) {
    side.captainId = team.captainId;
  } else {
    std::vector<const Player*> xi;
    for (const auto& id : ids)
      if (const Player* p = byId.find(id)) xi.push_back(p);
    side.captainId = pickCaptain(xi);
  }
  for (const auto& id : ids) side.players.push_back(byId.at(id));
  for (const auto& id : benchOf(team, ids)) side.bench.push_back(byId.at(id));
  return side;
}

SideInput aiSide(const Team& team, Pitch pitch, const CareerPlayers* careerPlayers) {
  const BestXI xi = pickBestXI(team, pitch);
  std::vector<const Player*> players;
  for (const auto& id : xi.ids)
    for (const auto& p : team.squad)
      if (p.id == id) {
        players.push_back(&p);
        break;
      }
  const auto plan = aiTrainingPlan(players, pitch);
  return buildSide(team, xi.ids, xi.keeperId, careerPlayers, plan);
}

Match simFixture(const Fixture& fixture, const std::vector<Team>& league, const CareerPlayers* careerPlayers,
                 const MatchupBook* matchups) {
  auto find = [&](const TeamId& id) -> const Team& {
    for (const auto& t : league)
      if (t.id == id) return t;
    return league[0];
  };
  MatchSetup setup;
  setup.seed = fixture.seed;
  setup.id = cat('F', fixture.no, '-', fixture.seed);
  setup.pitch = fixture.pitch;
  setup.userTeam = std::nullopt;
  setup.teams = {aiSide(find(fixture.home), fixture.pitch, careerPlayers),
                 aiSide(find(fixture.away), fixture.pitch, careerPlayers)};
  setup.ground.home = std::optional<TeamId>(fixture.home);
  setup.ground.big = fixture.stage != Stage::league;
  setup.matchups = matchups;
  Match m = createMatch(setup);
  simMatch(m);
  return m;
}

}  // namespace t20
