# 04: Data model

Every object the game stores, with its JS shape and the C++ struct to write. Field names stay the same as JS (camelCase) so the JSON save format is identical and the C++ can be checked against the JS line by line.

Conventions:
- **Numbers:** JS has one number type. Use `int` for counters (runs, balls, wickets, years, ages, money in lakh is always a multiple of 5 but use `double` to be safe), and `double` for everything that can be fractional: skills (fatigue makes them fractional in a match), ratings, confidence, rhythm, fitness, mood, popularity, board confidence.
- **Optional:** a JS field that can be `null`/missing → `std::optional<T>`.
- **Maps keyed by id:** JS objects iterate in insertion order. Where the code iterates them, use `OrderedMap<K,V>` (see [05 §1](05-engine-port-guide.md#1-determinism-rules)). Where it only looks things up, `std::unordered_map` is fine.
- **Ids:** `using PlayerId = std::string;` (`"CHK-1"`…`"CHK-16"` for roster players, `"P1"`, `"P2"`… for generated ones). `using TeamId = std::string;` (3 letters). Keep strings: they are the save format and C++ is fast enough.

---

## 1. Enums

| Enum | Values (JSON spelling) |
|------|------------------------|
| `Role` | `WK`, `BAT`, `AR`, `BOWL` |
| `Hand` | `R`, `L` |
| `BowlType` | `fast`, `fastmed`, `medium`, `lfast`, `lfastmed`, `lmedium` (pace) · `off`, `leg`, `sla`, `lws` (spin) · or none (`null`) |
| `Pitch` | `balanced`, `flat`, `green`, `dusty`, `slow` (setup also allows `random`) |
| `CaptainStyle` | `balanced`, `aggressive`, `cautious`, `shrewd`, `spin`, `pace` |
| `Personality` | `balanced`, `stars`, `moneyball`, `youth`, `experience`, `mavericks`, `bowling`, `bigspender` |
| `Origin` | `roster`, `prospect`, `journeyman`, `entrant`, `academy`, `draft`, `custom` (created by the user) |
| `ContractVia` | `retained`, `auction`, `rtm`, `renewed`, `draft`, `academy`, `base`, `free`, `trade`, `replacement`, `custom` (created by the user) |
| `Stage` | `league`, `q1`, `elim`, `q2`, `final` |
| `MatchStatus` | `toss`, `live`, `break`, `complete` |
| `Awaiting` | none, `bowler`, `batter`, `review` |
| `DismissalType` | `bowled`, `lbw`, `caught`, `caughtKeeper`, `caughtBowled`, `stumped`, `runout` |
| `BallKind` | `run`, `b4`, `b6`, `extra`, `wicket` |
| `CommentaryKind` | `info`, `wicket`, `extra`, `dot`, `run`, `four`, `six`, `milestone`, `over`, `result`, `drop`, `review` |
| `Phase` | `pp`, `mid`, `death` |
| `Sky` | `clear`, `overcast`, `humid`, `hot` |
| `RainWhen` | `pre`, `first`, `second` |
| `PlanKey` | pace: `stock`, `yorkers`, `bouncers`, `wide`, `slower`, `attack` · spin: `stock`, `flight`, `darts`, `wide`, `mystery` · user choice can also be `auto` |
| `FieldPreset` | `attacking`, `balanced`, `defensive`, `death`, `spin`, `offside`, plus user choices `auto`, `custom` |
| `PositionKey` | `slip1`, `slip2`, `gully`, `legSlip`, `shortLeg`, `sillyPoint`, `point`, `cover`, `extraCover`, `midOff`, `midOn`, `midWicket`, `squareLeg`, `shortFineLeg`, `shortThirdMan`, `thirdMan`, `deepPoint`, `deepCover`, `longOff`, `longOn`, `deepMidWicket`, `deepSquareLeg`, `fineLeg` |
| `SpecialKey` | `yorker`, `outswinger`, `inswinger`, `bouncer`, `seamer`, `knuckle`, `cutter`, `googly`, `doosra`, `carrom`, `armball`, `flipper`, `ripper`, `drifter` |
| `DrillKey` | `technique`, `range`, `rotation`, `pressure`, `newball`, `death`, `spincraft`, `lines`, `fielding` |
| `Tempo` | `auto` or 1–5 (Defend, Careful, Balanced, Attack, All-out) |
| `TargetMode` | none, `attack`, `block` |
| `Screen` | `home`, `xi`, `training`, `toss`, `match`, `season`, `offseason`, `draft` (alias), `club`, `sacked`, `players` |

---

## 2. Static data (compiled in, never saved)

| Table | JS source | Contents |
|-------|-----------|----------|
| `TEAMS` | `roster.js` | 10 × `{ id, name, city, color, ground, pitch, captain, personality }` |
| `ROSTER` | `roster.js` | 10 × 16 rows `[name, role, hand, bowlType, bat, power, bowl, field, age, overseas, batStyle, bowlStyle]` (2 WK, 5 BAT, 4 AR, 5 BOWL each) |
| `PITCHES` | `pitch.js` | label, report, par, boundary, dot, wicket, paceWicket, spinWicket, wear {spin, paceDecay, boundary} |
| `BOWL_TYPES` | `league.js` | label, short, spin |
| `BAT_SKILLS`, `PACE_SKILLS`, `SPIN_SKILLS`, `FIELD_SKILLS` | `skills.js` | key, label, group, weight, help |
| `BAT_STYLES`, `PACE_STYLES`, `SPIN_STYLES` | `skills.js` | skill modifiers per style |
| `POSITIONS`, `PRESETS` | `field.js` | angle, dist, zone; 6 presets of 9 positions |
| `PACE_PLANS`, `SPIN_PLANS`, `CAPTAIN_STYLES` | `tactics.js` | |
| `SPECIALS`, `BY_TYPE` | `special.js` | label, skill, dismissal multipliers |
| `DRILLS` | `development.js` | boosts per area |
| `CURVE` | `ageing.js` | per-season skill change by age band |
| `INJURIES` | `fitness.js` | pace list and other list |
| `TARGETS` | `board.js` | 5 board targets |
| `PERSONALITIES`, `RETENTION_SLABS`, `BASE_PRICES` | `finance.js` | |
| `FACILITIES`, `TICKET_TIERS`, `SPONSOR_SLOTS`, `BRANDS`, `DEAL_TYPES`, `COACH_AREAS`, `COACH_TIERS`, `STAFF_ROLES`, `STAFF_TIERS`, `GROUND_CAPACITY`, `BASE_POPULARITY`, `PRIZES` | `club.js` | |
| `ACHIEVEMENTS` | `achievements.js` | 23 (16 match, 7 season) |
| `LADDERS`, `APPEARANCES` | `milestones.js` | |
| `FIRST`, `LAST` | `names.js` | name parts |
| `LINES`, `WICKET_LINES`, `CAUGHT_AT`, shot lines | `commentary.js` | commentary text pools |
| `SKIES`, DLS table `F` | `weather.js` | |

The 10 teams:

| id | Name | Colour | Ground | Home pitch | AI captain | AI personality |
|----|------|--------|--------|-----------|------------|----------------|
| CHK | Chennai Supreme Kings | `#f2b705` | Chepok Stadium | dusty | shrewd | experience |
| MUI | Mumbay Islanders | `#0b72c9` | Wankade Stadium | flat | aggressive | stars |
| RGC | Regal Challengers Bengalore | `#b3121f` | Chinnaswami Stadium | flat | aggressive | stars |
| KNR | Kolkota Night Riders | `#5b2d90` | Eden Grounds | balanced | spin | mavericks |
| DLC | Dilli Capitals | `#2c4fb0` | Arun Jaitly Stadium | slow | balanced | youth |
| PBK | Punjabi Kings | `#ef5a2a` | Mohali Stadium | balanced | aggressive | bigspender |
| RJR | Rajasthan Regals | `#e8408f` | Sawai Mansing Stadium | dusty | shrewd | moneyball |
| SNH | Sunraisers Hyderabad | `#ff8c1a` | Rajiv Gandi Stadium | flat | pace | bowling |
| GUT | Gujarat Tigers | `#1b2a55` | Motera Arena | balanced | cautious | balanced |
| LKG | Lakhnow Super Giants | `#19b39a` | Ekaana Stadium | slow | cautious | youth |

Port tip: generate `roster.cpp` from `roster.js` with a small Node script (`node -e` that imports `ROSTER` and prints C++ initialisers) rather than typing 160 rows by hand.

---

## 3. Skills

```cpp
// Fixed-size skill sets with a presence mask: a bowler has either the pace set or the spin set.
enum class BatK   { defence, vsPace, vsSpin, grounded, lofted, rotation, running, awareness, temperament, COUNT };
enum class BowlK  { pace, accuracy, swing, seam, variations, death, turn, flight, COUNT };
enum class FieldK { catching, ground, throwing, keeping, COUNT };

template <class K> struct SkillSet {
  std::array<double, size_t(K::COUNT)> v{};
  std::bitset<size_t(K::COUNT)> has;
  double  operator[](K k) const { return v[size_t(k)]; }
  double& operator[](K k)       { has.set(size_t(k)); return v[size_t(k)]; }
  bool    contains(K k) const   { return has.test(size_t(k)); }
};

struct Skills { SkillSet<BatK> bat; SkillSet<BowlK> bowl; SkillSet<FieldK> field; };
```

- Batting: all 9 always present.
- Bowling: pace set = `pace, accuracy, swing, seam, variations, death`; spin set = `turn, flight, accuracy, variations`. A player with no bowling type still has the **spin** set (because `isPaceType(null)` is false).
- "Is this a pace skill set?" → `bowl.contains(BowlK::pace)` (JS: `base.bowl.pace !== undefined`). Super Over bowler pick uses `bowl.contains(BowlK::death)`.
- Fielding: all 4 always present (non-keepers have a low `keeping`).
- JSON: write only present keys, keys in the JS definition order.

---

## 4. Player

One struct serves the world save and the copies inside a match.

```cpp
struct Contract   { double cost; double salary; int years; ContractVia via; int signed_; }; // JSON key "signed"
struct Injury     { std::string type, label; int matches; int year; std::optional<PlayerId> replacedBy; };
struct Retired    { int year; int age; std::optional<TeamId> team; bool quiet = false; };
struct Drafted    { int year; int no; TeamId team; };
struct HistoryPt  { int year; int age; double bat, bowl, field; };
struct SkillFrac  { std::map<std::string,double> bat, bowl, field; };        // ageing/coaching remainders
struct PreEdit    { std::string name; int age; Skills skills; int year;
                    Role role; Hand hand; std::optional<BowlType> bowlType;
                    std::optional<std::string> bowlStyle; std::optional<SpecialKey> special; };
struct TrainingTag{ DrillKey drill; std::string label; };
struct BaseRatings{ double bat, bowl, field; Skills skills; };              // before development

struct Player {
  // identity
  PlayerId id; std::string name; Role role; Hand hand; int age;
  std::optional<BowlType> bowlType; bool overseas = false;
  // ratings: headline numbers are ALWAYS recomputed from skills (recompute())
  double bat, bowl, field, power /*= skills.bat.lofted*/, awareness /*= skills.bat.awareness*/;
  Skills skills; std::string batStyle; std::optional<std::string> bowlStyle;
  std::optional<SpecialKey> special;                 // chosen in the editor; else auto from best skill

  // world-only
  std::optional<double> potential;
  std::optional<TeamId> team;                        // null = free agent or retired
  Origin origin = Origin::roster;
  std::optional<Retired> retired;
  std::vector<HistoryPt> history;                    // one point per off-season
  std::optional<int> joined;
  std::optional<Contract> contract;
  std::optional<bool> capped;
  std::optional<double> fitness;                     // 0-100, missing = 100
  std::optional<Injury> injury;
  SkillFrac skillFrac;
  bool edited = false; std::optional<PreEdit> preEdit;
  std::optional<Drafted> drafted;
  std::optional<TeamId> formerTeam;                  // set on release, cleared after the auction
  int unsoldYears = 0; int coachLift = 0;

  // transient (built for screens and matches; never in the world save)
  std::optional<Skills> devBase;                     // skills development is measured from
  std::optional<BaseRatings> base;                   // before development (applyProgress)
  bool unavailable = false;                          // injured
  std::optional<double> startConf;                   // from form (+4 if trained)
  std::optional<TrainingTag> training;               // match-only drill
  bool tired = false;                                // fatigue applied
  bool impactIn = false, impactOut = false;          // Impact Player flags in a match
};
```

`mainRating(p)` = `bowl` for BOWL, `round((bat+bowl)/2)` for AR, else `bat`.

---

## 5. Match

```cpp
struct Toss        { std::optional<std::string> call, coin; int winner; std::optional<std::string> decision; }; // "bat"/"bowl"
struct ImpactSub   { PlayerId out, in; std::string when; };
struct MatchTeam   { TeamId id; std::string name; std::string color; PlayerId keeperId; CaptainStyle captain;
                     std::optional<PlayerId> captainId; std::vector<PlayerId> players /*XI in batting order*/;
                     std::vector<PlayerId> bench /*5 named subs*/; std::optional<ImpactSub> impact; };
struct MatchSettings { double aggression = 3; bool autoBowl = false, autoBat = false, autoReview = false;
                       std::map<PlayerId, std::optional<TargetMode>> targets;   // by bowler
                       std::map<PlayerId, Tempo> batterTempo; };                // by batter
struct Rain        { RainWhen when; int overs = 0 /*pre*/; int atOver = 0; int lost = 0; bool abandon = false; };
struct Weather     { Sky sky; std::optional<Rain> rain; double rainChance; bool rainDone = false; };
struct Dls         { bool reduced=false; std::optional<std::string> note; std::optional<double> r1, r2;
                     std::optional<int> target, firstInnings, par; bool abandoned=false, noResult=false; };
struct Fans        { std::optional<int> home; int capacity; std::array<int,2> base, now, overStart;
                     std::array<double,2> mood; std::set<std::string> told; };
struct Result      { std::optional<int> winner; bool superOver=false, tied=false, dls=false, noResult=false; std::string text; };
struct FixtureRef  { int no; std::string seasonId; int seasonNo; Stage stage; std::string venue; TeamId home; };
struct SoHistory   { std::map<int, OrderedMap<PlayerId,bool>> bat; std::map<int, PlayerId> bowl; int secondBatting; };

struct Match {
  std::string id; int32_t seed; int32_t rngState; Pitch pitch;
  std::optional<int> userTeam;                       // 0/1, or none for AI v AI
  OrderedMap<PlayerId, Player> players;              // copies (XI + bench of both sides)
  std::array<MatchTeam,2> teams;
  std::optional<Toss> toss; std::optional<int> battingFirst;
  MatchStatus status = MatchStatus::toss; int current = 0;
  std::vector<Innings> innings;
  Awaiting awaiting = Awaiting::none; std::optional<BallDecision> pendingBall;
  MatchSettings settings;
  std::optional<Result> result; std::optional<PlayerId> potm;
  int commentarySeq = 0; bool dew = false; Weather weather; int oversPerSide = 20; std::optional<Dls> dls;
  Fans fans; std::unordered_map<std::string,double> h2h;   // "bat|bowl" -> edge -1..+1
  std::optional<FixtureRef> fixture; std::optional<TeamId> managed; bool quiet = false;
  std::optional<SoHistory> soPrev; std::vector<std::string> impactNotes;
};
```

### Innings

```cpp
struct Extras       { int wd=0, nb=0, lb=0, b=0; };
struct ReviewsLeft  { int bat=2, bowl=2; };
struct ReviewRecord { int ball; int side; PlayerId batter, bowler; DismissalType type; std::string given; bool overturned; };
struct Dismissal    { DismissalType type; std::optional<PlayerId> bowler, fielder; };
struct BatterInnings{ int runs=0, balls=0, fours=0, sixes=0, dots=0; bool out=false; std::optional<std::string> howOut;
                      double conf; bool zone=false; std::optional<Dismissal> dismissal; };
struct BowlingFigures{ int balls=0, runs=0, wickets=0, maidens=0, dots=0, fours=0, sixes=0, wides=0, noballs=0;
                       double rhythm=50; std::optional<int> specialAt; int specials=0; };
struct Ball {
  std::string label;        // "•", "1", "4", "6", "W", "wd", "2wd", "nb", "4nb", "1lb", "1b"
  BallKind kind; std::optional<PlayerId> bat, faced; int runs=0;
  std::optional<DismissalType> wicketType; std::optional<SpecialKey> special;
  std::optional<std::pair<std::string,bool>> review; // {by: "bat"/"bowl", overturned}
  std::optional<PositionKey> pos; int saved=0 /*runs the shot was worth if cut off*/; int overthrow=0;
  bool legBye=false, bye=false, drop=false, wide=false;
  std::optional<double> dir, dist;                    // shot placement (degrees, share of boundary)
};
struct Over         { PlayerId bowler; std::vector<Ball> balls; int runs=0, bowlerRuns=0, wickets=0; };
struct FallOfWicket { int runs, wicket; PlayerId playerId; std::string over; int ball; bool announced=false; };
struct Partnership  { int runs=0, balls=0; };
struct CommentaryLine { int id; std::string over; CommentaryKind kind; std::string text; };
struct Mindset      { PlayerId batter; double instructed, tempo; std::string label; };
struct Drop         { PlayerId fielder, batter; int ball; std::string type; };   // "catch"/"stumping"
struct UserField    { FieldPreset preset; std::optional<std::vector<PositionKey>> positions; };
struct FieldNow     { FieldPreset preset; std::vector<PositionKey> positions; std::string key;
                      std::map<PositionKey, PlayerId> assign; };
struct LastPlan     { PlayerId bowler; PlanKey plan; bool fits; double read; };
struct PlanStreak   { PlanKey plan; int n; };

struct Innings {
  int batting, bowlingSide; int maxBalls, maxWickets; std::optional<int> target;
  int runs=0, wickets=0, legalBalls=0; Extras extras; ReviewsLeft reviewsLeft; std::vector<ReviewRecord> reviews;
  OrderedMap<PlayerId, BatterInnings> batters;           // every batter of the side, in order
  std::vector<PlayerId> battedOrder, yetToBat;
  std::optional<PlayerId> striker, nonStriker;
  std::vector<PlayerId> bowlingOrder; OrderedMap<PlayerId, BowlingFigures> bowling;
  std::optional<PlayerId> currentBowler, lastOverBowler;
  std::vector<Over> overs; std::vector<FallOfWicket> fow; Partnership partnership;
  bool freeHit=false; std::vector<CommentaryLine> commentary; bool complete=false;
  std::optional<Mindset> mindset; std::vector<Drop> drops;
  int superOver = 0;                                      // 0 = normal innings, n = n-th Super Over
  std::map<PlayerId, PlanKey> plans;                      // user's plan per bowler
  std::map<PlayerId, PlanStreak> planStreak;              // per batter
  std::optional<UserField> userField; std::optional<FieldNow> fieldNow; std::optional<LastPlan> lastPlan;
};
```

### The pending ball (`match.pendingBall`, the DRS wait)

```cpp
struct Umpire  { DismissalType type; std::string truth, given; std::string reviewer; bool sure, reviewed=false, overturned=false; };
struct Shot    { double dir, dist; std::optional<PositionKey> pos; bool rope=false; int hit=0; };
struct BallDecision {
  std::string outcome;                 // "0","1","2","3","4","6","W"
  std::optional<DismissalType> type; std::optional<PlayerId> fielderId; std::optional<std::string> chanceMissed;
  std::optional<SpecialKey> special; bool wasFreeHit; PlanKey plan; bool fits; double h2hEdge;
  std::optional<Shot> shot; std::optional<std::string> extra /*"lb"/"b"*/; int overthrow=0;
  std::optional<Umpire> umpire; int overIdx;
};
```

---

## 6. Season

```cpp
struct InningsSummary {
  TeamId team; int runs, wickets, balls; bool allOut; int maxBalls; int extras;
  std::vector<std::array<int,2>> overs;                   // [runs, wickets] per over
  std::vector<std::tuple<PlayerId,int,int,int,int,int,int>> batting;   // id, runs, balls, 4s, 6s, out(0/1), dots
  std::vector<std::tuple<PlayerId,int,int,int,int,int,int,int,int,int>> bowling; // id, balls, runs, wkts, mdns, dots, 4s, 6s, wd, nb
};
struct FieldingLine { int ct=0, st=0, ro=0, drops=0; };
struct MatchSummary {
  std::string matchId; std::optional<TeamId> winner; std::string text; bool noResult, dls;
  std::optional<std::vector<std::tuple<TeamId,int,int>>> superOver;
  std::optional<PlayerId> potm; std::optional<std::pair<TeamId,std::string>> toss; bool dew;
  std::vector<InningsSummary> innings; OrderedMap<PlayerId, FieldingLine> fielding;
  std::vector<std::tuple<TeamId,PlayerId,PlayerId>> impact;           // team, out, in
};
struct Fixture { int no; Stage stage; TeamId home, away; std::string venue; Pitch pitch; int32_t seed;
                 std::optional<MatchSummary> result; };
struct Season  { std::string id /*"S{n}-{seed}"*/; int number; int32_t seed; TeamId userTeam;
                 std::vector<TeamId> groupA, groupB;                   // JSON: groups { A, B }
                 std::map<TeamId, std::pair<std::string,Pitch>> venues;
                 std::vector<Fixture> fixtures;
                 std::optional<TeamId> champion, runnerUp; std::optional<std::vector<TeamId>> top4; };
struct SeasonArchive { int number; TeamId champion, runnerUp, userTeam; std::optional<int> userPos;
                       std::optional<std::pair<PlayerId,int>> orangeCap, purpleCap; std::optional<std::pair<PlayerId,double>> mvp; };
struct Busy { std::string label; int done; };
struct SeasonState { std::optional<Season> season; std::vector<SeasonArchive> history; std::optional<Busy> busy /*not saved*/; };
```

Note: the JS save writes `batting`/`bowling` rows as JSON arrays. Keep that in C++ JSON so saves stay compatible.

---

## 7. Career

```cpp
struct BatCareer  { int inns=0, notOuts=0, runs=0, balls=0, fours=0, sixes=0, hs=0; bool hsNotOut=false; int fifties=0, hundreds=0, ducks=0; };
struct BowlCareer { int inns=0, balls=0, runs=0, wickets=0, maidens=0, dots=0, bestW=0, bestR=0, fourW=0; };
struct FieldCareer{ int catches=0, stumpings=0, runOuts=0, drops=0; };
struct RecentLine { TeamId vs; std::optional<std::string> bat, bowl; char result; /*W/L/T*/ };
struct CareerStats{ int matches=0, wins=0, potm=0; BatCareer bat; BowlCareer bowl; FieldCareer field;
                    std::vector<RecentLine> recent; /* newest first, max 5 */ };
struct Progress   { std::map<std::string,double> bat, bowl, field; };   // development deltas, capped ±8% (min 3)
struct Popularity { double v, d; };
struct ManagerRecord { int played=0, wins=0, streak=0, bestStreak=0;
                       std::map<std::string, std::pair<std::string,TeamId>> unlocked; };  // key -> {when, vs}

struct Career {
  OrderedMap<PlayerId, CareerStats> players;
  std::vector<std::string> recorded;                 // match ids already recorded, max 500
  std::map<PlayerId, Progress> progress;
  std::optional<LastReport> lastReport;              // { matchId, reports[] } development report
  Records records;                                   // 9 top-10 lists (records.js)
  std::map<PlayerId, Popularity> popularity;
  std::map<std::string, std::array<int,5>> matchups; // "bat|bowl" -> [balls, runs, outs, 4s, 6s]
  std::vector<MilestoneLog> milestones;              // newest 50 across the league
  std::optional<LastMilestones> lastMilestones; ManagerRecord manager; std::optional<LastUnlocked> lastUnlocked;
};
```

Records lists (each max 10): `highScores`, `bestBowling`, `fastestFifties`, `fastestHundreds`, `mostSixes`, `highTotals`, `lowTotals`, `bigWinsRuns`, `bigWinsWkts`. Each entry carries `matchId`, `when` (`"Season 3"` or `"Friendly"`), ids and numbers (see `records.js`).

---

## 8. World

```cpp
struct WorldTeam   { std::vector<PlayerId> squad; int rtm = 0;
                     std::optional<PlayerId> captainId; };  // club captain; only counts while he is in the squad (clubCaptainOf)
struct Target      { std::string key, label; int need; int rank; int season; };
struct BoardEntry  { int season; std::optional<TeamId> team; std::optional<std::string> target; std::optional<int> position;
                     std::optional<std::string> verdict; double delta=0, confidence=0; bool inRed=false;
                     std::optional<std::string> event; std::optional<TeamId> from, to; };   // event: "moved"/"sacked"
struct Sacked      { int season; TeamId team; std::vector<TeamId> offers; std::vector<TeamId> order; };
struct Manager     { std::optional<TeamId> team; double confidence = 60; std::optional<Target> target;
                     std::vector<BoardEntry> history; std::optional<Sacked> sacked; };
struct Offseason   { int year; std::string stage; /* contracts|retention|draft|auction */ bool mega;
                     std::vector<TeamId> order; TeamId userTeam; std::string then; /* season|players */
                     std::vector<PlayerId> retain; };
struct DraftPick   { TeamId team; std::optional<PlayerId> id; int no; bool skipped = false; };
struct Draft       { int year; std::vector<PlayerId> pool; std::vector<TeamId> order /*20 picks*/;
                     std::vector<DraftPick> made; std::set<PlayerId> scouted; int scoutsLeft; TeamId userTeam; std::string then; };
struct Bid         { TeamId team; double price; };
struct Lot         { PlayerId id; double base; std::optional<double> price; std::optional<TeamId> leader;
                     std::vector<Bid> bids; std::set<TeamId> out; std::string stage; /* bidding|rtm-invoke|rtm-raise|rtm-match */
                     OrderedMap<TeamId,double> vals; std::optional<TeamId> rtmTeam; std::optional<double> userMax; };
struct Sale        { PlayerId id; TeamId team; double price; bool rtm; int no; std::string set; std::optional<TeamId> former; };
struct Auction     { int year; std::string type; /* mini|mega */ std::vector<PlayerId> lots;
                     std::map<PlayerId,std::string> setOf; int idx = 0; std::optional<Lot> lot;
                     std::vector<Sale> results; std::vector<PlayerId> unsold; TeamId userTeam;
                     bool userAuto = false; std::vector<std::string> log; };
struct Transfer    { int year; std::string kind; /* trade|replacement */ TeamId team; std::optional<TeamId> with;
                     std::vector<PlayerId> got, gave; double cash = 0; std::optional<PlayerId> for_; std::string text; };
struct InjuryNews  { PlayerId id; TeamId team; std::string label; int matches; };

struct World {
  int version = 3; int year = 0; int nextId = 1;
  OrderedMap<PlayerId, Player> players;              // everyone ever: squads, free agents, retired
  OrderedMap<TeamId, WorldTeam> teams;
  std::optional<Draft> draft; std::optional<Auction> auction; std::optional<Offseason> offseason;
  std::optional<OffseasonReport> lastOffseason;       // see world.js runOffseason `report`
  Manager manager; std::map<TeamId, Club> clubs;
  std::vector<Transfer> transfers;                    // newest first, max 40
  std::vector<InjuryNews> lastInjuries;               // newest first, max 20
};
```

`OffseasonReport` = `{ year, mega, aged[], retired[], released[], renewed[], retained[], academy[], drafted[], signed[], auction, coaching[] }` (exact row shapes in `world.js` `runOffseason` and `club.js` `applyCoaching`).

---

## 9. Club (business)

```cpp
struct Building  { int level; int ready; double cost; };
struct ClubPlan  { std::string tier = "standard"; std::optional<Pitch> pitch; int prizeShare = 0; };
struct Staff     { int bat = 0, bowl = 0, field = 0; };          // index into STAFF_TIERS
struct Sponsor   { std::string slot, brand, type; double fee; std::optional<std::pair<double,double>> bonus; int years, from; };
struct Coach     { PlayerId playerId; std::string area; int tier; int from; std::optional<int> ends, paid; };
struct Gate      { int year, no; TeamId opp; Stage stage; int att, cap; double revenue; };
struct Books     { std::map<std::string,double> income, spend; };
struct Club {
  double bank = 3000; double popularity;              // starts at BASE_POPULARITY[team]
  std::map<std::string,int> levels;                    // stands, hospitality, fanzone, curator, academy, hpc
  std::map<std::string,Building> building;
  ClubPlan plan, active; Staff staffPlan, staff; std::optional<int> staffPaid;
  std::vector<Sponsor> sponsors; std::vector<Coach> coaches;
  std::optional<std::pair<int,double>> goodwill;       // {year, pct off renewals}
  bool inSeason = false; std::optional<int> activatedYear, closedYear;
  std::map<int,Books> books;                           // last 10 years
  std::vector<Gate> gates;                             // last 40
  std::vector<ClubSeasonSummary> history;              // last 10
  std::optional<CoachingReport> lastCoaching;
  std::vector<std::pair<int,std::string>> log;         // newest first, max 30
};
```

Money is in **lakh** everywhere (₹1 crore = 100 lakh). Format with `money()` (see [08 §8](08-ui-design-system.md#8-formatting-helpers-corejsmathhpp-uiformatcpp)).

---

## 10. Setup, UI and settings

```cpp
struct TrainingSlot { PlayerId playerId; DrillKey drill; };
struct Lineup       { std::vector<PlayerId> xi; std::optional<PlayerId> keeperId, captainId; };
struct MatchPrefs   { double aggression = 3; bool autoBowl = false; };
struct SetupState {
  Screen screen = Screen::home; Screen returnTo = Screen::home;
  TeamId userTeamId, oppTeamId; std::string pitch = "random";
  std::vector<TrainingSlot> training;                  // max 3
  std::optional<FixtureRef> fixture;                   // season fixture being prepared
  std::vector<PlayerId> xi; std::optional<PlayerId> keeperId, captainId;
  std::map<TeamId, Lineup> lineups;                    // remembered per team
  MatchPrefs matchSettings;
};
struct UiState { std::optional<PlayerId> cardPlayerId; bool cardEdit = false; };

struct GameState {
  SetupState setup; std::optional<Match> match; Career career; UiState ui; SeasonState season; World world;
};
```

`Settings` (was `localStorage t20coach.ui.*`): `commentaryOn`, `commentary` (expanded), `commentaryKeyOnly`, `fieldView` (expanded), `fielders`, `watchSpeed` (`slow|normal|fast`), `watchStopWicket`, `fold.field`, `fold.target`; plus new desktop ones: window size/position, UI scale, theme (dark/light).
