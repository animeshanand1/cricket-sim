#pragma once
// The match as plain data (match.js keeps it as a serialisable object; so do we).
// Field names follow the JS so the save format can stay the same.

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/types.hpp"

namespace t20 {

enum class MatchStatus : uint8_t { toss, live, break_, complete };
inline std::string_view name(MatchStatus s) {
  static constexpr std::string_view names[] = {"toss", "live", "break", "complete"};
  return names[static_cast<size_t>(s)];
}

enum class Awaiting : uint8_t { none, bowler, batter, review };
T20_ENUM_NAMES(Awaiting, "", "bowler", "batter", "review")

enum class BallKind : uint8_t { run, b4, b6, extra, wicket };
T20_ENUM_NAMES(BallKind, "run", "b4", "b6", "extra", "wicket")

enum class CommentKind : uint8_t { info, wicket, extra, dot, run, four, six, milestone, over, result, drop, review };
T20_ENUM_NAMES(CommentKind, "info", "wicket", "extra", "dot", "run", "four", "six", "milestone", "over", "result", "drop",
               "review")

enum class TossDecision : uint8_t { bat, bowl };
T20_ENUM_NAMES(TossDecision, "bat", "bowl")

enum class TargetMode : uint8_t { attack, block };

// The 23 fielding positions (field.js POSITIONS, in the same order)
enum class Pos : uint8_t {
  slip1, slip2, gully, legSlip, shortLeg, sillyPoint, point, cover, extraCover, midOff, midOn, midWicket, squareLeg,
  shortFineLeg, shortThirdMan, thirdMan, deepPoint, deepCover, longOff, longOn, deepMidWicket, deepSquareLeg, fineLeg,
  COUNT
};
T20_ENUM_NAMES(Pos, "slip1", "slip2", "gully", "legSlip", "shortLeg", "sillyPoint", "point", "cover", "extraCover",
               "midOff", "midOn", "midWicket", "squareLeg", "shortFineLeg", "shortThirdMan", "thirdMan", "deepPoint",
               "deepCover", "longOff", "longOn", "deepMidWicket", "deepSquareLeg", "fineLeg")
inline constexpr size_t kPosCount = static_cast<size_t>(Pos::COUNT);

enum class FieldPreset : uint8_t { attacking, balanced, defensive, death, spin, offside, auto_, custom };
inline std::string_view name(FieldPreset p) {
  static constexpr std::string_view names[] = {"attacking", "balanced", "defensive", "death", "spin", "offside", "auto",
                                               "custom"};
  return names[static_cast<size_t>(p)];
}

// ---------------------------------------------------------------------------

// Season stages (season.js)
enum class Stage : uint8_t { league, q1, elim, q2, final_ };
inline std::string_view name(Stage s) {
  static constexpr std::string_view names[] = {"league", "q1", "elim", "q2", "final"};
  return names[static_cast<size_t>(s)];
}

// match.fixture: which season game a match is
struct FixtureRef {
  int no = 0;
  std::string seasonId;
  int seasonNo = 0;
  Stage stage = Stage::league;
  std::string venue;
  TeamId home;
};

struct ImpactSub {
  PlayerId out, in;
  std::string when;
};

struct MatchTeam {
  TeamId id;
  std::string name, color;
  PlayerId keeperId;
  Captain captain = Captain::balanced;  // the AI's captaincy style
  std::optional<PlayerId> captainId;    // the player leading the side
  std::vector<PlayerId> players;        // the XI in batting order
  std::vector<PlayerId> bench;          // named substitutes (Impact Player rule)
  std::optional<ImpactSub> impact;
};

// A tempo instruction: 1-5, or nullopt for 'auto'
using Tempo = std::optional<double>;

struct MatchSettings {
  Tempo aggression = 3.0;
  bool autoBowl = false, autoBat = false, autoReview = false;
  std::unordered_map<PlayerId, std::optional<TargetMode>> targets;  // by bowler
  std::unordered_map<PlayerId, Tempo> batterTempo;                  // by batter
};

struct Rain {
  RainWhen when = RainWhen::pre;
  int overs = 0;   // pre: overs a side
  int atOver = 0;  // first / second
  int lost = 0;
  bool abandon = false;  // second only
};

struct Weather {
  Sky sky = Sky::clear;
  std::optional<Rain> rain;
  double rainChance = 0;
  bool rainDone = false;
};

struct Dls {
  bool reduced = false;
  std::string note;
  std::optional<double> r1, r2;
  std::optional<int> target, firstInnings, par;
  bool abandoned = false, noResult = false;
};

struct Fans {
  std::optional<int> home;
  double capacity = 0;
  std::array<double, 2> base{}, now{}, overStart{};
  std::array<double, 2> mood{50, 50};
  std::vector<std::string> told;
};

struct Toss {
  std::optional<std::string> call, coin;  // "heads" / "tails"
  int winner = 0;
  std::optional<TossDecision> decision;
};

struct Result {
  std::optional<int> winner;
  bool superOver = false, tied = false, dls = false, noResult = false;
  std::string text;
};

// ---------------------------------------------------------------------------
// Innings
// ---------------------------------------------------------------------------

struct Extras {
  int wd = 0, nb = 0, lb = 0, b = 0;
};

struct ReviewsLeft {
  int bat = 2, bowl = 2;
};

struct ReviewRecord {
  int ball = 0;
  int side = 0;
  PlayerId batter, bowler;
  Out type = Out::lbw;
  std::string given;
  bool overturned = false;
};

struct Dismissal {
  Out type = Out::bowled;
  std::optional<PlayerId> bowler, fielder;
};

struct BatterInnings {
  int runs = 0, balls = 0, fours = 0, sixes = 0, dots = 0;
  bool out = false;
  std::optional<std::string> howOut;
  double conf = 45;
  bool zone = false;
  std::optional<Dismissal> dismissal;
};

struct BowlingFigures {
  int balls = 0, runs = 0, wickets = 0, maidens = 0, dots = 0, fours = 0, sixes = 0, wides = 0, noballs = 0;
  double rhythm = 50;
  std::optional<int> specialAt;
  int specials = 0;
};

struct BallReview {
  std::string by;  // "bat" / "bowl"
  bool overturned = false;
};

struct Ball {
  std::string label;  // "•", "1", "4", "6", "W", "wd", "2wd", "nb", "4nb", "1lb", "1b"
  BallKind kind = BallKind::run;
  std::optional<PlayerId> bat, faced;
  int runs = 0;
  std::optional<Out> wicketType;
  std::optional<SpecialKey> special;
  std::optional<BallReview> review;
  std::optional<Pos> pos;
  int saved = 0;  // runs the shot was worth if a fielder cut it off
  int overthrow = 0;
  bool legBye = false, bye = false, drop = false, wide = false;
  std::optional<double> dir, dist;  // placement (degrees, share of the boundary)
};

struct Over {
  PlayerId bowler;
  std::vector<Ball> balls;
  int runs = 0, bowlerRuns = 0, wickets = 0;
};

struct FallOfWicket {
  int runs = 0, wicket = 0;
  PlayerId playerId;
  std::string over;
  int ball = 0;
  bool announced = false;
};

struct Partnership {
  int runs = 0, balls = 0;
};

struct CommentaryLine {
  int id = 0;
  std::string over;
  CommentKind kind = CommentKind::info;
  std::string text;
};

struct MindsetNow {
  PlayerId batter;
  double instructed = 3, tempo = 3;
  std::string label;
};

struct DropRecord {
  PlayerId fielder, batter;
  int ball = 0;
  std::string type;  // "catch" / "stumping"
};

struct UserField {
  FieldPreset preset = FieldPreset::auto_;
  std::optional<std::vector<Pos>> positions;
};

struct FieldNow {
  FieldPreset preset = FieldPreset::balanced;
  std::vector<Pos> positions;
  // the cache key: same bowler, same field, same fielders -> same men in the same places
  PlayerId keyBowler;
  std::vector<PlayerId> keyOutfield;
  std::array<PlayerId, kPosCount> assign{};  // empty = nobody there
};

struct LastPlan {
  PlayerId bowler;
  Plan plan = Plan::stock;
  bool fits = false;
  double read = 0;
};

struct PlanStreak {
  Plan plan = Plan::stock;
  int n = 0;
};

struct Innings {
  int batting = 0, bowlingSide = 1;
  int superOver = 0;  // 0 = a normal innings; n = the n-th Super Over
  int maxBalls = 120, maxWickets = 10;
  std::optional<int> target;
  int runs = 0, wickets = 0, legalBalls = 0;
  Extras extras;
  ReviewsLeft reviewsLeft;
  std::vector<ReviewRecord> reviews;
  OrderedMap<PlayerId, BatterInnings> batters;
  std::vector<PlayerId> battedOrder, yetToBat;
  std::optional<PlayerId> striker, nonStriker;
  std::vector<PlayerId> bowlingOrder;
  OrderedMap<PlayerId, BowlingFigures> bowling;
  std::optional<PlayerId> currentBowler, lastOverBowler;
  std::vector<Over> overs;
  std::vector<FallOfWicket> fow;
  Partnership partnership;
  bool freeHit = false;
  std::vector<CommentaryLine> commentary;
  bool complete = false;
  std::optional<MindsetNow> mindset;
  std::vector<DropRecord> drops;
  // tactics
  std::unordered_map<PlayerId, std::optional<Plan>> plans;  // user's plan per bowler (nullopt = auto)
  std::unordered_map<PlayerId, PlanStreak> planStreak;      // per batter
  std::optional<UserField> userField;
  std::optional<FieldNow> fieldNow;
  std::optional<LastPlan> lastPlan;
};

// The pending ball (match.pendingBall) while the user decides on a review
struct Umpire {
  Out type = Out::lbw;
  std::string truth, given;  // "out" / "notOut"
  std::string reviewer;      // "bat" / "bowl"
  bool sure = false, reviewed = false, overturned = false;
};

struct Shot {
  double dir = 0, dist = 0;
  std::optional<Pos> pos;
  bool rope = false;
  int hit = 0;
};

// Outcome of a legal ball: index 0..6 = "0","1","2","3","4","6","W"
enum class Outcome : uint8_t { o0, o1, o2, o3, o4, o6, W };
inline constexpr int kOutcomeRuns[] = {0, 1, 2, 3, 4, 6, -1};
inline Outcome outcomeForRuns(int runs) {
  switch (runs) {
    case 0: return Outcome::o0;
    case 1: return Outcome::o1;
    case 2: return Outcome::o2;
    case 3: return Outcome::o3;
    case 4: return Outcome::o4;
    default: return Outcome::o6;
  }
}

struct BallDecision {
  Outcome outcome = Outcome::o0;
  std::optional<Out> type;
  std::optional<PlayerId> fielderId;
  std::optional<std::string> chanceMissed;  // "catch" / "stumping"
  std::optional<SpecialKey> special;
  bool wasFreeHit = false;
  Plan plan = Plan::stock;
  bool fits = false;
  double h2hEdge = 0;
  std::optional<Shot> shot;
  std::optional<std::string> extra;  // "lb" / "b"
  int overthrow = 0;
  std::optional<Umpire> umpire;
  int overIdx = 0;
};

struct SoHistory {
  // side -> [(batter, was out)] in batting order; side -> first bowler
  std::array<std::optional<std::vector<std::pair<PlayerId, bool>>>, 2> bat;
  std::array<std::optional<PlayerId>, 2> bowl;
  int secondBatting = 0;
};

struct Match {
  std::string id;
  int32_t seed = 0;
  int32_t rngState = 0;
  Pitch pitch = Pitch::balanced;
  std::optional<int> userTeam;  // 0/1, or none for AI v AI
  OrderedMap<PlayerId, Player> players;  // copies (XI + bench of both sides)
  std::array<MatchTeam, 2> teams;
  std::optional<Toss> toss;
  std::optional<int> battingFirst;
  MatchStatus status = MatchStatus::toss;
  int current = 0;
  std::vector<Innings> innings;
  Awaiting awaiting = Awaiting::none;
  std::optional<BallDecision> pendingBall;
  MatchSettings settings;
  std::optional<Result> result;
  std::optional<PlayerId> potm;
  int commentarySeq = 0;
  bool dew = false;
  Weather weather;
  int oversPerSide = 20;
  std::optional<Dls> dls;
  Fans fans;
  OrderedMap<std::string, double> h2h;  // "bat|bowl" -> edge -1..+1
  bool quiet = false;                    // a copy simmed for win probability: no commentary
  std::optional<SoHistory> soPrev;
  std::vector<std::string> impactNotes;
  std::optional<FixtureRef> fixture;  // the season fixture this match belongs to
  std::optional<TeamId> managed;      // a quick sim of the user's club (achievements)
};

}  // namespace t20
