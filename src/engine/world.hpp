#pragma once
// The living league (world.js): squads, every player's current skills, potential and rating
// history, retirements and the youth draft. Starts from the fixed roster and changes every
// off-season. All functions mutate a World in place; randomness is seeded by the world's year.

#include <optional>
#include <string>
#include <vector>

#include "engine/career_types.hpp"
#include "engine/editor.hpp"
#include "engine/rng.hpp"
#include "engine/world_types.hpp"

namespace t20 {

inline constexpr int WORLD_VERSION = 3;
inline constexpr int SQUAD_MIN = 18;
inline constexpr int SQUAD_MAX = 25;
inline constexpr int DRAFT_ROUNDS = 2;
inline constexpr int DRAFT_POOL = 26;
inline constexpr int SCOUT_REPORTS = 6;
inline constexpr int MAX_RETIRE_PER_TEAM = 4;
int roleTarget(Role r);

void recompute(Player& p);
double potentialFor(const Player& p);

World createWorld(const std::vector<Team>& league);
void upgradeWorld(World& world);
const TeamInfo& teamMeta(const TeamId& id);

// Teams in league order with their squads as player objects; a club captain sets captainId
std::vector<Team> worldTeams(const World& world);
std::optional<TeamId> teamOfId(const World& world, const PlayerId& id);

// The club captain: counts only while he is still in the squad
std::optional<PlayerId> clubCaptainOf(const World& world, const TeamId& teamId);
bool setClubCaptain(World& world, const TeamId& teamId, const std::optional<PlayerId>& id);

struct CreateResult {
  bool ok = false;
  std::string error;
  PlayerId id;
};
// A player the user designed (validateNewPlayer), signed straight into a squad
CreateResult createPlayer(World& world, const TeamId& teamId, const Edit& edit);

enum class MakeKind { prospect, entrant, journeyman };
// A generated player. role unset = drawn from the role mix (consumes the stream first, as JS does)
Player& makePlayer(World& world, Rng& rng, MakeKind kind = MakeKind::prospect, std::optional<Role> role = std::nullopt,
                   double quality = 0);
Role pickRole(Rng& rng);

double playerValue(const Player& p, double future = 0.35);
struct RoleCounts {
  int wk = 0, bat = 0, ar = 0, bowl = 0;
  int operator[](Role r) const { return r == Role::WK ? wk : r == Role::BAT ? bat : r == Role::AR ? ar : bowl; }
};
RoleCounts roleCounts(const World& world, const TeamId& teamId);
Role neediestRole(const World& world, const TeamId& teamId);

struct PotentialView {
  bool exact;
  double lo, hi;
};
std::optional<PotentialView> potentialView(const World& world, const Player& p, const std::optional<TeamId>& userTeam);
std::string_view potentialGrade(double v);

double retireChance(const Player& p);

// The off-season up to contracts/retention: ageing, coaching, retirements, academy prospects
const OffseasonReport& runOffseason(World& world, std::optional<std::vector<TeamId>> order,
                                    std::optional<TeamId> userTeam, std::optional<std::string> then);

std::vector<PlayerId> expiring(const World& world, const TeamId& teamId);
bool renewPlayer(World& world, const TeamId& teamId, const PlayerId& id, int years);
bool releasePlayer(World& world, const TeamId& teamId, const PlayerId& id);
void closeContracts(World& world);
bool setRetention(World& world, const std::vector<PlayerId>& ids);
void closeRetention(World& world);

std::optional<TeamId> draftOnTheClock(const World& world);
std::vector<PlayerId> draftAvailable(const World& world);
bool scoutProspect(World& world, const PlayerId& id);
bool draftPick(World& world, const TeamId& teamId, const std::optional<PlayerId>& id);
std::optional<PlayerId> aiDraftChoice(const World& world, const TeamId& teamId);
void advanceDraft(World& world, const std::optional<TeamId>& userTeam);
void finishDraft(World& world);
AuctionResult closeAuction(World& world);
void fillSquads(World& world);

struct Trend {
  HistoryPt from, to;
};
std::optional<Trend> ratingTrend(const Player& p);

}  // namespace t20
