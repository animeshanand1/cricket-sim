#pragma once
// The game controller: the C++ replacement for the Redux slices, the thunks and the store's
// "match finished" listener (store.js, thunks.js, *Slice.js). Every method mutates the GameState
// exactly as the JS action of the same name does, so the two games stay in step.
//
// Randomness only makes seeds (new seasons, new friendlies, "surprise me" pitches); it comes from
// injectable sources so tests can replay the JS game's runs.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "app/game_state.hpp"
#include "engine/editor.hpp"
#include "engine/match.hpp"
#include "engine/trades.hpp"

namespace t20::app {

struct Seeds {
  std::function<double()> random;  // Math.random: [0, 1)
  std::function<double()> now;     // Date.now: milliseconds
  static Seeds system();            // real randomness and the clock
};

struct SimOptions {
  bool includeUser = false;
  std::optional<int> stopAfter;
  std::function<void(int done)> progress;  // called every two games (the busy indicator)
};

struct CreateOutcome {
  bool ok = false;
  PlayerId id;
  OrderedMap<std::string, std::string> errors;  // "form" = a squad-level problem
};

struct CaptainOutcome {
  bool ok = false;
  bool inXI = false, injured = false;
  std::string error;
};

class Game {
 public:
  explicit Game(GameState state, Seeds seeds = Seeds::system());
  // A brand-new game: fresh world from the roster, empty career, default setup
  static GameState freshState();
  // The boot step after loading (store.js): the selected XI must still exist in the squad
  void boot();

  GameState& state() { return s_; }
  const GameState& state() const { return s_; }
  // League as it is now (world + development), as every screen and match sees it
  std::vector<Team> league() const;

  // Which parts changed since the last call (for the save manager)
  struct Dirty {
    bool setup = false, career = false, season = false, world = false, match = false;
    bool any() const { return setup || career || season || world || match; }
  };
  Dirty takeDirty();
  void markAllDirty();
  // Goes up with every change to the state, so screens can cache what they derive from it
  uint64_t revision() const { return rev_; }

  // ---- setup (setupSlice reducers)
  void goTo(const std::string& screen);
  void openPlayers();
  void closePlayers();
  void selectUserTeam(const TeamId& id);
  void selectOpponent(const TeamId& id);
  void setPitch(const std::string& pitch);
  void setXI(const std::vector<PlayerId>& xi, const std::optional<PlayerId>& keeperId,
             const std::optional<PlayerId>& captainId = std::nullopt);
  void setTraining(size_t slot, const std::optional<PlayerId>& playerId, const std::string& drill = "");
  void clearTraining();
  void togglePlayer(const PlayerId& id);
  void movePlayer(const PlayerId& id, int dir);
  void swapPlayers(const PlayerId& id1, const PlayerId& id2);
  void reorderXI(int fromIndex, int toIndex);
  void setCaptain(const PlayerId& id);
  void captainCleared();
  void setKeeper(const PlayerId& id);
  void updateMatchSettings(std::optional<double> aggression, std::optional<bool> autoBowl);

  // ---- friendlies and matches (thunks.js)
  void chooseTeam(const TeamId& teamId);
  void autoPick();
  void startFixture();
  void rematch();
  void newGame();
  // Match actions (matchSlice); each one checks whether the match just finished
  void callToss(const std::string& call);
  void chooseToss(TossDecision decision);
  void nextBall();
  void nextOver();
  void simInnings();
  void simMatch();
  void startChase();
  void pickBowler(const PlayerId& id);
  void changeBowler();
  void setBowlingPlan(std::optional<Plan> plan);
  void setField(FieldPreset preset, std::optional<std::vector<Pos>> positions = std::nullopt);
  void setBatterTarget(const PlayerId& bowlerId, std::optional<TargetMode> mode);
  void pickBatter(const PlayerId& id);
  void setAggression(double level);
  void setBatterTempo(const PlayerId& batterId, Tempo level);
  void setAutoBowl(bool on);
  void impactSub(int side, const PlayerId& out, const PlayerId& in);
  void reviewCall(bool review);
  void setAutoReview(bool on);
  // A match played elsewhere (the match screen's own session) that has just finished
  void recordFinishedMatch(const Match& m);

  // ---- season
  void startSeason(const TeamId& userTeamId);
  void simFixtures(const SimOptions& opts = {});
  void playNextFixture();
  void backToSeason();
  void nextSeason();
  void acceptJob(const TeamId& teamId);

  // ---- off-season
  void startOffseason(std::optional<std::vector<TeamId>> order = std::nullopt, const std::string& then = "players",
                      std::optional<TeamId> userTeam = std::nullopt);
  void contractRenewed(const PlayerId& id, int years);
  void contractReleased(const PlayerId& id);
  void contractsClosed();
  void retentionSet(const std::vector<PlayerId>& ids);
  void retentionClosed();
  void scout(const PlayerId& id);
  void makeDraftPick(const PlayerId& id);
  void completeDraft();
  void auctionBid();
  void auctionPass();
  void auctionLimit(double max);
  void auctionAutoRest();
  void auctionRtm(const std::string& step, double value);
  void finishOffseason();
  void offseasonReportDismissed();

  // ---- club business
  bool clubUpgradeOrdered(const std::string& key);
  bool clubTicketsSet(const std::string& tier);
  bool clubPitchSet(std::optional<Pitch> pitch);
  bool clubPrizeShareSet(int share);
  bool clubStaffSet(const std::string& role, int tier);
  bool clubCoachAssigned(const PlayerId& playerId, const std::string& area, int tier);
  bool clubCoachRemoved(const PlayerId& playerId);
  bool clubSponsorSigned(const std::string& slot, size_t index);

  // ---- editor, new players, the club captain
  EditResult savePlayerEdit(const PlayerId& id, const EditDraft& draft);
  void restorePlayer(const PlayerId& id);
  void allEditsRestored();
  CreateOutcome createNewPlayer(const TeamId& teamId, const EditDraft& draft);
  CaptainOutcome makeClubCaptain(const PlayerId& id);
  void removeClubCaptain(const TeamId& teamId);
  void chooseCaptain(const PlayerId& id);

  // ---- transfers
  TradeVerdict proposeTrade(const TeamId& other, const Offer& offer);
  bool signCover(const PlayerId& injuredId, const PlayerId& id);

  // ---- career
  void resetCareer();
  void openCard(const PlayerId& id);
  void openEditor(const PlayerId& id);
  void closeCard();

 private:
  int32_t newSeed();
  void recordLineup();
  void withMatch(const std::function<void(Match&)>& fn);
  void afterMatch(const Match& m);
  void takeGate(int fixtureNo);
  void boardMatchResult(bool won, bool tied);
  void matchFitness(const Match& m);
  void seasonTargetSet(const TeamId& team, int rank, int season);

  // Every change goes through here: it flags the part for saving and bumps the revision
  Dirty& mark() {
    ++rev_;
    return dirty_;
  }

  GameState s_;
  Seeds seeds_;
  Dirty dirty_;
  uint64_t rev_ = 0;
};

}  // namespace t20::app
