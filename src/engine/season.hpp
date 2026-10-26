#pragma once
// An IPL-style season (season.js): two groups of five seeded by strength, 70 league games, points
// table with net run rate, playoffs (Qualifier 1, Eliminator, Qualifier 2, Final), and the stats,
// analytics, records and awards built from the match summaries.

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/season_types.hpp"

namespace t20 {

inline constexpr int LEAGUE_MATCHES = 70;
std::string_view stageLabel(Stage s);

Season createSeason(int32_t seed, int number, const TeamId& userTeam, const std::vector<Team>& league);

// A compact record of a completed match: enough for the table, NRR, stats and analytics
MatchSummary summarizeMatch(const Match& match);
bool recordResult(Season& season, int fixtureNo, MatchSummary summary);

struct TableRow {
  TeamId team;
  std::string group;
  int p = 0, w = 0, l = 0, nr = 0, pts = 0;
  double rf = 0, bf = 0, ra = 0, ba = 0, wkts = 0, ballsBowled = 0;
  std::vector<std::string> form;  // last 5: W / L / N
  double nrr = 0, wpb = 0;
  int pos = 0;
};
std::vector<TableRow> pointsTable(const Season& season, int upTo = std::numeric_limits<int>::max());
bool leagueComplete(const Season& season);
void advancePlayoffs(Season& season);

const Fixture* nextFixture(const Season& season);
const Fixture* nextUserFixture(const Season& season);
// "waiting" | "eliminated" | "playing" | none
std::optional<std::string> userPlayoffStatus(const Season& season);

// ---- Stats and analytics
using TeamOf = std::function<TeamId(const PlayerId&)>;

struct BatStat {
  PlayerId id;
  TeamId team;
  int inns = 0, no = 0, runs = 0, balls = 0, fours = 0, sixes = 0, hs = 0;
  bool hsNO = false;
  int fifties = 0, hundreds = 0, ducks = 0, dots = 0, m = 0;
  std::optional<double> avg, sr, boundaryPct;
  std::string hsText;
};
struct BowlStat {
  PlayerId id;
  TeamId team;
  int inns = 0, balls = 0, runs = 0, wkts = 0, maidens = 0, dots = 0, fours = 0, sixes = 0, wides = 0, noballs = 0;
  int bestW = 0, bestR = 0, fourW = 0, fiveW = 0, m = 0;
  double overs = 0;
  std::string oversText, bestText;
  std::optional<double> avg, econ, sr, dotPct;
};
struct FieldStat {
  PlayerId id;
  TeamId team;
  int ct = 0, st = 0, ro = 0, drops = 0, m = 0, dismissals = 0;
};
struct MvpStat {
  PlayerId id;
  TeamId team;
  double pts = 0;
  int wkts = 0, dots = 0, fours = 0, sixes = 0, catches = 0, m = 0;
};
struct SeasonStats {
  std::vector<BatStat> batting;
  std::vector<BowlStat> bowling;
  std::vector<FieldStat> fielding;
  std::vector<MvpStat> mvp;
  int matches = 0;
};
SeasonStats seasonStats(const Season& season, const TeamOf& teamOf);

struct TotalLine {
  int runs = 0, wickets = 0;
  TeamId vs;
  int no = 0;
};
struct TeamStat {
  TeamId team;
  int p = 0, w = 0, l = 0, batFirstP = 0, batFirstW = 0, chaseP = 0, chaseW = 0;
  int runs = 0, balls = 0, conceded = 0, ballsBowled = 0;
  std::vector<int> firstInnsTotals;
  std::optional<TotalLine> highest, lowest;
  int ppRuns = 0, ppOvers = 0, midRuns = 0, midOvers = 0, deathRuns = 0, deathOvers = 0;
  int ppConc = 0, ppConcOvers = 0, deathConc = 0, deathConcOvers = 0;
  int fours = 0, sixes = 0, dots = 0, wkts = 0, superOvers = 0;
  std::optional<double> avgFirst, rr, econ, ppRR, midRR, deathRR, ppEcon, deathEcon, dotPct, winPct;
};
std::vector<TeamStat> teamAnalytics(const Season& season);

struct PositionDay {
  int no = 0;
  struct Row {
    TeamId team;
    int pos, pts;
  };
  std::vector<Row> table;
};
std::vector<PositionDay> positionHistory(const Season& season);

struct SeasonTotal {
  TeamId team, vs;
  int runs, wickets, no;
  Stage stage;
};
struct SeasonScore {
  PlayerId id;
  TeamId team, vs;
  int runs, balls, out, no;
};
struct SeasonFigure {
  PlayerId id;
  TeamId team, vs;
  int wkts, runs, balls, no;
};
struct SeasonRecords {
  std::vector<SeasonTotal> highestTotals, lowestTotals;
  std::vector<SeasonScore> highestScores;
  std::vector<SeasonFigure> bestFigures;
};
SeasonRecords seasonRecords(const Season& season, const TeamOf& teamOf);

struct SeasonAwards {
  std::optional<BatStat> orangeCap, mostSixes, mostFours, bestStrikeRate;
  std::optional<BowlStat> purpleCap, bestEconomy, mostDots;
  std::optional<MvpStat> mvp, emerging;
  std::optional<FieldStat> safestHands;
};
SeasonAwards seasonAwards(const Season& season, const TeamOf& teamOf,
                          const std::function<std::optional<int>(const PlayerId&)>& ageOf);

}  // namespace t20
