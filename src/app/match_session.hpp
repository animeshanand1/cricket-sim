#pragma once
// The live match the UI is showing: owns the engine Match, runs the win-probability job a few
// sims per frame, and drives Watch mode. All engine calls from the Match screen go through here.

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/match.hpp"
#include "engine/matchups.hpp"
#include "engine/types.hpp"
#include "engine/winprob.hpp"

namespace t20::app {

// What the last ball did, for moment banners and the pip pop
struct BallEvent {
  std::string outcome;  // playBall's result: "W", "4", "6", "wd", ...
  std::string batter;   // who faced it
  std::string bowler;
  bool matchOver = false;
};

class MatchSession {
 public:
  MatchSession();

  // A friendly between two league sides (indexes into league()), at the home side's ground.
  // id: the match id the career records it under (default: the seed)
  void newMatch(int home, int away, int32_t seed, std::optional<std::string> id = std::nullopt);

  // The squads to pick from: the saved world with development (Game::league()); the fixed
  // roster until this is called
  void setLeague(std::vector<Team> league) { league_ = std::move(league); }
  // The career's batter v bowler book: head to head and the edges a match starts with
  void setSavedMatchups(const MatchupBook* book) { saved_ = book; }
  // Called once when a match finishes (the Game records it and the save manager saves)
  std::function<void(const Match&)> onComplete;
  // Match ids when newMatch isn't given one (rematches): unique across runs, so the career never
  // skips a match as already recorded
  std::function<std::string(int32_t seed)> makeId;

  const std::vector<Team>& league() const { return league_; }
  const Match& match() const { return match_; }
  bool hasMatch() const { return hasMatch_; }
  bool isLive() const { return hasMatch_ && (match_.status == MatchStatus::live || match_.status == MatchStatus::break_); }
  const Team& homeTeam() const { return league_[static_cast<size_t>(home_)]; }

  // Play a new match between the same two sides, or the next fixture
  void rematch();

  // Dock actions. Next ball / next over do nothing while a decision is pending (as in the JS);
  // the sims let the AI make any call that comes up.
  void nextBall();
  void nextOver();
  void simInnings();
  void simMatch();
  void startChase();

  // Captain's decisions (matchSlice.js reducers)
  void pickBowler(const PlayerId& id);
  void pickBatter(const PlayerId& id);
  void changeBowler();
  void setBowlingPlan(std::optional<Plan> plan);  // nullopt = auto
  void setField(FieldPreset preset, std::optional<std::vector<Pos>> positions = std::nullopt);
  void setBatterTarget(const PlayerId& bowlerId, std::optional<TargetMode> mode);
  void setBatterTempo(const PlayerId& batterId, Tempo level);
  void setAutoBowl(bool on);
  void setAutoReview(bool on);
  void reviewCall(bool review);
  // Impact Player: `inId` from the bench replaces `outId` (false if the engine refuses it)
  bool impactSub(int side, const PlayerId& outId, const PlayerId& inId);
  // The vice-captain (AI) answers whatever call is pending (scripted runs, screenshots)
  void letAiDecide();

  // Head to head: the career's record plus this match
  std::optional<HeadToHead> headToHead(const PlayerId& bat, const PlayerId& bowl) const;
  // "bowler" / "batter" / "" from the edge the engine plays with today
  std::string_view edgeVerdictOf(const PlayerId& bat, const PlayerId& bowl) const;

  // Called once per frame: Watch mode ticks and the win-probability job
  void update(double dt);

  bool watching = false;
  bool stopOnWicket = true;  // Watch pauses when a wicket falls
  double watchInterval = 0.9;  // seconds per ball (Slow 1.6 / Medium 0.9 / Fast 0.35)

  // Win chance of side 0 and 1 (0-1), once the current sims are done
  std::optional<std::array<double, 2>> winChance() const { return winChance_; }

  // Events since the UI last asked (the UI turns them into banners)
  std::vector<BallEvent> takeEvents();

 private:
  void afterBalls();
  void recordLastBall(const std::string& outcome, const std::string& batter, const std::string& bowler);
  void restartWinProb();

  std::vector<Team> league_;
  Match match_;
  bool hasMatch_ = false;
  int home_ = 0, away_ = 0;
  int32_t seed_ = 0;
  MatchupBook live_;
  const MatchupBook* saved_ = nullptr;
  bool reported_ = false;
  double watchClock_ = 0;
  std::unique_ptr<WinChanceRun> winRun_;
  std::optional<std::array<double, 2>> winChance_;
  std::vector<BallEvent> events_;
};

}  // namespace t20::app
