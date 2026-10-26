#pragma once
// The career save (career.js and friends): per-player records, development, the records book,
// popularity, batter v bowler match-ups, milestones and the manager's achievements.
// Field names follow the JS so the JSON save is the same as the browser's.

#include <optional>
#include <string>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/matchups.hpp"
#include "engine/types.hpp"

namespace t20 {

struct BatCareer {
  int inns = 0, notOuts = 0, runs = 0, balls = 0, fours = 0, sixes = 0, hs = 0;
  bool hsNotOut = false;
  int fifties = 0, hundreds = 0, ducks = 0;
};
struct BowlCareer {
  int inns = 0, balls = 0, runs = 0, wickets = 0, maidens = 0, dots = 0, bestW = 0, bestR = 0, fourW = 0;
};
struct FieldCareer {
  int catches = 0, stumpings = 0, runOuts = 0, drops = 0;
};
struct RecentLine {
  TeamId vs;
  std::optional<std::string> bat, bowl;  // "45* (31)", "2/28"
  std::string result;                    // "W" / "L" / "T"
};
struct CareerStats {
  int matches = 0, wins = 0, potm = 0;
  BatCareer bat;
  BowlCareer bowl;
  FieldCareer field;
  std::vector<RecentLine> recent;  // newest first, at most 5
};

// Development deltas on top of a player's own skills (development.js)
struct Progress {
  SkillSet<BatK> bat;
  SkillSet<BowlK> bowl;
  SkillSet<FieldK> field;
};

// One line of the development report after a match
struct DevPerf {
  double perf = 0;
  std::string label, line;
  double delta = 0;
};
struct DevField {
  int taken = 0, dropped = 0;
  double delta = 0;
};
struct Overalls {
  double bat = 0, bowl = 0, field = 0;
};
struct DevReport {
  PlayerId id;
  std::optional<DevPerf> bat, bowl;
  std::optional<DevField> field;
  bool captain = false;
  Overalls before, after;
};
struct LastReport {
  std::string matchId;
  std::vector<DevReport> reports;
};

// ---- Records book (records.js): nine top-10 lists, each with its own row shape
struct RecBase {
  std::string matchId, when;
};
struct ScoreRec : RecBase {  // highScores
  PlayerId id;
  TeamId team, vs;
  int runs = 0, balls = 0;
  bool notOut = false;
  int fours = 0, sixes = 0;
};
struct SixesRec : RecBase {  // mostSixes
  PlayerId id;
  TeamId team, vs;
  int sixes = 0, runs = 0, balls = 0;
};
struct FastRec : RecBase {  // fastestFifties, fastestHundreds
  PlayerId id;
  TeamId team, vs;
  int balls = 0, runs = 0;
};
struct BowlRec : RecBase {  // bestBowling
  PlayerId id;
  TeamId team, vs;
  int wickets = 0, runs = 0, balls = 0;
};
struct TotalRec : RecBase {  // highTotals, lowTotals
  TeamId team, vs;
  int runs = 0, wickets = 0, balls = 0;
  double overs = 20;
};
struct MarginRec : RecBase {  // bigWinsRuns (no ballsLeft), bigWinsWkts
  TeamId team, vs;
  int margin = 0;
  std::optional<int> ballsLeft;
};
struct Records {
  std::vector<ScoreRec> highScores;
  std::vector<BowlRec> bestBowling;
  std::vector<FastRec> fastestFifties, fastestHundreds;
  std::vector<SixesRec> mostSixes;
  std::vector<TotalRec> highTotals, lowTotals;
  std::vector<MarginRec> bigWinsRuns, bigWinsWkts;
};

struct Popularity {
  double v = 0, d = 0;
};

// ---- Milestones (milestones.js)
struct MilestoneHit {
  PlayerId id;
  TeamId team;
  std::string type;  // runs, wickets, sixes, fours, catches, matches, hundred
  int value = 0;
  std::optional<std::string> when;  // set in the career log
};
struct LastMilestones {
  std::string matchId;
  std::vector<MilestoneHit> list;
};

// ---- Manager achievements (achievements.js)
struct Unlock {
  std::string when;
  TeamId vs;
};
struct ManagerRecord {
  int played = 0, wins = 0, streak = 0, bestStreak = 0;
  OrderedMap<std::string, Unlock> unlocked;
};
struct LastUnlocked {
  std::string matchId;
  std::vector<std::string> keys;
};

struct Career {
  OrderedMap<PlayerId, CareerStats> players;
  std::vector<std::string> recorded;  // match ids already recorded (the newest 500)
  OrderedMap<PlayerId, Progress> progress;
  std::optional<LastReport> lastReport;
  Records records;
  OrderedMap<PlayerId, Popularity> popularity;
  MatchupBook matchups;
  std::vector<MilestoneHit> milestones;  // newest 50 across the league
  std::optional<LastMilestones> lastMilestones;
  ManagerRecord manager;
  std::optional<LastUnlocked> lastUnlocked;
};

}  // namespace t20
