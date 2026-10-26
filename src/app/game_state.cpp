#include "app/game_state.hpp"

#include "engine/league.hpp"
#include "engine/league_view.hpp"

namespace t20::app {

namespace {
Json idList(const std::vector<PlayerId>& v) {
  Json a = Json::array();
  for (const auto& id : v) a.push(id);
  return a;
}
Json optId(const std::optional<PlayerId>& v) { return v ? Json(*v) : Json(); }

Lineup bestXIOf(const TeamId& teamId, const std::string& pitch) {
  const Team* team = teamById(LEAGUE(), teamId);
  if (!team) team = &LEAGUE()[0];
  Pitch p = Pitch::balanced;
  for (size_t i = 0; i < std::size(kPitchNames); ++i)
    if (kPitchNames[i] == pitch) p = static_cast<Pitch>(i);
  const BestXI xi = pickBestXI(*team, pitch == "random" ? Pitch::balanced : p);
  return Lineup{xi.ids, xi.keeperId, std::nullopt};
}
}  // namespace

Json setupToJson(const SetupState& s) {
  Json o = Json::object();
  o.add("userTeamId", s.userTeamId);
  o.add("oppTeamId", s.oppTeamId);
  o.add("pitch", s.pitch);
  Json lineups = Json::object();
  for (const auto& [team, l] : s.lineups) {
    Json e = Json::object();
    e.add("xi", idList(l.xi));
    e.add("keeperId", optId(l.keeperId));
    e.add("captainId", optId(l.captainId));
    lineups.add(team, e);
  }
  o.add("lineups", lineups);
  Json ms = Json::object();
  ms.add("aggression", s.matchSettings.aggression);
  ms.add("autoBowl", s.matchSettings.autoBowl);
  o.add("matchSettings", ms);
  return o;
}

SetupState setupFromSaved(const Json* saved) {
  const Json& j = saved && saved->isObject() ? *saved : Json();
  auto validTeam = [](const Json& v) { return v.isString() && teamById(LEAGUE(), v.str()) != nullptr; };
  SetupState s;
  s.userTeamId = validTeam(j["userTeamId"]) ? j["userTeamId"].str() : LEAGUE()[0].id;
  s.oppTeamId = validTeam(j["oppTeamId"]) ? j["oppTeamId"].str() : LEAGUE()[5].id;
  s.pitch = j["pitch"].isString() ? j["pitch"].str() : "random";
  for (const auto& [team, l] : j["lineups"].items()) {
    Lineup line;
    for (const auto& id : l["xi"].elements())
      if (id.isString()) line.xi.push_back(id.str());
    if (l["keeperId"].isString()) line.keeperId = l["keeperId"].str();
    if (l["captainId"].isString()) line.captainId = l["captainId"].str();
    s.lineups.set(team, line);
  }
  const Lineup* savedLine = s.lineups.find(s.userTeamId);
  const Lineup initial = savedLine && savedLine->xi.size() == 11 ? *savedLine : bestXIOf(s.userTeamId, "balanced");
  s.xi = initial.xi;
  s.keeperId = initial.keeperId;
  s.captainId = initial.captainId;
  s.lineups.set(s.userTeamId, Lineup{s.xi, s.keeperId, s.captainId});
  if (j["matchSettings"].isObject()) {
    s.matchSettings.aggression = j["matchSettings"]["aggression"].num(3);
    s.matchSettings.autoBowl = j["matchSettings"]["autoBowl"].truthy();
  }
  return s;
}

}  // namespace t20::app
