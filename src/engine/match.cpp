#include "engine/match.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include "core/str.hpp"
#include "engine/ai.hpp"
#include "engine/commentary.hpp"
#include "engine/field.hpp"
#include "engine/league.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "engine/shots.hpp"
#include "engine/skills.hpp"
#include "engine/special.hpp"
#include "engine/tactics.hpp"
#include "engine/weather.hpp"

namespace t20 {

// ---------------------------------------------------------------------------
// Probability model
// ---------------------------------------------------------------------------

namespace {
// Baseline for a legal delivery between evenly matched players in the middle overs
constexpr std::array<double, 7> BASE = {0.34, 0.392, 0.07, 0.006, 0.142, 0.055, 0.0405};

struct PhaseMod {
  double o0, o1, o2, o4, o6, W;
};
const PhaseMod& phaseMods(Phase p) {
  static const PhaseMod mods[] = {
      {1.0, 0.92, 0.9, 1.3, 0.95, 0.92},   // pp
      {1.0, 1.08, 1.0, 0.88, 0.95, 0.85},  // mid
      {0.84, 1.0, 1.2, 1.12, 1.5, 1.3},    // death
  };
  return mods[static_cast<size_t>(p)];
}

using O = Outcome;
}  // namespace

const std::vector<AggressionLevel> AGGRESSION = {
    {1, "Defend"}, {2, "Careful"}, {3, "Balanced"}, {4, "Attack"}, {5, "All-out"}};

Outcome weightedOutcome(RandFn rng, const OutcomeWeights& w) {
  const size_t n = w.hasW ? 7 : 6;
  double total = 0;
  for (size_t i = 0; i < n; ++i) total += w.w[i];
  double x = rng() * total;
  for (size_t i = 0; i < n; ++i) {
    x -= w.w[i];
    if (x <= 0) return static_cast<Outcome>(i);
  }
  return static_cast<Outcome>(n - 1);
}

OutcomeWeights outcomeWeights(const OutcomeInputs& in) {
  OutcomeWeights w;
  w.w = BASE;
  const bool spin = isSpinner(*in.bowler);
  const BatterNow& B = in.B;
  const BowlerNow& T = in.T;

  // Skill battle, one dimension at a time
  w[O::W] *= js_exp((-1.15 * (B.survive - T.threat)) / 100);
  w[O::o0] *= js_exp((-1.0 * (B.rotate - T.control)) / 100);
  w[O::o1] *= js_exp((0.45 * (B.rotate - T.control)) / 100);
  w[O::o2] *= js_exp((0.5 * (B.running - 60)) / 100);
  w[O::o3] *= js_exp((1.0 * (B.running - 60)) / 100);
  w[O::o4] *= js_exp((1.1 * (B.hit4 - T.control)) / 100);
  w[O::o6] *= js_exp((1.1 * (B.hit6 - T.control)) / 100) * js_exp((B.lofted - 60) / 50);

  // The right bowler for the moment
  if (in.edge) {
    w[O::W] *= in.edge->wicket;
    w[O::o4] *= in.edge->boundary;
    w[O::o6] *= in.edge->boundary;
  }
  // Fielding side: good ground fielders save boundaries and twos
  if (in.field) {
    w[O::o4] *= js_exp((-0.5 * (in.field->ground - 65)) / 100);
    w[O::o2] *= js_exp((-0.4 * (in.field->ground - 65)) / 100);
  }
  // New at the crease: still reading the pace and bounce
  if (in.ballsFaced < 6) {
    w[O::W] *= 1.25;
    w[O::o4] *= 0.8;
    w[O::o6] *= 0.7;
  }
  // Very long innings: legs get heavy
  if (in.ballsFaced > 40) w[O::W] *= 1 + (in.ballsFaced - 40) * 0.028;
  // Nervous nineties: temperament decides how much they bite
  if (in.runsScored >= 85 && in.runsScored < 100) w[O::W] *= 1 + 0.7 * (1 - B.temperament / 100);

  // Confidence: a batter in the zone is hard to get out and finds the rope
  const double c = (in.conf - 50) / 50;
  w[O::W] *= js_exp(-0.35 * c);
  w[O::o0] *= js_exp(-0.1 * c);
  w[O::o4] *= js_exp(0.15 * c);
  w[O::o6] *= js_exp(0.2 * c);

  // Bowler rhythm
  const double rh = (in.rhythm - 50) / 50;
  w[O::W] *= js_exp(0.12 * rh);
  w[O::o4] *= js_exp(-0.15 * rh);
  w[O::o6] *= js_exp(-0.15 * rh);

  // Match phase
  const PhaseMod& pm = phaseMods(in.phase);
  w[O::o0] *= pm.o0;
  w[O::o1] *= pm.o1;
  w[O::o2] *= pm.o2;
  w[O::o4] *= pm.o4;
  w[O::o6] *= pm.o6;
  w[O::W] *= pm.W;

  // Batting tempo: -2 (defend) .. +2 (all-out), continuous
  const double a = in.tempo - 3;
  w[O::o0] *= js_exp(-0.1 * a);
  w[O::o1] *= js_exp(-0.02 * a);
  w[O::o4] *= js_exp(0.18 * a);
  w[O::o6] *= js_exp(0.34 * a);
  w[O::W] *= js_exp((a < 0 ? 0.38 : 0.3) * a);

  // Live conditions
  w[O::o4] *= in.cond.boundary;
  w[O::o6] *= in.cond.boundary;
  w[O::o0] *= in.cond.dot;
  w[O::W] *= in.cond.wicket;

  // Bowling type by phase
  if (spin && in.phase == Phase::pp) {
    w[O::o4] *= 1.1;
    w[O::o6] *= 1.05;
  }
  if (spin && in.phase == Phase::mid) {
    w[O::W] *= 1.05;
    w[O::o0] *= 1.04;
    w[O::o4] *= 0.92;
  }
  if (in.freeHit) {  // only a run out can dismiss
    w[O::o6] *= 1.8;
    w[O::o4] *= 1.3;
    w[O::o0] += w[O::W] * 0.9;
    w[O::W] *= 0.05;
  }
  return w;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

namespace {

// DRS: the umpire is good but not perfect
constexpr double UMPIRE_MISS_LBW = 0.12;  // a plumb lbw given not out
constexpr double UMPIRE_MISS_CK = 0.1;    // a feather given not out
constexpr double FALSE_OUT = 0.006;       // a dot ball wrongly given out
constexpr double BIG_APPEAL = 0.02;       // a big shout on a dot ball, rightly turned down

bool isUser(const Match& m, int side) { return m.userTeam && *m.userTeam == side; }

const Player& P(const Match& m, const PlayerId& id) { return m.players.at(id); }

void addComment(Match& match, const std::string& over, CommentKind kind, std::string text) {
  if (match.quiet) return;  // a copy simmed for win probability: nobody reads it
  Innings& inn = inningsOf(match);
  inn.commentary.push_back({++match.commentarySeq, over, kind, std::move(text)});
}

// Byes: the keeper's glovework against the league average (spinners are easier to take)
double byeChance(const Player& keeper, bool spin) {
  return 0.007 * js_exp((65 - keeper.skills.field[FieldK::keeping]) / 22) * (spin ? 0.6 : 1);
}

double round2(double x) { return js_round(x * 100) / 100; }

void swapStrike(Innings& inn) { std::swap(inn.striker, inn.nonStriker); }

BatterInnings freshBatter(double conf) {
  BatterInnings b;
  b.conf = conf;
  return b;
}

void prepareNext(Match& match, bool autoPick);
void startInnings(Match& match, int idx);
void afterBall(Match& match, bool legal, bool wicket, int batRuns);
void endInnings(Match& match);
void startSuperOver(Match& match);
void startSuperOverInnings(Match& match, int side, std::optional<int> target, int n);

void startMatch(Match& match) {
  const Toss& toss = *match.toss;
  const int firstBat = toss.decision == TossDecision::bat ? toss.winner : 1 - toss.winner;
  match.battingFirst = firstBat;
  // Rain before the start: a shortened game
  const std::optional<Rain> rain = match.weather.rain;
  if (rain && rain->when == RainWhen::pre) {
    match.oversPerSide = rain->overs;
    Dls dls;
    dls.reduced = true;
    dls.note = cat("Rain delayed the start: ", rain->overs, " overs a side.");
    match.dls = dls;
    match.weather.rainDone = true;
  }
  startInnings(match, 0);
  if (match.dls && !match.dls->note.empty() && match.current == 0)
    addComment(match, "0.0", CommentKind::info,
               cat(match.dls->note, " Each bowler can bowl ", static_cast<int>(std::ceil(rain->overs / 5.0)), " overs."));
}

Innings newInnings(const Match& match, int idx) {
  Innings inn;
  inn.batting = idx == 0 ? *match.battingFirst : 1 - *match.battingFirst;
  inn.bowlingSide = 1 - inn.batting;
  const auto& order = match.teams[static_cast<size_t>(inn.batting)].players;
  inn.batters.reserve(order.size() + 2);
  for (const PlayerId& id : order) inn.batters.set(id, freshBatter(P(match, id).startConf.value_or(START_CONFIDENCE)));
  inn.maxBalls = match.oversPerSide * 6;
  // a side short of 11 is all out when it runs out of batters
  inn.maxWickets = std::min(10, static_cast<int>(order.size()) - 1);
  if (idx == 1) inn.target = match.dls && match.dls->target ? *match.dls->target : match.innings[0].runs + 1;
  inn.reviewsLeft = {REVIEWS, REVIEWS};
  inn.battedOrder = {order[0], order[1]};
  inn.yetToBat.assign(order.begin() + 2, order.end());
  inn.striker = order[0];
  inn.nonStriker = order[1];
  return inn;
}

void startInnings(Match& match, int idx) {
  match.current = idx;
  Innings inn = newInnings(match, idx);
  if (static_cast<int>(match.innings.size()) <= idx) match.innings.resize(static_cast<size_t>(idx) + 1);
  match.innings[static_cast<size_t>(idx)] = std::move(inn);
  match.status = MatchStatus::live;
  const Innings& cur = inningsOf(match);
  const MatchTeam& team = match.teams[static_cast<size_t>(cur.batting)];
  const Player& a = P(match, *cur.striker);
  const Player& b = P(match, *cur.nonStriker);
  if (idx == 0) {
    addComment(match, "0.0", CommentKind::info,
               cat(team.name, " will bat first. ", a.name, " and ", b.name, " walk out to open."));
  } else {
    addComment(match, "0.0", CommentKind::info,
               cat(team.name, " need ", *cur.target, " to win from ", static_cast<int>(std::ceil(cur.maxBalls / 6.0)),
                   " overs", match.dls && match.dls->target ? " (DLS)" : "", ". ", a.name, " and ", b.name, " to open."));
  }
  if (idx == 1) {
    std::string_view note;
    if (match.pitch == Pitch::dusty) note = "The surface is breaking up. The spinners should get even more turn now.";
    if (match.pitch == Pitch::green) note = "The grass has dried out. Less movement for the seamers this time.";
    if (match.pitch == Pitch::slow) note = "The pitch has slowed up further. Timing will be even harder.";
    if (!note.empty()) addComment(match, "0.0", CommentKind::info, std::string(note));
    if (match.dew)
      addComment(match, "0.0", CommentKind::info,
                 "Dew is expected later in this innings, which should help the chasing side.");
    for (const std::string& n : match.impactNotes) addComment(match, "0.0", CommentKind::info, n);
    match.impactNotes.clear();
  }
  prepareNext(match, false);
}

void placeBatter(Match& match, const PlayerId& id) {
  Innings& inn = inningsOf(match);
  inn.yetToBat.erase(std::remove(inn.yetToBat.begin(), inn.yetToBat.end(), id), inn.yetToBat.end());
  inn.battedOrder.push_back(id);
  // Walking in during a collapse knocks a batter's belief
  if (wicketsInLast(inn, 18) >= 2) {
    BatterInnings& b = inn.batters.at(id);
    b.conf = std::max(20.0, b.conf - 4);
  }
  if (!inn.striker)
    inn.striker = id;
  else
    inn.nonStriker = id;
  inn.partnership = {0, 0};
  addComment(match, oversText(inn.legalBalls), CommentKind::info, cat(P(match, id).name, " comes to the crease."));
}

void assignBowler(Match& match, const PlayerId& id) {
  Innings& inn = inningsOf(match);
  inn.currentBowler = id;
  if (!inn.bowling.contains(id)) {
    inn.bowling.set(id, BowlingFigures{});
    inn.bowlingOrder.push_back(id);
  }
  const size_t ov = static_cast<size_t>(overIndex(inn));
  Over fresh;
  fresh.bowler = id;
  if (inn.overs.size() <= ov) inn.overs.resize(ov + 1);
  inn.overs[ov] = std::move(fresh);
  if (match.current == 1 && match.dew && overIndex(inn) == 8)
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               "The dew has arrived. The ball is getting wet and the spinners will struggle to grip it.");
  if (match.current == 1 && match.dew && overIndex(inn) == 10)
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               "Two-ball rule: with the dew heavy, the umpires bring out a second, drier ball after the 10th over.");
}

// Fill any gap (new batter, new bowler) either by asking the user or via the AI.
void prepareNext(Match& match, bool autoPick) {
  if (match.status != MatchStatus::live) return;
  Innings& inn = inningsOf(match);
  if (inn.superOver) autoPick = true;  // Super Over line-ups are set up front
  const bool userBat = isUser(match, inn.batting);
  const bool userBowl = isUser(match, inn.bowlingSide);
  if (!inn.striker || !inn.nonStriker) {
    if (userBat && !autoPick && !match.settings.autoBat) {
      match.awaiting = Awaiting::batter;
      return;
    }
    placeBatter(match, aiNextBatter(match));
  }
  if (!inningsOf(match).currentBowler) {
    if (userBowl && !autoPick && !match.settings.autoBowl) {
      match.awaiting = Awaiting::bowler;
      return;
    }
    assignBowler(match, aiChooseBowler(match));
  }
  match.awaiting = Awaiting::none;
  refreshMindset(match);
}

// The captain sets the tempo; the batter adjusts it to his own reading of the game.
double currentTempo(Match& match, Innings& inn) {
  if (inn.superOver) {
    std::optional<double> need;
    if (inn.target) need = *inn.target - inn.runs;
    const double left = inn.maxBalls - inn.legalBalls;
    const double tempo = !need                                    ? 5
                         : *need / std::max(1.0, left) > 1.3 ? 5
                         : *need / std::max(1.0, left) > 0.8 ? 4
                                                                   : 3;
    inn.mindset = MindsetNow{*inn.striker, tempo, tempo, "Super Over: every ball counts"};
    return tempo;
  }
  const bool userBatting = isUser(match, inn.batting);
  // each batter has his own instruction
  const Tempo own = userBatting ? batterTempoOf(match, *inn.striker) : std::nullopt;
  double instructed = own ? *own : aiAggression(match);
  if (userBatting) {
    // The captain's word on this bowler: go after him, or see him off
    auto it = match.settings.targets.find(*inn.currentBowler);
    if (it != match.settings.targets.end() && it->second) {
      if (*it->second == TargetMode::attack) instructed = std::min(5.0, instructed + 1);
      if (*it->second == TargetMode::block) instructed = std::max(1.0, instructed - 1.5);
    }
  } else {
    const Captain style = match.teams[static_cast<size_t>(inn.batting)].captain;
    instructed = std::max(1.0, std::min(5.0, instructed + styleTempo(style)));
    // Shrewd captains go after the part-timers
    if (style == Captain::shrewd && P(match, *inn.currentBowler).bowl < 55) instructed = std::min(5.0, instructed + 0.8);
  }
  Mindset m = computeMindset(match, inn, *inn.striker, instructed);
  inn.mindset = MindsetNow{*inn.striker, instructed, m.tempo, std::move(m.label)};
  return m.tempo;
}

struct UmpireText {
  std::optional<std::string> lead, after;
};

// The commentary for a ball with an umpire's decision in it
std::optional<UmpireText> umpireText(const Match& match, const Innings& inn, const Umpire& u) {
  const Player& bowler = P(match, *inn.currentBowler);
  const Player& striker = P(match, *inn.striker);
  const std::string lead = cat(surname(bowler.name), " to ", surname(striker.name));
  const std::string_view shout =
      u.type == Out::lbw ? "rapped on the pad, big appeal for lbw" : "past the edge, big appeal for caught behind";
  const std::string_view miss = u.type == Out::lbw ? "missing leg stump" : "no bat on it";
  const std::string& batTeam = match.teams[static_cast<size_t>(inn.batting)].name;
  const std::string& bowlTeam = match.teams[static_cast<size_t>(inn.bowlingSide)].name;
  UmpireText t;
  if (u.given == "out") {
    if (u.reviewed && u.overturned) {
      t.lead = cat(lead, ", ", shout, "... and the finger goes up! ", surname(striker.name), " reviews straight away: ", miss,
                   ". Overturned, he survives.");
      return t;
    }
    if (u.reviewed) {
      t.after = cat(" He reviewed, but the replay backs the umpire, and ", batTeam, " lose a review.");
      return t;
    }
    if (u.truth == "notOut") {
      t.after = cat(" Replays suggest that was ", miss, ": a poor decision",
                    inn.reviewsLeft.bat ? "" : ", and no reviews left to save him", ".");
      return t;
    }
    return std::nullopt;
  }
  if (u.reviewed && u.overturned) {
    t.lead = cat(lead, ", ", shout, "... not out, says the umpire. ", bowlTeam, " review, and it's OUT! The decision is overturned.");
  } else if (u.reviewed) {
    t.lead = cat(lead, ", ", shout, ". Not out. ", bowlTeam, " review... still not out, and the review is lost.");
  } else if (u.truth == "out") {
    t.lead = cat(lead, ", ", shout, "... not out! Replays suggest that was out",
                 inn.reviewsLeft.bowl ? cat("; ", bowlTeam, " chose not to review") : std::string(", but they had no reviews left"),
                 ".");
  } else {
    t.lead = cat(lead, ", ", shout, ". Not out.");
  }
  return t;
}

struct DismissResult {
  std::string howOut;
  bool bowlerCredit;
};

// Record a dismissal (the ball itself is recorded by the caller)
DismissResult dismiss(Innings& inn, const PlayerId& outId, Out type, const Player& bowler,
                      const Player* fielder, const Player& keeper, Over& thisOver) {
  BowlingFigures& fig = inn.bowling.at(bowler.id);
  const bool bowlerCredit = type != Out::runout;
  const std::string b = surname(bowler.name);
  const std::string f = surname(fielder ? fielder->name : std::string());
  std::string howOut;
  switch (type) {
    case Out::bowled: howOut = cat("b ", b); break;
    case Out::lbw: howOut = cat("lbw b ", b); break;
    case Out::caught: howOut = cat("c ", f, " b ", b); break;
    case Out::caughtKeeper: howOut = cat("c †", surname(keeper.name), " b ", b); break;
    case Out::caughtBowled: howOut = cat("c & b ", b); break;
    case Out::stumped: howOut = cat("st †", surname(keeper.name), " b ", b); break;
    case Out::runout: howOut = cat("run out (", f, ")"); break;
  }
  inn.wickets += 1;
  if (bowlerCredit) fig.wickets += 1;
  bowlerEvent(fig, bowlerCredit ? BowlerEvent::wicket : BowlerEvent::run);
  const std::optional<PlayerId> survivor = (inn.striker && *inn.striker == outId) ? inn.nonStriker : inn.striker;
  if (survivor) partnerDismissed(inn.batters.at(*survivor));
  thisOver.wickets += 1;
  BatterInnings& outBat = inn.batters.at(outId);
  outBat.out = true;
  outBat.howOut = howOut;
  Dismissal dis;
  dis.type = type;
  if (bowlerCredit) dis.bowler = bowler.id;
  if (fielder) dis.fielder = fielder->id;
  outBat.dismissal = dis;
  inn.fow.push_back({inn.runs, inn.wickets, outId, oversText(inn.legalBalls), inn.legalBalls, false});
  if (inn.striker && *inn.striker == outId)
    inn.striker.reset();
  else
    inn.nonStriker.reset();
  return {howOut, bowlerCredit};
}

void milestone(Match& match, const Player& player, int before, BatterInnings& bat) {
  for (int m : {50, 100}) {
    if (before < m && bat.runs >= m) {
      milestoneBoost(bat, m);
      addComment(match, oversText(inningsOf(match).legalBalls), CommentKind::milestone,
                 cat(m == 50 ? "Fifty" : "Hundred", " for ", player.name, "! ", bat.runs, " off ", bat.balls, " balls."));
    }
  }
}

void settleReview(Match& match, BallDecision& d, bool reviewed) {
  Innings& inn = inningsOf(match);
  Umpire& u = *d.umpire;
  u.reviewed = reviewed;
  u.overturned = reviewed && u.truth != u.given;
  if (reviewed) {
    if (!u.overturned) {  // a successful review is kept
      if (u.reviewer == "bat")
        inn.reviewsLeft.bat -= 1;
      else
        inn.reviewsLeft.bowl -= 1;
    }
    inn.reviews.push_back({inn.legalBalls + 1, u.reviewer == "bat" ? inn.batting : inn.bowlingSide, *inn.striker,
                           *inn.currentBowler, u.type, u.given, u.overturned});
  }
  const bool out = (reviewed ? u.truth : u.given) == "out";
  if (out) {
    d.outcome = Outcome::W;
    d.type = u.type;
    if (u.type == Out::caughtKeeper)
      d.fielderId = match.teams[static_cast<size_t>(inn.bowlingSide)].keeperId;
    else
      d.fielderId.reset();
  } else {
    d.outcome = Outcome::o0;
    d.type.reset();
    d.fielderId.reset();
  }
}

// The AI side's call: review when sure, now and then on a hunch
bool aiReviews(Match& match, const BallDecision& d) {
  if (d.umpire->sure) return true;
  StateStream stream{&match.rngState};
  return stream() < 0.08;
}

// --- Legal delivery: record it ----------------------------------------------
std::string finishLegalBall(Match& match, const BallDecision& d) {
  Innings& inn = inningsOf(match);
  StateStream stream{&match.rngState};
  RandFn r(stream);
  const Player& bowler = P(match, *inn.currentBowler);
  const Player& striker = P(match, *inn.striker);
  const bool spin = isSpinner(bowler);
  BowlingFigures& fig = inn.bowling.at(bowler.id);
  BatterInnings& bat = inn.batters.at(striker.id);
  Over& thisOver = inn.overs[static_cast<size_t>(overIndex(inn))];
  const std::string tag = ballTag(inn.legalBalls);
  const Player& keeper = P(match, match.teams[static_cast<size_t>(inn.bowlingSide)].keeperId);
  const Player* fielder = d.fielderId ? &P(match, *d.fielderId) : nullptr;
  const double mirror = striker.hand == Hand::L ? -1 : 1;
  std::optional<Placement> placed;
  if (d.shot) placed = Placement{js_round(d.shot->dir * mirror), round2(d.shot->dist)};
  std::optional<BallReview> review;
  if (d.umpire && d.umpire->reviewed) review = BallReview{d.umpire->reviewer, d.umpire->overturned};
  const std::optional<UmpireText> umpire = d.umpire ? umpireText(match, inn, *d.umpire) : std::nullopt;
  inn.legalBalls += 1;
  fig.balls += 1;
  bat.balls += 1;
  inn.partnership.balls += 1;
  const int32_t key = ballKey(match.seed, match.current, inn.legalBalls, static_cast<int>(thisOver.balls.size()));

  if (d.outcome == Outcome::W) {
    const Out type = *d.type;
    int runs = 0;
    PlayerId outId = striker.id;
    if (type == Out::runout) {
      runs = r() < 0.5 ? 1 : 0;
      if (r() < 0.4) outId = *inn.nonStriker;
    }
    // Runs completed before a run out
    inn.runs += runs;
    bat.runs += runs;
    fig.runs += runs;
    thisOver.runs += runs;
    thisOver.bowlerRuns += runs;
    inn.partnership.runs += runs;
    if (runs % 2 == 1) swapStrike(inn);
    if (runs == 0) fig.dots += 1;
    const DismissResult dr = dismiss(inn, outId, type, bowler, fielder, keeper, thisOver);
    const BatterInnings& outBat = inn.batters.at(outId);
    Ball ball;
    ball.label = "W";
    ball.kind = BallKind::wicket;
    ball.bat = outId;
    ball.faced = striker.id;
    ball.runs = runs;
    ball.wicketType = type;
    ball.special = d.special;
    ball.review = review;
    if (d.shot) ball.pos = d.shot->pos;
    const Placement pl = placed ? *placed : shotFor(key, runs, type, P(match, outId).hand);
    ball.dir = pl.dir;
    ball.dist = pl.dist;
    thisOver.balls.push_back(std::move(ball));

    const Player& outPlayer = P(match, outId);
    std::optional<CatchAt> catchAt;
    if (type == Out::caught && d.shot && d.shot->pos) {
      const PositionDef& pd = position(*d.shot->pos);
      catchAt = CatchAt{pd.label, static_cast<int>(pd.zone), d.shot->rope};
    }
    std::string text;
    if (umpire && umpire->lead) {
      text = *umpire->lead;
    } else {
      BallTextArgs a;
      a.bowler = &bowler;
      a.striker = &striker;
      a.runs = runs;
      a.spin = spin;
      a.fielder = fielder;
      a.wicketType = type;
      a.catchAt = catchAt;
      text = ballText(r, a);
    }
    if (d.special) {
      const auto at = text.find(". ");
      if (at != std::string::npos)
        text.replace(at, 2, cat(", the ", lower_ascii(std::string(specialDef(*d.special).label)), "! "));
    }
    std::string line = cat(text, " ", outPlayer.name, " ", dr.howOut, " ", outBat.runs, " (", outBat.balls, "b",
                           outBat.fours ? cat(" ", outBat.fours, "x4") : std::string(),
                           outBat.sixes ? cat(" ", outBat.sixes, "x6") : std::string(), ").");
    if (umpire && umpire->after) line += *umpire->after;
    if (d.fits && outId == striker.id)
      line += cat(" The plan worked: ", lower_ascii(std::string(planByKey(bowler, d.plan).label)),
                  " to a batter who struggles against it.");
    if (dr.bowlerCredit && edgeVerdict(d.h2hEdge) == "bowler")
      line += cat(" ", surname(bowler.name), " has his number: he gets him again.");
    addComment(match, tag, CommentKind::wicket, std::move(line));
    afterBall(match, true, true, runs);
    return "W";
  }

  const int runs = kOutcomeRuns[static_cast<size_t>(d.outcome)];
  const std::optional<std::string>& extra = d.extra;  // "lb" | "b" | none
  const int ot = d.overthrow;
  const int total = runs + ot;
  inn.runs += total;
  thisOver.runs += total;
  inn.partnership.runs += total;
  if (extra) {
    if (*extra == "lb")
      inn.extras.lb += runs;
    else
      inn.extras.b += runs;
    fig.dots += 1;
    batterFacedBall(bat, 0, false, striker.skills.bat[BatK::temperament]);
    bowlerEvent(fig, BowlerEvent::dot);
  } else {
    const int before = bat.runs;
    bat.runs += total;
    fig.runs += total;
    thisOver.bowlerRuns += total;
    if (total == 0) {
      fig.dots += 1;
      bat.dots += 1;
    }
    if (runs == 4) {
      bat.fours += 1;
      fig.fours += 1;
    }
    if (runs == 6) {
      bat.sixes += 1;
      fig.sixes += 1;
    }
    batterFacedBall(bat, total, runs == 4, striker.skills.bat[BatK::temperament]);
    bowlerEvent(fig, total == 0   ? BowlerEvent::dot
                     : runs == 4 ? BowlerEvent::four
                     : runs == 6 ? BowlerEvent::six
                                 : BowlerEvent::run);
    milestone(match, striker, before, bat);
  }
  const int saved = d.shot && d.shot->hit > runs ? d.shot->hit : 0;
  {
    Ball ball;
    ball.label = extra ? cat(runs, *extra) : total == 0 ? std::string("•") : std::to_string(total);
    ball.kind = !extra && runs == 4 ? BallKind::b4 : !extra && runs == 6 ? BallKind::b6 : BallKind::run;
    ball.bat = striker.id;
    ball.runs = extra ? 0 : total;
    ball.legBye = extra && *extra == "lb";
    ball.bye = extra && *extra == "b";
    ball.drop = d.chanceMissed.has_value();
    ball.special = d.special;
    ball.review = review;
    if (d.shot) ball.pos = d.shot->pos;
    ball.saved = saved;
    ball.overthrow = ot;
    const Placement pl = placed ? *placed : shotFor(key, extra ? 0 : runs, std::nullopt, striker.hand);
    ball.dir = pl.dir;
    ball.dist = pl.dist;
    thisOver.balls.push_back(std::move(ball));
  }
  const std::string who = surname(striker.name);
  if (d.chanceMissed) {
    bowlerEvent(fig, BowlerEvent::drop);
    const std::string f = surname(fielder->name);
    const std::string at =
        d.shot && d.shot->pos ? cat(" at ", lower_ascii(std::string(position(*d.shot->pos).label))) : std::string();
    addComment(match, tag, CommentKind::drop,
               *d.chanceMissed == "catch"
                   ? cat(surname(bowler.name), " to ", who, ", DROPPED! ", f, " puts down a chance", at,
                         runs ? " and they scramble a single" : "", ". ", who, " survives on ", bat.runs, ".")
                   : cat(surname(bowler.name), " to ", who, ", beaten, but ", f, " fumbles the stumping! ", who,
                         " gets a life on ", bat.runs, "."));
  } else {
    CommentKind kind = extra             ? CommentKind::extra
                       : total == 0      ? CommentKind::dot
                       : runs == 4       ? CommentKind::four
                       : runs == 6       ? CommentKind::six
                                         : CommentKind::run;
    std::string text;
    if (umpire && umpire->lead) {
      text = *umpire->lead;
      if (d.umpire->reviewed) kind = CommentKind::review;
    } else if (extra && *extra == "b") {
      text = byeText(bowler, striker, runs);
    } else if (extra && *extra == "lb") {
      BallTextArgs a;
      a.bowler = &bowler;
      a.striker = &striker;
      a.runs = runs;
      a.spin = spin;
      a.legBye = true;
      text = ballText(r, a);
    } else if (d.special) {
      text = specialText(cat(surname(bowler.name), " to ", who), *d.special, who, runs);
    } else if (d.shot && d.shot->hit) {
      std::optional<Saver> saver;
      if (saved && d.shot->pos && inn.fieldNow) {
        const PlayerId& byId = inn.fieldNow->assign[static_cast<size_t>(*d.shot->pos)];
        if (!byId.empty()) {
          const PositionDef& pd = position(*d.shot->pos);
          saver = Saver{P(match, byId).name, pd.label, static_cast<int>(pd.zone)};
        }
      }
      text = shotText(r, bowler, striker, regionOf(d.shot->dir), d.shot->hit, runs, saver);
    } else {
      BallTextArgs a;
      a.bowler = &bowler;
      a.striker = &striker;
      a.runs = runs;
      a.spin = spin;
      a.kph = speedKph(bowler);
      a.over = d.overIdx;
      text = ballText(r, a);
    }
    if (ot) text += overthrowText(ot);
    addComment(match, tag, kind, std::move(text));
  }
  // Batters cross for each run they complete; an overthrow to the rope adds runs without running
  const int ran = ot == 4 ? runs : total;
  if (ran % 2 == 1) swapStrike(inn);
  afterBall(match, true, false, extra ? 0 : total);
  return std::to_string(total);
}

// Commentary for swings in momentum: a batter hitting top gear, a collapse under way.
void announceMomentum(Match& match) {
  Innings& inn = inningsOf(match);
  for (const std::optional<PlayerId>& id : {inn.striker, inn.nonStriker}) {
    if (!id) continue;
    BatterInnings& b = inn.batters.at(*id);
    if (!b.zone && b.conf >= 80 && b.balls >= 10) {
      b.zone = true;
      addComment(match, oversText(inn.legalBalls), CommentKind::milestone,
                 cat(P(match, *id).name, " is in the zone now: ", b.runs, " off ", b.balls,
                     " and seeing it like a football."));
    }
  }
  if (inn.fow.empty()) return;
  FallOfWicket& last = inn.fow.back();
  if (last.ball == inn.legalBalls && !last.announced) {
    last.announced = true;
    const int n = wicketsInLast(inn, 18);
    if (n >= 2 && inn.wickets < inn.maxWickets) {
      const FallOfWicket& first = inn.fow[inn.fow.size() - static_cast<size_t>(n)];
      const MatchTeam& team = match.teams[static_cast<size_t>(inn.batting)];
      addComment(match, oversText(inn.legalBalls), CommentKind::info,
                 n >= 3 ? cat("Collapse! ", team.name, " have lost ", n, " wickets in ", inn.legalBalls - (first.ball - 1),
                              " balls. The batters will be looking to dig in.")
                        : cat(team.name, " are wobbling: two wickets in quick succession. Time to rebuild."));
    }
  }
}

// Strategic timeouts: the fielding side's after the 8th over, the batting side's after the 14th.
void strategicTimeout(Match& match) {
  Innings& inn = inningsOf(match);
  const int done = inn.legalBalls / 6;
  if (done == 8) {
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               cat("Strategic timeout (", match.teams[static_cast<size_t>(inn.bowlingSide)].name,
                   "). The fielding side regroup."));
    for (auto& [id, f] : inn.bowling) f.rhythm = std::min(95.0, f.rhythm + 3);
  } else if (done == 14) {
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               cat("Strategic timeout (", match.teams[static_cast<size_t>(inn.batting)].name,
                   "). The batters plan the finish."));
    for (const std::optional<PlayerId>& id : {inn.striker, inn.nonStriker}) {
      if (!id) continue;
      BatterInnings& b = inn.batters.at(*id);
      b.conf = std::min(100.0, b.conf + 2);
    }
  }
}

// ---------------------------------------------------------------------------
// Rain and DLS
// ---------------------------------------------------------------------------

// At the end of an over: does the forecast rain arrive now? Returns true if the innings (or
// match) has ended because of it.
bool rainArrives(Match& match) {
  if (!match.weather.rain || match.weather.rainDone) return false;
  const Rain rain = *match.weather.rain;
  Innings& inn = inningsOf(match);
  const int over = inn.legalBalls / 6;
  const int which = rain.when == RainWhen::first ? 0 : rain.when == RainWhen::second ? 1 : -1;
  if (match.current != which || over < rain.atOver) return false;
  match.weather.rainDone = true;
  const std::string tag = oversText(inn.legalBalls);
  const int oldOvers = inn.maxBalls / 6;
  if (which == 0) {
    // First innings cut short: both sides lose overs, the chase gets a DLS target
    const int newOvers = std::max(over, std::max(5, oldOvers - rain.lost));
    const double r1 = firstInningsResources(oldOvers, newOvers, over, inn.wickets);
    match.oversPerSide = newOvers;
    inn.maxBalls = newOvers * 6;
    Dls dls;
    dls.r1 = r1;
    dls.firstInnings = newOvers;
    match.dls = dls;
    addComment(match, tag, CommentKind::info,
               cat("Rain! The covers come on. ", rain.lost, " overs are lost: it's now ", newOvers, " overs a side.",
                   newOvers == over ? " That is the end of the innings." : ""));
    if (inn.legalBalls >= inn.maxBalls) {
      endInnings(match);
      return true;
    }
    return false;
  }
  // The chase is interrupted
  const Innings& first = match.innings[0];
  const double r1 = match.dls && match.dls->r1 ? *match.dls->r1 : firstInningsResources(first.maxBalls / 6);
  const double r2Before = resources(oldOvers, 0);
  if (rain.abandon) {
    Dls dls = match.dls ? *match.dls : Dls{};
    dls.r1 = r1;
    dls.abandoned = true;
    if (over < 5) {
      dls.noResult = true;
      match.dls = dls;
      addComment(match, tag, CommentKind::info,
                 "Rain. Heavy rain, and it is not going to stop. Fewer than 5 overs of the chase were possible: no result.");
    } else {
      const double used = r2Before - resources(oldOvers - over, inn.wickets);
      dls.par = dlsPar(first.runs, r1, used);
      match.dls = dls;
      addComment(match, tag, CommentKind::info,
                 cat("Rain, and it's set in. The match is called off with ", inn.runs, "/", inn.wickets,
                     "; the DLS par score was ", *dls.par, "."));
    }
    endInnings(match);
    return true;
  }
  const int newOvers = std::max(std::max(5, over + 1), oldOvers - rain.lost);
  const double lostRes = resources(oldOvers - over, inn.wickets) - resources(newOvers - over, inn.wickets);
  const double r2 = r2Before - lostRes;
  inn.maxBalls = newOvers * 6;
  inn.target = dlsTarget(first.runs, r1, r2);
  Dls dls = match.dls ? *match.dls : Dls{};
  dls.r1 = r1;
  dls.r2 = r2;
  dls.target = inn.target;
  match.dls = dls;
  addComment(match, tag, CommentKind::info,
             cat("Rain stops play. When they come back, ", oldOvers - newOvers,
                 " overs have been lost: the revised DLS target is ", *inn.target, " from ", newOvers, " overs (",
                 *inn.target - inn.runs, " more needed)."));
  if (inn.runs >= *inn.target || inn.legalBalls >= inn.maxBalls) {
    endInnings(match);
    return true;
  }
  return false;
}

// The chase target: first innings + 1, or DLS-adjusted if the first innings was cut short.
void setChaseTarget(Match& match) {
  if (!match.dls || !match.dls->r1) return;
  const Innings& first = match.innings[0];
  const double r2 = resources(match.oversPerSide, 0);
  match.dls->target = dlsTarget(first.runs, *match.dls->r1, r2);
  match.dls->r2 = r2;
}

std::string plural(int n, std::string_view word) { return cat(n, " ", word, n == 1 ? "" : "s"); }

Result computeResult(const Match& match) {
  std::vector<const Innings*> so;
  for (const Innings& i : match.innings)
    if (i.superOver) so.push_back(&i);
  Result res;
  if (!so.empty()) {
    const Innings& last = *so.back();
    const int winner = last.runs >= *last.target ? last.batting : last.bowlingSide;
    const int n = static_cast<int>(std::ceil(so.size() / 2.0));
    static const std::string_view nth[] = {"", "", "second", "third", "fourth", "fifth"};
    const std::string which = n == 1 ? std::string("the Super Over")
                              : n < 6 ? cat("the ", nth[n], " Super Over")
                                      : cat("the ", n, "th Super Over");
    res.winner = winner;
    res.superOver = true;
    res.text = cat("Match tied. ", match.teams[static_cast<size_t>(winner)].name, " won ", which);
    return res;
  }
  const Innings& first = match.innings[0];
  const Innings& second = match.innings[1];
  const MatchTeam& t1 = match.teams[static_cast<size_t>(first.batting)];
  const MatchTeam& t2 = match.teams[static_cast<size_t>(second.batting)];
  if (match.dls && match.dls->abandoned) {
    if (match.dls->noResult) {
      res.noResult = true;
      res.text = "No result: rain has the final say";
      return res;
    }
    const int par = *match.dls->par;
    res.dls = true;
    if (second.runs > par) {
      const int wk = second.maxWickets - second.wickets;
      res.winner = second.batting;
      res.text = cat(t2.name, " won by ", plural(wk, "wicket"), " (DLS method, par ", par, ")");
      return res;
    }
    if (second.runs == par) {
      res.tied = true;
      res.text = cat("Match tied on DLS (par ", par, ")");
      return res;
    }
    const int m = par - second.runs;
    res.winner = first.batting;
    res.text = cat(t1.name, " won by ", plural(m, "run"), " (DLS method)");
    return res;
  }
  if (second.runs >= *second.target) {
    const int wk = second.maxWickets - second.wickets;  // wickets in hand: fewer than 10 for a side short of 11
    const int left = second.maxBalls - second.legalBalls;
    res.winner = second.batting;
    res.text = cat(t2.name, " won by ", plural(wk, "wicket"),
                   left ? cat(" with ", plural(left, "ball"), " to spare") : std::string(" off the last ball"));
    return res;
  }
  if (second.runs == *second.target - 1) {
    res.text = "Match tied";
    return res;
  }
  const int margin = *second.target - 1 - second.runs;
  const bool dlsTarget = match.dls && match.dls->target;
  res.winner = first.batting;
  res.dls = dlsTarget;
  res.text = cat(t1.name, " won by ", plural(margin, "run"), dlsTarget ? " (DLS method)" : "");
  return res;
}

void endInnings(Match& match) {
  Innings& inn = inningsOf(match);
  inn.complete = true;
  match.awaiting = Awaiting::none;
  const MatchTeam& team = match.teams[static_cast<size_t>(inn.batting)];
  if (inn.superOver) {
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               cat(team.name, " make ", inn.runs, "/", inn.wickets, " in their Super Over."));
    if (!inn.target) {
      startSuperOverInnings(match, inn.bowlingSide, inn.runs + 1, inn.superOver);
      return;
    }
    const auto soCount = std::count_if(match.innings.begin(), match.innings.end(), [](const Innings& i) { return i.superOver; });
    if (inn.runs == *inn.target - 1 && soCount < 40) {
      addComment(match, oversText(inn.legalBalls), CommentKind::result, "The Super Over is tied too! Another Super Over.");
      startSuperOver(match);
      return;
    }
  } else {
    const bool allOut = inn.wickets >= inn.maxWickets;
    addComment(match, oversText(inn.legalBalls), CommentKind::info,
               cat(team.name, allOut ? " all out for " : " finish on ", inn.runs,
                   inn.wickets < inn.maxWickets ? cat("/", inn.wickets) : std::string(), " in ", oversText(inn.legalBalls),
                   " overs."));
    if (match.current == 0) {
      match.status = MatchStatus::break_;
      setChaseTarget(match);
      // AI sides use their Impact Player at the innings break
      for (int side : {0, 1})
        if (!isUser(match, side)) aiUseImpact(match, side);
      return;
    }
    if (!(match.dls && match.dls->abandoned) && inn.runs == *inn.target - 1) {
      addComment(match, oversText(inn.legalBalls), CommentKind::result, "Scores level! We are going to a Super Over.");
      startSuperOver(match);
      return;
    }
  }
  match.status = MatchStatus::complete;
  match.result = computeResult(match);
  match.potm = playerOfTheMatch(match);
  addComment(match, oversText(inningsOf(match).legalBalls), CommentKind::result, match.result->text);
}

void afterBall(Match& match, bool legal, bool wicket, int batRuns) {
  Innings& inn = inningsOf(match);
  const bool chaseDone = inn.target && inn.runs >= *inn.target;
  const bool overDone = legal && inn.legalBalls % 6 == 0;

  for (std::string& note : updateFans(match, batRuns, wicket, overDone))
    addComment(match, oversText(inn.legalBalls), CommentKind::info, std::move(note));
  announceMomentum(match);
  if (overDone) {
    const Over& o = inn.overs[static_cast<size_t>(overIndex(inn) - 1)];
    BowlingFigures& fig = inn.bowling.at(o.bowler);
    if (o.bowlerRuns == 0) {
      fig.maidens += 1;
      bowlerEvent(fig, BowlerEvent::maiden);
    }
    if (o.bowlerRuns >= 14) bowlerEvent(fig, BowlerEvent::expensiveOver);
    const Player& b = P(match, o.bowler);
    addComment(match, oversText(inn.legalBalls), CommentKind::over,
               cat("End of over ", inn.legalBalls / 6, ": ", plural(o.runs, "run"),
                   o.wickets ? cat(", ", plural(o.wickets, "wicket")) : std::string(), ". Score ", inn.runs, "/", inn.wickets,
                   ". ", surname(b.name), " ", oversText(fig.balls), "-", fig.maidens, "-", fig.runs, "-", fig.wickets, "."));
  }

  if (inn.wickets >= inn.maxWickets || inn.legalBalls >= inn.maxBalls || chaseDone) {
    endInnings(match);
    return;
  }
  // Rain
  if (overDone && !inn.superOver && rainArrives(match)) return;
  if (overDone && !inn.superOver) strategicTimeout(match);
  if (overDone) {
    swapStrike(inn);
    inn.lastOverBowler = inn.currentBowler;
    inn.currentBowler.reset();
  }
  prepareNext(match, false);
}

// ---------------------------------------------------------------------------
// Super Over (IPL rules): the side that batted second bats first; 6 balls, 2 wickets; repeated
// until there is a winner. In a repeat, batters dismissed and the bowler used in the previous
// Super Over can't be picked, and the side that batted second in it bats first.
// ---------------------------------------------------------------------------

struct SoPicks {
  std::vector<PlayerId> batters;
  PlayerId bowler;
};

SoPicks superOverPicks(const Match& match, int side, const std::optional<SoHistory>& prev) {
  const MatchTeam& team = match.teams[static_cast<size_t>(side)];
  std::vector<PlayerId> banBat;
  if (prev && prev->bat[static_cast<size_t>(side)])
    for (const auto& [id, out] : *prev->bat[static_cast<size_t>(side)])
      if (out) banBat.push_back(id);
  const std::optional<PlayerId> banBowl = prev ? prev->bowl[static_cast<size_t>(side)] : std::nullopt;
  auto hitter = [&](const PlayerId& id) {
    const Player& p = P(match, id);
    return p.skills.bat[BatK::lofted] * 0.6 + p.bat * 0.4;
  };
  std::vector<PlayerId> batters;
  for (const PlayerId& id : team.players)
    if (std::find(banBat.begin(), banBat.end(), id) == banBat.end()) batters.push_back(id);
  std::stable_sort(batters.begin(), batters.end(),
                   [&](const PlayerId& a, const PlayerId& b) { return hitter(b) - hitter(a) < 0; });
  if (batters.size() > 3) batters.resize(3);
  auto deathSkill = [&](const PlayerId& id) {
    const Player& p = P(match, id);
    const auto& b = p.skills.bowl;
    return (b.contains(BowlK::death) ? b[BowlK::death] * 0.6 + b[BowlK::accuracy] * 0.4
                                     : b[BowlK::variations] * 0.5 + b[BowlK::accuracy] * 0.5) *
           (p.bowl / 100);
  };
  std::vector<PlayerId> bowlers;
  for (const PlayerId& id : team.players)
    if (id != team.keeperId && !(banBowl && id == *banBowl)) bowlers.push_back(id);
  std::stable_sort(bowlers.begin(), bowlers.end(),
                   [&](const PlayerId& a, const PlayerId& b) { return deathSkill(b) - deathSkill(a) < 0; });
  return {batters, bowlers.empty() ? PlayerId{} : bowlers[0]};
}

std::optional<SoHistory> soHistory(const Match& match) {
  // Who batted (and was out) and who bowled in the most recent completed Super Over pair
  std::vector<const Innings*> so;
  for (const Innings& i : match.innings)
    if (i.superOver) so.push_back(&i);
  if (so.size() < 2) return std::nullopt;
  SoHistory h;
  for (size_t k = so.size() - 2; k < so.size(); ++k) {
    const Innings& inn = *so[k];
    std::vector<std::pair<PlayerId, bool>> outs;
    for (const PlayerId& id : inn.battedOrder) outs.emplace_back(id, inn.batters.at(id).out);
    h.bat[static_cast<size_t>(inn.batting)] = outs;
    h.bowl[static_cast<size_t>(inn.bowlingSide)] =
        inn.bowlingOrder.empty() ? std::optional<PlayerId>() : std::optional<PlayerId>(inn.bowlingOrder[0]);
  }
  h.secondBatting = so.back()->batting;
  return h;
}

void startSuperOver(Match& match) {
  const std::optional<SoHistory> prev = soHistory(match);
  const auto soCount = std::count_if(match.innings.begin(), match.innings.end(), [](const Innings& i) { return i.superOver; });
  const int n = static_cast<int>(soCount / 2 + 1);
  const int firstBat = prev ? prev->secondBatting : match.innings[1].batting;
  match.soPrev = prev;
  startSuperOverInnings(match, firstBat, std::nullopt, n);
}

void startSuperOverInnings(Match& match, int side, std::optional<int> target, int n) {
  const std::optional<SoHistory> prev = match.soPrev;
  const SoPicks bat = superOverPicks(match, side, prev);
  const PlayerId bowl = superOverPicks(match, 1 - side, prev).bowler;
  Innings inn;
  inn.batting = side;
  inn.bowlingSide = 1 - side;
  inn.superOver = n;
  inn.maxBalls = 6;
  inn.maxWickets = std::min(2, static_cast<int>(bat.batters.size()) - 1);
  inn.target = target;
  inn.reviewsLeft = {0, 0};
  for (const PlayerId& id : bat.batters)
    inn.batters.set(id, freshBatter(P(match, id).startConf.value_or(START_CONFIDENCE) + 8));
  inn.battedOrder.assign(bat.batters.begin(), bat.batters.begin() + std::min<size_t>(2, bat.batters.size()));
  if (bat.batters.size() > 2) inn.yetToBat.assign(bat.batters.begin() + 2, bat.batters.end());
  if (!bat.batters.empty()) inn.striker = bat.batters[0];
  if (bat.batters.size() > 1) inn.nonStriker = bat.batters[1];
  match.innings.push_back(std::move(inn));
  match.current = static_cast<int>(match.innings.size()) - 1;
  match.status = MatchStatus::live;
  std::string names;
  for (size_t i = 0; i < bat.batters.size(); ++i) names += (i ? ", " : "") + P(match, bat.batters[i]).name;
  addComment(match, "0.0", CommentKind::info,
             cat(n > 1 ? cat("Super Over ", n) : std::string("SUPER OVER"), ": ",
                 match.teams[static_cast<size_t>(side)].name, " bat",
                 target ? cat(", needing ", *target) : std::string(" first"), ". ", names, " to bat; ",
                 P(match, bowl).name, " to bowl."));
  assignBowler(match, bowl);
  prepareNext(match, true);
}

}  // namespace

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

Match createMatch(const MatchSetup& setup) {
  Match match;
  match.id = setup.id ? *setup.id : std::to_string(setup.seed);
  match.seed = setup.seed;
  match.rngState = setup.seed;
  match.pitch = setup.pitch;
  match.userTeam = setup.userTeam;
  for (const SideInput& t : setup.teams) {
    for (const Player& p : t.players) match.players.set(p.id, p);
    for (const Player& p : t.bench) match.players.set(p.id, p);
  }
  for (size_t i = 0; i < 2; ++i) {
    const SideInput& t = setup.teams[i];
    MatchTeam& mt = match.teams[i];
    mt.id = t.id;
    mt.name = t.name;
    mt.color = t.color;
    mt.keeperId = t.keeperId;
    mt.captain = t.captain;
    mt.captainId = t.captainId;
    for (const Player& p : t.players) mt.players.push_back(p.id);
    for (const Player& p : t.bench) mt.bench.push_back(p.id);
  }
  match.settings = setup.settings;
  match.innings.reserve(4);
  // Evening game? Dew makes the ball wet in the second innings: harder to grip, easier to chase.
  StateStream stream{&match.rngState};
  match.dew = stream() < 0.4;
  match.weather = setup.weather ? *setup.weather : forecast(setup.seed, setup.pitch);
  if (match.weather.sky == Sky::humid && !match.dew) match.dew = stream() < 0.35;
  match.oversPerSide = 20;
  match.dls.reset();
  match.fans = createFans(match, setup.ground);
  // Batter v bowler history: who has the edge in each pairing today
  std::array<std::vector<PlayerId>, 2> ids;
  for (size_t i = 0; i < 2; ++i) {
    for (const Player& p : setup.teams[i].players) ids[i].push_back(p.id);
    for (const Player& p : setup.teams[i].bench) ids[i].push_back(p.id);
  }
  match.h2h = matchupEdges(setup.matchups, ids);
  return match;
}

const Toss& callToss(Match& match, const std::string& call) {
  StateStream stream{&match.rngState};
  const std::string coin = stream() < 0.5 ? "heads" : "tails";
  const int user = match.userTeam.value_or(0);
  const int winner = call == coin ? user : 1 - user;
  match.toss = Toss{call, coin, winner, std::nullopt};
  if (!isUser(match, winner)) {
    match.toss->decision = aiTossDecision(match);
    startMatch(match);
  }
  return *match.toss;
}

void chooseTossDecision(Match& match, TossDecision decision) {
  match.toss->decision = decision;
  startMatch(match);
}

void autoToss(Match& match) {
  StateStream stream{&match.rngState};
  const int winner = stream() < 0.5 ? 0 : 1;
  match.toss = Toss{std::nullopt, std::nullopt, winner, std::nullopt};
  match.toss->decision = aiTossDecision(match);
  startMatch(match);
}

void startSecondInnings(Match& match) {
  if (match.status != MatchStatus::break_) return;
  startInnings(match, 1);
}

// ---------------------------------------------------------------------------
// Decisions
// ---------------------------------------------------------------------------

bool userIsBatting(const Match& match) {
  return match.status == MatchStatus::live && isUser(match, inningsOf(match).batting);
}
bool userIsBowling(const Match& match) {
  return match.status == MatchStatus::live && isUser(match, inningsOf(match).bowlingSide);
}

bool setNextBatter(Match& match, const PlayerId& id) {
  const Innings& inn = inningsOf(match);
  if (match.awaiting != Awaiting::batter || std::find(inn.yetToBat.begin(), inn.yetToBat.end(), id) == inn.yetToBat.end())
    return false;
  placeBatter(match, id);
  match.awaiting = Awaiting::none;
  prepareNext(match, false);
  return true;
}

bool setBowler(Match& match, const PlayerId& id) {
  if (match.awaiting != Awaiting::bowler) return false;
  const auto opts = bowlingOptions(match);
  const auto it = std::find_if(opts.begin(), opts.end(), [&](const BowlOption& o) { return o.id == id; });
  if (it == opts.end() || !it->ok) return false;
  assignBowler(match, id);
  match.awaiting = Awaiting::none;
  refreshMindset(match);
  return true;
}

bool setBowlingPlan(Match& match, std::optional<Plan> plan) {
  Innings& inn = inningsOf(match);
  if (!isUser(match, inn.bowlingSide) || !inn.currentBowler) return false;
  inn.plans[*inn.currentBowler] = plan;
  return true;
}

bool setField(Match& match, FieldPreset preset, std::optional<std::vector<Pos>> positions) {
  Innings& inn = inningsOf(match);
  if (!isUser(match, inn.bowlingSide)) return false;
  UserField uf;
  uf.preset = preset;
  if (positions)
    uf.positions = std::move(positions);
  else if (preset != FieldPreset::auto_)
    uf.positions = presetByKey(preset).positions;
  inn.userField = uf;
  return true;
}

bool setBatterTarget(Match& match, const PlayerId& bowlerId, std::optional<TargetMode> mode) {
  match.settings.targets[bowlerId] = mode;
  refreshMindset(match);
  return true;
}

bool setBatterTempo(Match& match, const PlayerId& batterId, Tempo level) {
  match.settings.batterTempo[batterId] = level;
  refreshMindset(match);
  return true;
}

Tempo batterTempoOf(const Match& match, const PlayerId& batterId) {
  auto it = match.settings.batterTempo.find(batterId);
  return it != match.settings.batterTempo.end() ? it->second : match.settings.aggression;
}

bool canChangeBowler(const Match& match) {
  if (match.status != MatchStatus::live || match.awaiting != Awaiting::none) return false;
  const Innings& inn = inningsOf(match);
  if (inn.superOver || !isUser(match, inn.bowlingSide) || !inn.currentBowler) return false;
  const size_t ov = static_cast<size_t>(overIndex(inn));
  if (ov >= inn.overs.size()) return false;
  const Over& over = inn.overs[ov];
  return over.bowler == *inn.currentBowler && over.balls.empty();
}

bool changeBowler(Match& match) {
  if (!canChangeBowler(match)) return false;
  Innings& inn = inningsOf(match);
  const PlayerId id = *inn.currentBowler;
  inn.overs.erase(inn.overs.begin() + overIndex(inn));
  const BowlingFigures* f = inn.bowling.find(id);
  const bool bowledBefore = std::any_of(inn.overs.begin(), inn.overs.end(), [&](const Over& o) { return o.bowler == id; });
  if (f && f->balls == 0 && f->noballs == 0 && f->wides == 0 && !bowledBefore) {
    inn.bowling.erase(id);
    inn.bowlingOrder.erase(std::remove(inn.bowlingOrder.begin(), inn.bowlingOrder.end(), id), inn.bowlingOrder.end());
  }
  inn.currentBowler.reset();
  match.awaiting = Awaiting::bowler;
  return true;
}

void resolvePending(Match& match) {
  if (match.awaiting == Awaiting::review)
    reviewDecision(match, aiReviews(match, *match.pendingBall));
  else if (match.awaiting != Awaiting::none)
    prepareNext(match, true);
}

void refreshMindset(Match& match) {
  if (match.status != MatchStatus::live || match.awaiting != Awaiting::none) return;
  Innings& inn = inningsOf(match);
  if (inn.striker && inn.currentBowler) currentTempo(match, inn);
}

// ---------------------------------------------------------------------------
// Ball by ball
// ---------------------------------------------------------------------------

std::string playBall(Match& match) {
  if (match.status != MatchStatus::live || match.awaiting != Awaiting::none) return "";
  Innings& inn = inningsOf(match);
  StateStream stream{&match.rngState};
  RandFn r(stream);
  const Player& bowler = P(match, *inn.currentBowler);
  const Player& striker = P(match, *inn.striker);
  const int over = overIndex(inn);
  // A Super Over is bowled with an old ball in a death-overs situation
  const int ctxOver = inn.superOver ? 19 : over;
  const Phase phase = inn.superOver ? phaseOf(ctxOver) : phaseAt(&inn, ctxOver);
  const bool spin = isSpinner(bowler);
  BowlingFigures& fig = inn.bowling.at(bowler.id);
  BatterInnings& bat = inn.batters.at(striker.id);
  Over& thisOver = inn.overs[static_cast<size_t>(over)];
  const std::string tag = ballTag(inn.legalBalls);
  const MatchTeam& fielding = match.teams[static_cast<size_t>(inn.bowlingSide)];
  const Player& keeper = P(match, fielding.keeperId);

  const Conditions cond = pitchConditions(match, ctxOver, spin);
  // The bowling plan for this ball: the user's call for this bowler, or the AI captain's
  const bool userField = isUser(match, inn.bowlingSide) && !inn.superOver;
  std::optional<Plan> chosen;
  if (userField) {
    auto it = inn.plans.find(bowler.id);
    if (it != inn.plans.end()) chosen = it->second;
  }
  std::optional<PlanStreak> streak;
  if (auto it = inn.planStreak.find(striker.id); it != inn.planStreak.end()) streak = it->second;
  std::optional<ChaseState> chase;
  if (inn.target) chase = ChaseState{static_cast<double>(*inn.target - inn.runs), static_cast<double>(inn.maxBalls - inn.legalBalls)};
  Plan plan;
  if (chosen) {
    plan = *chosen;
  } else {
    AiPlanContext ctx;
    ctx.r = &r;
    ctx.streak = streak ? &*streak : nullptr;
    ctx.chase = chase;
    plan = aiPlan(fielding.captain, bowler, striker, phase, ctx);
  }
  // A batter who has seen the same plan ball after ball is waiting for it
  const PlanEffect eff =
      readByBatter(planEffect(plan, bowler, &striker), plan, streak && streak->plan == plan ? streak->n + 1 : 1);
  inn.lastPlan = LastPlan{bowler.id, plan, eff.fits, eff.read};
  // The field: the user's setting, or the AI captain's for this plan and phase; then who stands where
  FieldSet fieldSet;
  if (userField && inn.userField && inn.userField->preset != FieldPreset::auto_) {
    const UserField& uf = *inn.userField;
    fieldSet = {uf.preset, legalize(uf.positions ? *uf.positions : presetByKey(uf.preset).positions, phase)};
  } else {
    fieldSet = aiField(fielding.captain, phase, plan, bowler);
  }
  std::vector<PlayerId> outfield;
  for (const PlayerId& id : fielding.players)
    if (id != bowler.id && id != fielding.keeperId) outfield.push_back(id);
  // Same bowler, same field (and same side, after an Impact sub): the same men in the same places
  const bool sameField = inn.fieldNow && inn.fieldNow->keyBowler == bowler.id &&
                         inn.fieldNow->positions == fieldSet.positions && inn.fieldNow->keyOutfield == outfield;
  FieldNow fn;
  fn.preset = fieldSet.preset;
  fn.positions = fieldSet.positions;
  fn.keyBowler = bowler.id;
  fn.assign = sameField ? inn.fieldNow->assign : assignFielders(fieldSet.positions, outfield, match.players);
  fn.keyOutfield = std::move(outfield);
  inn.fieldNow = std::move(fn);
  const FieldNow& fieldNow = *inn.fieldNow;
  const std::vector<PlayerId>& outfielders = fieldNow.keyOutfield;
  auto fielderAtOpt = [&](std::optional<Pos> k) -> const Player* {
    if (!k) return nullptr;
    const PlayerId& id = fieldNow.assign[static_cast<size_t>(*k)];
    return id.empty() ? nullptr : &P(match, id);
  };
  const FielderAt fielderAt = [&](Pos k) { return fielderAtOpt(k); };
  auto anyFielder = [&]() -> const Player* {
    const double u = r();
    return &P(match, outfielders[static_cast<size_t>(std::floor(u * static_cast<double>(outfielders.size())))]);
  };
  const FieldEffect fe = fieldEffect(fieldSet.positions, bowler, &striker, plan);
  const double rh = (fig.rhythm - 50) / 50;
  const auto& bs = bowler.skills.bowl;
  const double swingWides = !spin && over < 4 ? js_exp((bs[BowlK::swing] - 60) / 150) : 1;
  const double wideP = 0.03 * js_exp((65 - bs[BowlK::accuracy]) / 45) * swingWides * (phase == Phase::death ? 1.3 : 1) *
                       (spin ? 0.55 : 1) * cond.wide * js_exp(-0.4 * rh) * eff.wide;
  const double nbP = spin ? 0.002 : 0.006 * js_exp((60 - bs[BowlK::accuracy]) / 40);
  const double x = r();

  // --- Wide -----------------------------------------------------------------
  if (x < wideP) {
    const int runs = r() < 0.04 ? 5 : 1;
    inn.runs += runs;
    inn.extras.wd += runs;
    fig.runs += runs;
    fig.wides += 1;
    thisOver.runs += runs;
    thisOver.bowlerRuns += runs;
    inn.partnership.runs += runs;
    bowlerEvent(fig, BowlerEvent::wide);
    // Down the track to the spinner, beaten wide of the stumps: the keeper does the rest
    if (spin && runs == 1 && !inn.freeHit && r() < 0.03 * (keeper.skills.field[FieldK::keeping] / 70)) {
      Ball ball;
      ball.label = "W";
      ball.kind = BallKind::wicket;
      ball.wide = true;
      ball.bat = striker.id;
      ball.faced = striker.id;
      ball.runs = 0;
      ball.wicketType = Out::stumped;
      ball.dir = 0;
      ball.dist = 0;
      thisOver.balls.push_back(std::move(ball));
      const DismissResult dr = dismiss(inn, striker.id, Out::stumped, bowler, &keeper, keeper, thisOver);
      addComment(match, tag, CommentKind::wicket,
                 cat(surname(bowler.name), " to ", surname(striker.name),
                     ", down the track, beaten by the turn and it's called wide... but ", surname(keeper.name),
                     " has the bails off! STUMPED. ", striker.name, " ", dr.howOut, " ", bat.runs, " (", bat.balls, "b)."));
      afterBall(match, false, true, 0);
      return "W";
    }
    Ball ball;
    ball.label = runs > 1 ? cat(runs, "wd") : std::string("wd");
    ball.kind = BallKind::extra;
    thisOver.balls.push_back(std::move(ball));
    BallTextArgs a;
    a.bowler = &bowler;
    a.striker = &striker;
    a.runs = runs;
    a.spin = spin;
    a.kind = BallTextArgs::Kind::wide;
    addComment(match, tag, CommentKind::extra, ballText(r, a));
    afterBall(match, false, false, 0);
    return "wd";
  }

  const double tempo = currentTempo(match, inn);
  const bool chasePressure =
      inn.target && (static_cast<double>(*inn.target - inn.runs) * 6) / std::max(1, inn.maxBalls - inn.legalBalls) > 10;
  const bool pressure = wicketsInLast(inn, 18) >= 2 || chasePressure || (bat.runs >= 85 && bat.runs < 100);
  const BatterNow B = batterNow(striker, spin, pressure);
  const BallContext bctx{ctxOver, phase, match.pitch, cond.wear, cond.dew};
  const BowlerNow T = bowlerNow(bowler, bctx);
  const FieldingNow field = fieldingNow(match, inn);
  const Edge edge = specialistEdge(bowler, bctx);
  OutcomeInputs oi;
  oi.bowler = &bowler;
  oi.B = B;
  oi.T = T;
  oi.edge = edge;
  oi.field = field;
  oi.ballsFaced = bat.balls;
  oi.runsScored = bat.runs;
  oi.phase = phase;
  oi.tempo = tempo;
  oi.cond = cond;
  oi.conf = bat.conf;
  oi.rhythm = fig.rhythm;
  oi.freeHit = inn.freeHit;
  OutcomeWeights w = outcomeWeights(oi);
  w[O::W] *= eff.W;
  w[O::o0] *= eff.o0;
  w[O::o1] *= eff.o1;
  w[O::o4] *= eff.o4;
  w[O::o6] *= eff.o6;
  w[O::W] *= fe.W;
  w[O::o0] *= fe.o0;
  w[O::o1] *= fe.o1;
  w[O::o2] *= fe.o2;
  w[O::o4] *= fe.o4;
  w[O::o6] *= fe.o6;
  // History between these two: a bowler who keeps getting him out is in his head, and the other way round
  const double* h2hFound = match.h2h.find(matchupKey(striker.id, bowler.id));
  const double h2hEdge = h2hFound ? *h2hFound : 0;
  if (h2hEdge) {
    const EdgeEffect he = edgeEffect(h2hEdge);
    w[O::W] *= he.W;
    w[O::o4] *= he.boundary;
    w[O::o6] *= he.boundary;
  }

  // --- No ball: one extra plus whatever comes off the bat, then a free hit ----
  if (x < wideP + nbP) {
    w.hasW = false;
    const int runs = kOutcomeRuns[static_cast<size_t>(weightedOutcome(r, w))];
    inn.runs += 1 + runs;
    inn.extras.nb += 1;
    fig.runs += 1 + runs;
    fig.noballs += 1;
    bat.runs += runs;
    bat.balls += 1;
    if (runs == 4) {
      bat.fours += 1;
      fig.fours += 1;
    }
    if (runs == 6) {
      bat.sixes += 1;
      fig.sixes += 1;
    }
    thisOver.runs += 1 + runs;
    thisOver.bowlerRuns += 1 + runs;
    inn.partnership.runs += 1 + runs;
    inn.partnership.balls += 1;
    {
      Ball ball;
      ball.label = runs ? cat(runs, "nb") : std::string("nb");
      ball.kind = BallKind::extra;
      ball.bat = striker.id;
      ball.runs = runs;
      const Placement pl =
          shotFor(ballKey(match.seed, match.current, inn.legalBalls, static_cast<int>(thisOver.balls.size())), runs,
                  std::nullopt, striker.hand);
      ball.dir = pl.dir;
      ball.dist = pl.dist;
      thisOver.balls.push_back(std::move(ball));
    }
    batterFacedBall(bat, runs, runs == 4, striker.skills.bat[BatK::temperament]);
    bowlerEvent(fig, BowlerEvent::noball);
    BallTextArgs a;
    a.bowler = &bowler;
    a.striker = &striker;
    a.runs = runs;
    a.spin = spin;
    a.kind = BallTextArgs::Kind::noball;
    std::string text = ballText(r, a);
    if (runs) text += cat(" ", runs, " off the bat too.");
    addComment(match, tag, CommentKind::extra, std::move(text));
    inn.freeHit = true;
    if (runs % 2 == 1) swapStrike(inn);
    afterBall(match, false, false, runs);
    return "nb";
  }

  // --- Legal delivery: decide what happened ------------------------------------
  const bool wasFreeHit = inn.freeHit;
  inn.freeHit = false;
  // A bowler in rhythm can produce his special delivery; how it goes depends on the batter
  std::optional<SpecialKey> special;
  if (!wasFreeHit && specialReady(&fig) && r() < specialChance(fig)) {
    special = specialOf(bowler);
    if (special) {
      const SpecialEffect se = specialEffect(bowler, *special, fig.rhythm, B, bat.conf);
      w[O::W] *= se.W;
      w[O::o0] *= se.o0;
      w[O::o1] *= se.o1;
      w[O::o4] *= se.o4;
      w[O::o6] *= se.o6;
      fig.specialAt = fig.balls;
      fig.specials += 1;
    }
  }
  // Everything about this ball, as plain data: if there's a review for the user to decide, the
  // match waits here and finishLegalBall picks it up afterwards.
  BallDecision d;
  d.outcome = weightedOutcome(r, w);
  d.special = special;
  d.wasFreeHit = wasFreeHit;
  d.plan = plan;
  d.fits = eff.fits;
  d.h2hEdge = h2hEdge;
  d.overIdx = over;
  notePlanFaced(inn, striker.id, plan);
  const std::function<double(double)> lean = [plan](double angle) { return planLean(plan, regionOf(angle)); };

  if (d.outcome == O::W) {
    const double running =
        std::min(B.running, batterNow(P(match, *inn.nonStriker), spin, pressure).running);
    const auto ways = fieldDismissals(fieldSet.positions,
                                      planDismissals(plan, dismissalWeights(bowler, phase, spin, running, field)));
    Out type = weighted(r, special ? specialDismissals(*special, ways) : ways);
    if (wasFreeHit) type = Out::runout;
    if (type == Out::caught && r() < 0.06) type = Out::caughtBowled;
    const Player* fielder = nullptr;
    if (type == Out::caught) {
      // The catch goes where the fielders are, to the man standing there
      const std::optional<Pos> k = catchPosition(r, fieldSet.positions, spin, lean);
      fielder = fielderAtOpt(k);
      if (!fielder) fielder = anyFielder();
      if (k) {
        const double jitter = js_round(r() * 8 - 4);
        Shot s;
        s.dir = position(*k).angle + jitter;
        s.dist = position(*k).dist;
        s.pos = k;
        d.shot = s;
      }
    } else if (type == Out::runout) {
      std::vector<Pos> ring;
      for (Pos k : fieldSet.positions)
        if (position(k).zone != Zone::deep && !fieldNow.assign[static_cast<size_t>(k)].empty()) ring.push_back(k);
      if (!ring.empty()) fielder = fielderAtOpt(ring[static_cast<size_t>(std::floor(r() * static_cast<double>(ring.size())))]);
      if (!fielder) fielder = anyFielder();
    } else if (type == Out::caughtKeeper || type == Out::stumped) {
      fielder = &keeper;
    } else if (type == Out::caughtBowled) {
      fielder = &bowler;
    }
    // Chances go down: safe hands and sharp glovework matter
    if (type == Out::caught || type == Out::caughtBowled || type == Out::caughtKeeper) {
      const double skill = type == Out::caughtKeeper ? keeper.skills.field[FieldK::keeping]
                                                     : fielder->skills.field[FieldK::catching];
      if (r() < dropChance(skill)) d.chanceMissed = "catch";
    } else if (type == Out::stumped && r() < missedStumpingChance(keeper.skills.field[FieldK::keeping])) {
      d.chanceMissed = "stumping";
    }
    d.type = type;
    if (fielder) d.fielderId = fielder->id;
    if (d.chanceMissed) {
      inn.drops.push_back({fielder->id, striker.id, inn.legalBalls + 1, *d.chanceMissed});
      d.outcome = *d.chanceMissed == "catch" && r() < 0.3 ? O::o1 : O::o0;
    }
  }

  // The umpire's call on an lbw or a nick (no DRS on a free hit or in a Super Over)
  if (!inn.superOver && !wasFreeHit) {
    if (d.outcome == O::W && (d.type == Out::lbw || d.type == Out::caughtKeeper)) {
      Umpire u;
      u.type = *d.type;
      u.truth = "out";
      u.given = r() < (*d.type == Out::lbw ? UMPIRE_MISS_LBW : UMPIRE_MISS_CK) ? "notOut" : "out";
      d.umpire = u;
    } else if (d.outcome == O::o0 && !d.chanceMissed) {
      const double u = r();
      const Out type = !spin && r() < 0.35 ? Out::caughtKeeper : Out::lbw;
      if (u < FALSE_OUT) {
        Umpire um;
        um.type = type;
        um.truth = "notOut";
        um.given = "out";
        d.umpire = um;
      } else if (u < FALSE_OUT + BIG_APPEAL) {
        Umpire um;
        um.type = type;
        um.truth = "notOut";
        um.given = "notOut";
        d.umpire = um;
      }
    }
  }

  // Off the bat (or not): leg byes, byes, and where the shot went and who got to it
  if (d.outcome != O::W && !d.chanceMissed && !d.umpire) {
    const int runs = kOutcomeRuns[static_cast<size_t>(d.outcome)];
    bool legBye = false;
    if (runs == 1)
      legBye = r() < 0.06;
    else if (runs == 4)
      legBye = r() < 0.02;
    if (legBye) {
      d.extra = "lb";
    } else if (runs == 0) {
      if (r() < byeChance(keeper, spin)) {
        const double y = r();
        d.extra = "b";
        d.outcome = outcomeForRuns(y < 0.8 ? 1 : y < 0.95 ? 4 : 2);
      }
    } else {
      const double dir = shotDirection(r, runs, striker, plan, spin);
      const ShotResolution res = resolveShot(r, runs, dir, fieldSet.positions, fielderAt, wasFreeHit);
      if (res.catchOnRope) {
        d.outcome = O::W;
        d.type = Out::caught;
        const PlayerId& by = fieldNow.assign[static_cast<size_t>(*res.pos)];
        if (!by.empty()) d.fielderId = by;
        Shot s;
        s.dir = dir;
        s.dist = 0.97;
        s.pos = res.pos;
        s.rope = true;
        d.shot = s;
      } else {
        d.outcome = outcomeForRuns(res.runs);
        double dist;
        if (res.saved)
          dist = position(*res.pos).dist;
        else if (runs == 6)
          dist = 1.08 + r() * 0.15;
        else if (runs == 4)
          dist = 1;
        else if (runs == 1)
          dist = 0.3 + r() * 0.35;
        else if (runs == 2)
          dist = 0.6 + r() * 0.3;
        else
          dist = 0.85 + r() * 0.1;
        Shot s;
        s.dir = dir;
        s.dist = dist;
        s.pos = res.pos;
        s.hit = runs;
        d.shot = s;
        // A shy at the stumps that gets away
        if (!res.saved && (res.runs == 1 || res.runs == 2) && r() < 0.012 * js_exp((65 - field.throwing) / 40))
          d.overthrow = r() < 0.15 ? 4 : 1;
      }
    }
  }

  // DRS: the side the decision went against can review it
  if (d.umpire) {
    Umpire& u = *d.umpire;
    u.reviewer = u.given == "out" ? "bat" : "bowl";
    // The batter usually knows if he hit it, the bowler if it was hitting... usually
    u.sure = r() < (u.truth == u.given ? 0.25 : 0.75);
    const int left = u.reviewer == "bat" ? inn.reviewsLeft.bat : inn.reviewsLeft.bowl;
    if (left > 0) {
      const int side = u.reviewer == "bat" ? inn.batting : inn.bowlingSide;
      if (isUser(match, side) && !match.settings.autoReview) {
        match.pendingBall = d;
        match.awaiting = Awaiting::review;
        return "review";
      }
      const bool reviewed = aiReviews(match, d);
      settleReview(match, d, reviewed);
    } else {
      settleReview(match, d, false);
    }
  }
  return finishLegalBall(match, d);
}

bool reviewDecision(Match& match, bool review) {
  if (match.awaiting != Awaiting::review || !match.pendingBall) return false;
  BallDecision d = *match.pendingBall;  // plain copy, off the state tree
  match.pendingBall.reset();
  match.awaiting = Awaiting::none;
  settleReview(match, d, review);
  finishLegalBall(match, d);
  return true;
}

std::optional<PlayerId> playerOfTheMatch(const Match& match) {
  OrderedMap<PlayerId, double> score;
  for (const Innings& inn : match.innings) {
    if (inn.superOver) continue;
    for (const auto& [id, b] : inn.batters) {
      if (b.balls == 0) continue;
      const double sr = (b.runs * 100.0) / b.balls;
      const double* prev = score.find(id);
      score.set(id, (prev ? *prev : 0) + b.runs + ((sr - 130) * b.balls) / 250 + b.sixes);
    }
    for (const auto& [id, f] : inn.bowling) {
      if (f.balls == 0) continue;
      const double econ = (f.runs * 6.0) / f.balls;
      const double* prev = score.find(id);
      score.set(id, (prev ? *prev : 0) + f.wickets * 22 + (8.5 - econ) * (f.balls / 6.0) * 3 + f.dots * 0.3);
    }
  }
  const std::optional<int> winner = match.result ? match.result->winner : std::nullopt;
  std::optional<PlayerId> best;
  double bestScore = -std::numeric_limits<double>::infinity();
  for (const auto& [id, s0] : score) {
    int side = -1;
    for (int t = 0; t < 2; ++t) {
      const MatchTeam& team = match.teams[static_cast<size_t>(t)];
      if (std::find(team.players.begin(), team.players.end(), id) != team.players.end() ||
          (team.impact && team.impact->out == id)) {
        side = t;
        break;
      }
    }
    const double s = s0 * (winner && side == *winner ? 1.15 : 1);
    if (s > bestScore) {
      bestScore = s;
      best = id;
    }
  }
  return best;
}

// ---------------------------------------------------------------------------
// Batch play
// ---------------------------------------------------------------------------

void playOver(Match& match) {
  if (match.status != MatchStatus::live || match.awaiting != Awaiting::none) return;
  // As in the JS, the loop watches the innings it started in: if that innings ends mid-over and a
  // Super Over starts, play carries on until something changes (a decision, the end).
  const size_t idx = static_cast<size_t>(match.current);
  const int startOver = overIndex(match.innings[idx]);
  while (match.status == MatchStatus::live && match.awaiting == Awaiting::none &&
         overIndex(match.innings[idx]) == startOver)
    playBall(match);
}

void simInnings(Match& match) {
  const int idx = match.current;
  while (match.status == MatchStatus::live && match.current == idx) {
    resolvePending(match);
    playBall(match);
  }
}

void simMatch(Match& match) {
  if (match.status == MatchStatus::toss) autoToss(match);
  while (match.status != MatchStatus::complete) {
    if (match.status == MatchStatus::break_) {
      for (int side : {0, 1}) aiUseImpact(match, side);  // no-op for a side that already used it
      startSecondInnings(match);
    }
    simInnings(match);
  }
}

// ---------------------------------------------------------------------------
// Impact Player (IPL): one substitution per side from the 5 named substitutes, at the start of
// an innings, the end of an over, after a wicket, or at the innings break.
// ---------------------------------------------------------------------------

bool impactWindowOpen(const Match& match) {
  if (match.status == MatchStatus::break_) return true;
  if (match.status != MatchStatus::live) return false;
  const Innings& inn = inningsOf(match);
  if (inn.superOver || match.awaiting == Awaiting::review) return false;
  if (match.awaiting == Awaiting::batter || match.awaiting == Awaiting::bowler) return true;
  const size_t ov = static_cast<size_t>(overIndex(inn));
  const bool curEmpty = ov >= inn.overs.size() || inn.overs[ov].balls.empty();
  return inn.legalBalls % 6 == 0 && curEmpty;
}

ImpactOptions impactOptions(const Match& match, int side) {
  const MatchTeam& team = match.teams[static_cast<size_t>(side)];
  ImpactOptions o;
  if (team.impact) {
    o.reason = "Impact Player already used";
    return o;
  }
  if (team.bench.empty()) {
    o.reason = "No substitutes named";
    return o;
  }
  if (!impactWindowOpen(match)) {
    o.reason = "Only at an innings break, the end of an over, or after a wicket";
    return o;
  }
  const Innings* inn = match.status == MatchStatus::live ? &inningsOf(match) : nullptr;
  std::vector<PlayerId> atCrease;
  if (inn) {
    if (inn->striker) atCrease.push_back(*inn->striker);
    if (inn->nonStriker) atCrease.push_back(*inn->nonStriker);
  }
  std::optional<PlayerId> bowlingNow;
  if (inn && inn->bowlingSide == side && inn->currentBowler) {
    const size_t ov = static_cast<size_t>(overIndex(*inn));
    if (ov < inn->overs.size() && !inn->overs[ov].balls.empty()) bowlingNow = inn->currentBowler;
  }
  for (const PlayerId& id : team.players)
    if (id != team.keeperId && std::find(atCrease.begin(), atCrease.end(), id) == atCrease.end() &&
        !(bowlingNow && id == *bowlingNow))
      o.outs.push_back(id);
  const auto overseasIn = std::count_if(team.players.begin(), team.players.end(),
                                        [&](const PlayerId& id) { return P(match, id).overseas; });
  const bool anyOverseasOut =
      std::any_of(o.outs.begin(), o.outs.end(), [&](const PlayerId& id) { return P(match, id).overseas; });
  for (const PlayerId& id : team.bench) {
    ImpactIn x{id, true, ""};
    // Swapping an overseas player out frees a slot; checked per pair in useImpactPlayer
    if (P(match, id).overseas && overseasIn >= MAX_OVERSEAS && !anyOverseasOut) {
      x.ok = false;
      x.reason = "Already 4 overseas players";
    }
    o.ins.push_back(x);
  }
  o.allowed = true;
  return o;
}

bool useImpactPlayer(Match& match, int side, const PlayerId& outId, const PlayerId& inId, bool byAi) {
  const ImpactOptions opts = impactOptions(match, side);
  if (!opts.allowed || std::find(opts.outs.begin(), opts.outs.end(), outId) == opts.outs.end() ||
      std::none_of(opts.ins.begin(), opts.ins.end(), [&](const ImpactIn& x) { return x.id == inId; }))
    return false;
  MatchTeam& team = match.teams[static_cast<size_t>(side)];
  const auto overseas = std::count_if(team.players.begin(), team.players.end(),
                                      [&](const PlayerId& id) { return id != outId && P(match, id).overseas; }) +
                        (P(match, inId).overseas ? 1 : 0);
  if (overseas > MAX_OVERSEAS) return false;

  for (PlayerId& id : team.players)
    if (id == outId) id = inId;
  team.bench.erase(std::remove(team.bench.begin(), team.bench.end(), inId), team.bench.end());
  team.impact = ImpactSub{outId, inId,
                          match.status == MatchStatus::break_ ? std::string("innings break")
                                                              : cat("over ", oversText(inningsOf(match).legalBalls))};
  match.players.at(inId).impactIn = true;
  match.players.at(outId).impactOut = true;

  // Batting side mid-innings: the sub takes the replaced player's place in the queue, if he hadn't batted
  Innings* inn = match.status == MatchStatus::live ? &inningsOf(match) : nullptr;
  if (inn && inn->batting == side && std::find(inn->yetToBat.begin(), inn->yetToBat.end(), outId) != inn->yetToBat.end()) {
    for (PlayerId& id : inn->yetToBat)
      if (id == outId) id = inId;
    inn->batters.set(inId, freshBatter(P(match, inId).startConf.value_or(START_CONFIDENCE)));
  }
  // Fielding side: if the replaced player had just been handed the ball (no balls bowled), pick again
  if (inn && inn->bowlingSide == side && inn->currentBowler && *inn->currentBowler == outId) {
    const size_t ov = static_cast<size_t>(overIndex(*inn));
    if (ov >= inn->overs.size() || inn->overs[ov].balls.empty()) {
      if (inn->overs.size() > ov) inn->overs.resize(ov);
      const BowlingFigures& f = inn->bowling.at(outId);
      if (!f.balls && !f.runs) {
        inn->bowling.erase(outId);
        inn->bowlingOrder.erase(std::remove(inn->bowlingOrder.begin(), inn->bowlingOrder.end(), outId),
                                inn->bowlingOrder.end());
      }
      inn->currentBowler.reset();
    }
  }
  // Chasing side subbing at the break: AI sides re-sort their order so a new batter bats high
  if (match.status == MatchStatus::break_ && byAi && match.innings[0].bowlingSide == side) {
    std::vector<const Player*> ps;
    for (const PlayerId& id : team.players) ps.push_back(&P(match, id));
    team.players.clear();
    for (const Player* p : battingOrder(ps)) team.players.push_back(p->id);
  }
  const std::string note = cat("Impact Player: ", P(match, inId).name, " comes in for ", P(match, outId).name, " (",
                               team.name, ").");
  if (match.status == MatchStatus::live)
    addComment(match, oversText(inningsOf(match).legalBalls), CommentKind::info, note);
  else
    match.impactNotes.push_back(note);
  if (match.status == MatchStatus::live) prepareNext(match, false);
  return true;
}

std::optional<ImpactPlan> aiImpactPlan(const Match& match, int side) {
  const MatchTeam& team = match.teams[static_cast<size_t>(side)];
  if (team.impact || team.bench.empty()) return std::nullopt;
  const bool battedFirst = !match.innings.empty() && match.innings[0].batting == side;
  const ImpactOptions opts = impactOptions(match, side);
  if (!opts.allowed) return std::nullopt;
  auto legal = [&](const PlayerId& o, const PlayerId& i) {
    const auto n = std::count_if(team.players.begin(), team.players.end(),
                                 [&](const PlayerId& id) { return id != o && P(match, id).overseas; });
    return n + (P(match, i).overseas ? 1 : 0) <= MAX_OVERSEAS;
  };
  std::optional<ImpactPlan> best;
  for (const PlayerId& o : opts.outs) {
    for (const ImpactIn& in : opts.ins) {
      if (!legal(o, in.id)) continue;
      const double gain = battedFirst ? P(match, in.id).bowl - P(match, o).bowl : P(match, in.id).bat - P(match, o).bat;
      if (!best || gain > best->gain) best = ImpactPlan{o, in.id, gain};
    }
  }
  if (best && best->gain >= 12) return best;
  return std::nullopt;
}

void aiUseImpact(Match& match, int side) {
  const std::optional<ImpactPlan> plan = aiImpactPlan(match, side);
  if (plan) useImpactPlayer(match, side, plan->out, plan->in, true);
}

}  // namespace t20
