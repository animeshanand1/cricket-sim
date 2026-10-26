#pragma once
// Money (finance.js): market values, contracts, the purse, retention and renewals. All amounts are
// in lakh (1 crore = 100 lakh), following the IPL auction rules.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/world_types.hpp"

namespace t20 {

inline constexpr double PURSE = 12000;
inline constexpr int SQUAD_MIN_AUCTION = 18;
inline constexpr int SQUAD_MAX_AUCTION = 25;
inline constexpr int MAX_OVERSEAS_SQUAD = 8;
inline constexpr int MEGA_EVERY = 3;
inline constexpr double RETENTION_SLABS[] = {1800, 1400, 1100, 1800, 1400};
inline constexpr double RETENTION_UNCAPPED = 400;
inline constexpr int MAX_RETAIN = 6;
inline constexpr int MAX_RETAIN_CAPPED = 5;
inline constexpr int MAX_RETAIN_UNCAPPED = 2;
inline constexpr double OVERSEAS_FEE_CAP = 1800;
inline constexpr double BASE_PRICES[] = {30, 50, 75, 100, 125, 150, 200};
inline constexpr double PRICE_CAP = 2700;

inline bool isMegaYear(int year) { return year > 0 && year % MEGA_EVERY == 0; }

// "₹2.5 cr" / "₹75 L"
std::string money(std::optional<double> l);
double nextBid(double price);
bool isCapped(const Player& p);
double worth(const Player& p);
double marketValue(const Player& p);
double basePrice(const Player& p);

double payroll(const World& world, const TeamId& teamId);
double purseLeft(const World& world, const TeamId& teamId);
int overseasCount(const World& world, const TeamId& teamId);

double personalityFactor(std::string_view kind, const Player& p);
// The most `teamId` would pay for `p` right now (lakh), or 0 if it can't or won't bid
double teamValuation(const World& world, const TeamId& teamId, const Player& p, bool noise,
                     std::string_view personality);

Contract contract(double cost, int years, ContractVia via, int year, bool overseas = false);
void initialContracts(World& world);
inline double rookieCost(std::optional<int> pickNo) {
  return !pickNo ? 30 : std::max(30.0, 160.0 - (*pickNo - 1) * 7);
}
double renewalDemand(const Player& p, int years = 2);
std::vector<int> renewalYears(const Player& p);

struct RenewDeal {
  int years;
  double demand;
};
std::optional<RenewDeal> aiRenews(const World& world, const TeamId& teamId, const Player& p,
                                  std::string_view personality);

std::vector<double> retentionCosts(const World& world, const std::vector<PlayerId>& ids);
bool retentionValid(const World& world, const std::vector<PlayerId>& ids);
std::vector<PlayerId> aiRetention(const World& world, const TeamId& teamId, std::string_view personality);

}  // namespace t20
