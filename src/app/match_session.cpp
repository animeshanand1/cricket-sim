#include "app/match_session.hpp"

#include <unordered_map>

#include "core/str.hpp"

#include "engine/league.hpp"
#include "engine/rules.hpp"

namespace t20::app {

namespace {

SideInput sideOf(const Team& team, Pitch pitch) {
  const BestXI xi = pickBestXI(team, pitch);
  std::unordered_map<PlayerId, const Player*> by;
  for (const auto& p : team.squad) by[p.id] = &p;
  SideInput s;
  s.id = team.id;
  s.name = team.name;
  s.color = team.color;
  s.captain = team.captain;
  s.keeperId = xi.keeperId;
  for (const auto& id : xi.ids) s.players.push_back(*by.at(id));
  for (const auto& id : benchOf(team, xi.ids)) s.bench.push_back(*by.at(id));
  return s;
}

}  // namespace

MatchSession::MatchSession() : league_(generateLeague()) {}

void MatchSession::newMatch(int home, int away, int32_t seed, std::optional<std::string> id) {
  home_ = home;
  away_ = away;
  seed_ = seed;
  const Team& h = league_[static_cast<size_t>(home)];
  const Team& a = league_[static_cast<size_t>(away)];
  MatchSetup setup;
  setup.seed = seed;
  setup.id = id ? id : makeId ? std::optional<std::string>(makeId(seed)) : std::nullopt;
  setup.matchups = saved_;
  setup.pitch = h.pitch;
  setup.teams = {sideOf(h, h.pitch), sideOf(a, h.pitch)};
  // The user captains the home side and makes the calls (bowlers, batters, reviews)
  setup.userTeam = 0;
  match_ = createMatch(setup);
  autoToss(match_);
  hasMatch_ = true;
  adopted_ = false;
  reported_ = false;
  watching = false;
  watchClock_ = 0;
  events_.clear();
  winChance_.reset();  // the last match's estimate means nothing here
  winPoints_.clear();
  pendingPoint_.reset();
  lastWickets_ = 0;
  lastInnings_ = -1;
  afterBalls();
}

void MatchSession::adopt(Match match, std::vector<Team> league) {
  league_ = std::move(league);
  const TeamId home = match.fixture ? match.fixture->home : match.teams[0].id;
  home_ = away_ = 0;
  for (size_t i = 0; i < league_.size(); ++i) {
    if (league_[i].id == home) home_ = static_cast<int>(i);
    else if (league_[i].id == match.teams[0].id || league_[i].id == match.teams[1].id) away_ = static_cast<int>(i);
  }
  seed_ = match.seed;
  match_ = std::move(match);
  hasMatch_ = true;
  adopted_ = true;
  // A match that was already over when it came in has been recorded: never report it again
  reported_ = match_.status == MatchStatus::complete;
  watching = false;
  watchClock_ = 0;
  events_.clear();
  winChance_.reset();
  winPoints_.clear();
  pendingPoint_.reset();
  lastWickets_ = 0;
  lastInnings_ = -1;
  afterBalls();
}

void MatchSession::clear() {
  hasMatch_ = false;
  adopted_ = false;
  watching = false;
  winRun_.reset();
  winChance_.reset();
  events_.clear();
}

void MatchSession::rematch() { newMatch(home_, away_, seed_ + 7919); }

void MatchSession::nextBall() {
  if (!hasMatch_) return;
  if (match_.status == MatchStatus::break_) {
    startChase();
    return;
  }
  // The engine plays nothing while it waits for the captain's call (matchSlice nextBall)
  if (match_.status != MatchStatus::live || match_.awaiting != Awaiting::none) return;
  const Innings& inn = inningsOf(match_);
  const std::string batter = inn.striker ? match_.players.at(*inn.striker).name : "";
  const std::string bowler = inn.currentBowler ? match_.players.at(*inn.currentBowler).name : "";
  const std::string outcome = playBall(match_);
  // "review": the ball waits for the user's DRS call; its event comes with reviewCall()
  if (!outcome.empty() && outcome != "review") recordLastBall(outcome, batter, bowler);
  afterBalls();
}

void MatchSession::nextOver() {
  // engine playOver, one ball at a time so each ball gets its event
  if (!hasMatch_ || match_.status != MatchStatus::live || match_.awaiting != Awaiting::none) return;
  const int inningsNo = match_.current;
  const int over = overIndex(inningsOf(match_));
  for (int guard = 0; guard < 60; ++guard) {
    nextBall();
    if (match_.status != MatchStatus::live || match_.awaiting != Awaiting::none || match_.current != inningsNo) break;
    if (overIndex(inningsOf(match_)) != over) break;
  }
}

void MatchSession::simInnings() {
  if (!hasMatch_) return;
  if (match_.status == MatchStatus::break_) startSecondInnings(match_);
  if (match_.status == MatchStatus::live) {
    t20::simInnings(match_);
    events_.push_back(BallEvent{"", "", "", match_.status == MatchStatus::complete});
  }
  afterBalls();
}

void MatchSession::simMatch() {
  if (!hasMatch_ || match_.status == MatchStatus::complete) return;
  t20::simMatch(match_);
  events_.push_back(BallEvent{"", "", "", true});
  afterBalls();
}

void MatchSession::startChase() {
  if (hasMatch_ && match_.status == MatchStatus::break_) {
    startSecondInnings(match_);
    afterBalls();
  }
}

// --- captain's decisions ------------------------------------------------------------------

void MatchSession::pickBowler(const PlayerId& id) {
  if (t20::setBowler(match_, id)) afterBalls();
}

void MatchSession::pickBatter(const PlayerId& id) {
  if (setNextBatter(match_, id)) afterBalls();
}

void MatchSession::changeBowler() {
  if (t20::changeBowler(match_)) afterBalls();
}

void MatchSession::setBowlingPlan(std::optional<Plan> plan) {
  if (t20::setBowlingPlan(match_, plan)) afterBalls();
}

void MatchSession::setField(FieldPreset preset, std::optional<std::vector<Pos>> positions) {
  if (t20::setField(match_, preset, std::move(positions))) afterBalls();
}

void MatchSession::setBatterTarget(const PlayerId& bowlerId, std::optional<TargetMode> mode) {
  t20::setBatterTarget(match_, bowlerId, mode);
  afterBalls();
}

void MatchSession::setBatterTempo(const PlayerId& batterId, Tempo level) {
  t20::setBatterTempo(match_, batterId, level);
  afterBalls();
}

void MatchSession::setAutoBowl(bool on) {
  match_.settings.autoBowl = on;
  if (on && match_.awaiting == Awaiting::bowler) resolvePending(match_);
  afterBalls();
}

void MatchSession::setAutoReview(bool on) {
  match_.settings.autoReview = on;
  if (on && match_.awaiting == Awaiting::review) {
    resolvePending(match_);
    recordLastBall("", "", "");
  }
  afterBalls();
}

void MatchSession::reviewCall(bool review) {
  if (match_.awaiting != Awaiting::review) return;
  if (reviewDecision(match_, review)) recordLastBall("", "", "");
  afterBalls();
}

bool MatchSession::impactSub(int side, const PlayerId& outId, const PlayerId& inId) {
  const bool ok = useImpactPlayer(match_, side, outId, inId);
  if (ok) afterBalls();
  return ok;
}

void MatchSession::letAiDecide() {
  if (match_.awaiting == Awaiting::none) return;
  const bool review = match_.awaiting == Awaiting::review;
  resolvePending(match_);
  if (review) recordLastBall("", "", "");
  afterBalls();
}

std::optional<HeadToHead> MatchSession::headToHead(const PlayerId& bat, const PlayerId& bowl) const {
  return t20::headToHead(saved_, &live_, bat, bowl);
}

std::string_view MatchSession::edgeVerdictOf(const PlayerId& bat, const PlayerId& bowl) const {
  const double* e = match_.h2h.find(matchupKey(bat, bowl));
  return edgeVerdict(e ? *e : 0.0);
}

// The ball that just finished (after a review it is no longer "review" but its real result)
void MatchSession::recordLastBall(const std::string& outcome, const std::string& batter, const std::string& bowler) {
  BallEvent ev{outcome, batter, bowler, match_.status == MatchStatus::complete};
  // the ball sits in the latest over that has any (a wicket can end the innings, so search back)
  for (auto inn = match_.innings.rbegin(); inn != match_.innings.rend() && ev.outcome.empty(); ++inn) {
    for (auto ov = inn->overs.rbegin(); ov != inn->overs.rend(); ++ov) {
      if (ov->balls.empty()) continue;
      const Ball& b = ov->balls.back();
      ev.outcome = b.kind == BallKind::wicket ? "W" : b.kind == BallKind::b6 ? "6" : b.kind == BallKind::b4 ? "4" : b.label;
      if (b.faced) ev.batter = match_.players.at(*b.faced).name;
      ev.bowler = match_.players.at(ov->bowler).name;
      break;
    }
  }
  events_.push_back(ev);
}

void MatchSession::afterBalls() {
  if (match_.status != MatchStatus::live) watching = false;
  if (match_.status == MatchStatus::complete && !reported_) {
    reported_ = true;
    if (onComplete) onComplete(match_);
  }
  live_ = matchupsOf(match_);
  restartWinProb();
}

void MatchSession::addWinPoint(WinPoint p) {
  // the same moment again (an AI call, a setting changed): the newer estimate wins
  if (!winPoints_.empty() && winPoints_.back().innings == p.innings && winPoints_.back().balls == p.balls &&
      winPoints_.back().label == p.label) {
    winPoints_.back() = std::move(p);
    return;
  }
  winPoints_.push_back(std::move(p));
}

void MatchSession::restartWinProb() {
  // Is this a checkpoint for the chart?
  pendingPoint_.reset();
  const int first = match_.battingFirst.value_or(0);
  if (!match_.innings.empty()) {
    const Innings& inn = inningsOf(match_);
    const int inningsNo = match_.current;
    const bool wicket = inningsNo == lastInnings_ && inn.wickets > lastWickets_;
    lastInnings_ = inningsNo;
    lastWickets_ = inn.wickets;
    if (!inn.superOver && inningsNo <= 1) {
      WinPoint p;
      p.innings = inningsNo;
      p.balls = inn.legalBalls;
      p.maxBalls = inn.maxBalls;
      p.label = cat(match_.teams[static_cast<size_t>(inn.batting)].id, ' ', inn.runs, '/', inn.wickets, " (", oversText(inn.legalBalls), " ov)");
      if (match_.status == MatchStatus::complete) {
        const auto& r = match_.result;
        p.first = r && r->winner ? (*r->winner == first ? 1.0 : 0.0) : 0.5;
        p.label = "End";
        addWinPoint(p);
      } else if (match_.status == MatchStatus::break_) {
        p.label = "Innings break";
        pendingPoint_ = p;
      } else if (match_.status == MatchStatus::live) {
        const bool overEnd = inn.legalBalls % 6 == 0;
        const bool tight = inn.target && inn.maxBalls - inn.legalBalls <= 18;
        if (overEnd || wicket || tight) pendingPoint_ = p;
      }
    }
  }
  // The previous estimate stays on screen until the new one is ready
  if (match_.status == MatchStatus::complete) {
    winRun_.reset();
    const auto& r = match_.result;
    if (r && r->winner) winChance_ = std::array<double, 2>{*r->winner == 0 ? 1.0 : 0.0, *r->winner == 1 ? 1.0 : 0.0};
    else winChance_ = std::array<double, 2>{0.5, 0.5};
    return;
  }
  winRun_ = std::make_unique<WinChanceRun>(match_, inningsOf(match_).superOver ? 60 : 120);
}

void MatchSession::update(double dt) {
  if (!hasMatch_) return;
  // Watch: a ball at a time, waiting whenever there's a call for the captain to make
  if (watching && match_.status == MatchStatus::live && match_.awaiting == Awaiting::none) {
    watchClock_ += dt;
    if (watchClock_ >= watchInterval) {
      watchClock_ = 0;
      const int wickets = inningsOf(match_).wickets, inningsNo = match_.current;
      nextBall();
      if (stopOnWicket && match_.current == inningsNo && inningsOf(match_).wickets > wickets) watching = false;
      if (match_.current != inningsNo) watching = false;  // a new innings (a Super Over): pause
    }
  }
  if (winRun_) {
    // ~120 sims spread over a few frames keeps the UI at 60 fps
    if (winRun_->step(12)) {
      winChance_ = winRun_->result();
      winRun_.reset();
      if (pendingPoint_) {
        pendingPoint_->first = (*winChance_)[static_cast<size_t>(match_.battingFirst.value_or(0))];
        addWinPoint(*pendingPoint_);
        pendingPoint_.reset();
      }
    }
  }
}

std::vector<BallEvent> MatchSession::takeEvents() {
  std::vector<BallEvent> out;
  out.swap(events_);
  return out;
}

}  // namespace t20::app
