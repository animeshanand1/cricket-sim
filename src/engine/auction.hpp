#pragma once
// The player auction (auction.js), IPL style: marquee players first, then sets by role, capped
// before uncapped. AI franchises bid up to their own valuation; the user is asked whenever the AI
// bidding settles and they could still bid. In a mega auction a player's former team can use a
// Right to Match card. State lives in world.auction; every function mutates the world.

#include <functional>
#include <limits>
#include <optional>
#include <string>

#include "engine/world_types.hpp"

namespace t20 {

Auction& startAuction(World& world, const std::optional<TeamId>& userTeam, const std::string& type);
// Most the user can bid on this player right now (keeps enough back to fill the squad to 18)
double userCeiling(const World& world, const TeamId& teamId, const Player& p);
// "bid" | "rtm-invoke" | "rtm-raise" | "rtm-match", or none when the AI can keep going
std::optional<std::string> awaitingUser(const World& world);

void rtmInvoke(World& world, bool yes);
void rtmRaise(World& world, double amount);
void rtmMatch(World& world, bool yes);
// Run until the user has a decision or the auction ends: returns the decision, "done" or "paused"
std::string runAuction(World& world, int maxLots = std::numeric_limits<int>::max());

bool userBid(World& world);
bool userPass(World& world);
bool userLimit(World& world, double max);
void userAutoRest(World& world);

// After the last lot: squads under 18 sign unsold players at base price, or journeymen
AuctionResult finishAuction(World& world, const std::function<Player&(const TeamId&)>& makeJourneyman);

}  // namespace t20
