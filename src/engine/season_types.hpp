#pragma once
// A season (season.js): groups, 70 league fixtures, playoffs, and compact match summaries.
// The summary's batting and bowling rows are JSON arrays in the save, as in the browser.

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/match_types.hpp"
#include "engine/types.hpp"

namespace t20 {

struct BatRow {  // [id, runs, balls, fours, sixes, out (0/1), dots]
  PlayerId id;
  int runs = 0, balls = 0, fours = 0, sixes = 0, out = 0, dots = 0;
};
struct BowlRow {  // [id, balls, runs, wickets, maidens, dots, fours, sixes, wides, noballs]
  PlayerId id;
  int balls = 0, runs = 0, wkts = 0, maidens = 0, dots = 0, fours = 0, sixes = 0, wides = 0, noballs = 0;
};
struct InningsSummary {
  TeamId team;
  int runs = 0, wickets = 0, balls = 0;
  bool allOut = false;
  int maxBalls = 120, extras = 0;
  std::vector<std::array<int, 2>> overs;  // [runs, wickets] per over
  std::vector<BatRow> batting;
  std::vector<BowlRow> bowling;
};
struct FieldingLine {
  int ct = 0, st = 0, ro = 0, drops = 0;
};
struct SuperOverLine {
  TeamId team;
  int runs = 0, wickets = 0;
};
struct TossLine {
  TeamId winner;
  std::optional<TossDecision> decision;
};
struct ImpactLine {
  TeamId team;
  PlayerId out, in;
};
struct MatchSummary {
  std::string matchId;
  std::optional<TeamId> winner;
  std::string text;
  bool noResult = false, dls = false;
  std::optional<std::vector<SuperOverLine>> superOver;
  std::optional<PlayerId> potm;
  std::optional<TossLine> toss;
  bool dew = false;
  std::vector<InningsSummary> innings;
  OrderedMap<PlayerId, FieldingLine> fielding;
  std::vector<ImpactLine> impact;
};

struct Fixture {
  int no = 0;
  Stage stage = Stage::league;
  TeamId home, away;
  std::string venue;
  Pitch pitch = Pitch::balanced;
  int32_t seed = 0;
  std::optional<MatchSummary> result;
};

struct Venue {
  std::string ground;
  Pitch pitch = Pitch::balanced;
};

struct Season {
  std::string id;  // "S{number}-{seed}"
  int number = 1;
  int32_t seed = 0;
  TeamId userTeam;
  std::vector<TeamId> groupA, groupB;  // JSON: groups { A, B }
  OrderedMap<TeamId, Venue> venues;
  std::vector<Fixture> fixtures;
  std::optional<TeamId> champion, runnerUp;
  std::optional<std::vector<TeamId>> top4;
};

struct CapLine {
  PlayerId id;
  double value = 0;  // runs, wickets or MVP points
};
struct SeasonArchive {
  int number = 0;
  TeamId champion, runnerUp, userTeam;
  std::optional<int> userPos;
  std::optional<CapLine> orangeCap, purpleCap, mvp;  // JSON keys runs / wkts / pts
};

struct Busy {
  std::string label;
  int done = 0;
};

struct SeasonState {
  std::optional<Season> season;
  std::vector<SeasonArchive> history;
  std::optional<Busy> busy;  // not saved
};

}  // namespace t20
