#pragma once
// The mid-season transfer room (trades.js): a trade window with the other franchises, and cover
// for players out injured for a while. The log is world.transfers, newest first.

#include <optional>
#include <string>
#include <vector>

#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

inline constexpr int TRADE_OPEN = 25;
inline constexpr int TRADE_CLOSE = 45;
inline constexpr int MAX_TRADES = 2;
inline constexpr size_t MAX_PER_SIDE = 2;
inline constexpr size_t SQUAD_FLOOR = 15;
inline constexpr double PREMIUM = 1.08;
inline constexpr double CASH_STEP = 25;
inline constexpr double MAX_CASH_BACK = 1000;
inline constexpr int REPLACE_MIN_MATCHES = 3;
inline constexpr int AI_REPLACE_MATCHES = 5;
inline constexpr int FREE_AGENTS_PER_ROLE = 4;

struct TradeWindow {
  std::string state;  // none | before | open | closed
  bool open = false;
  int games = 0;
};
TradeWindow tradeWindow(const Season* season);
int tradesMade(const World& world, const TeamId& teamId);
double tradeValue(const World& world, const TeamId& teamId, const Player& p, bool joining);

struct Offer {
  std::vector<PlayerId> give, get;
  double cash = 0;  // paid by the proposing club (negative: the other club pays)
};
struct TradeVerdict {
  bool ok = false;
  std::vector<std::string> errors;
  bool accept = false;
  double shortBy = 0, theirIn = 0, theirOut = 0;
  bool done = false;
};
TradeVerdict evaluateTrade(const World& world, const TeamId& teamId, const TeamId& otherId, const Offer& offer);
TradeVerdict makeTrade(World& world, const TeamId& teamId, const TeamId& otherId, const Offer& offer);

void topUpFreeAgents(World& world);
std::vector<PlayerId> replaceable(const World& world, const TeamId& teamId);
std::optional<std::string> replacementBlock(const World& world, const TeamId& teamId, const Player* p);
struct ReplacementOption {
  PlayerId id;
  double fee;
  std::optional<std::string> block;
};
std::vector<ReplacementOption> replacementOptions(const World& world, const TeamId& teamId, const PlayerId& injuredId,
                                                  size_t limit = 8);
bool signReplacement(World& world, const TeamId& teamId, const PlayerId& injuredId, const PlayerId& id);
void aiReplacements(World& world, const std::optional<TeamId>& userTeam);

}  // namespace t20
