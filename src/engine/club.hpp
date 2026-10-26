#pragma once
// The club as a business (club.js): a club account apart from the player purse. Sponsors, prize
// money, the home ground (stands, hospitality, fan zone, curator), tickets, the youth academy, the
// high-performance centre and coaches. Every decision is a plan for the NEXT season. AI clubs keep
// the default club, so the league plays as before. Amounts in lakh.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/fans.hpp"
#include "engine/season_types.hpp"
#include "engine/world_types.hpp"

namespace t20 {

inline constexpr double START_BANK = 3000;
inline constexpr double CENTRAL_SHARE = 2500;
inline constexpr double RUNNING_COSTS = 1500;
inline constexpr double UPKEEP_PER_LEVEL = 15;
inline constexpr double RED_INK_PENALTY = 8;
inline constexpr size_t HISTORY_KEEP = 10;
inline constexpr double SEATS_PER_STAND = 5000;
inline constexpr double FAN_SPEND = 150;
inline constexpr double HOSPITALITY_PER_LEVEL = 40;
inline constexpr double MAX_GOODWILL = 10;
inline constexpr int MAX_LIFT = 3;
inline constexpr double PERSONAL_BASE = 0.8;
inline constexpr double STAFF_BASE = 0.35;
inline constexpr int STAFF_MAX_AGE = 26;
inline constexpr double HPC_BASE = 0.12;

double basePopularity(const TeamId& team);

struct Facility {
  std::string_view key, label, area;
  std::vector<double> costs;
  std::string help;
};
const std::vector<Facility>& FACILITIES();
const Facility* facility(std::string_view key);
int maxLevel(std::string_view key);
std::optional<double> upgradeCost(std::string_view key, int level);
inline constexpr double CURATOR_SHARE[] = {0, 0.8, 0.9, 1};

struct TicketTier {
  std::string_view key, label;
  double price, demand, fans;
  std::string_view note;
};
const std::vector<TicketTier>& TICKET_TIERS();
const TicketTier* ticketTier(std::string_view key);

double prizeFor(int position);
inline constexpr int PRIZE_SHARES[] = {0, 25, 50};
double goodwillFrom(double shared);

struct SponsorSlot {
  std::string_view key, label;
  double base;
};
const std::vector<SponsorSlot>& SPONSOR_SLOTS();
struct DealType {
  std::string_view key, label, note;
};
const std::vector<DealType>& DEAL_TYPES();

struct CoachArea {
  std::string_view key, label;
  std::vector<BatK> bat;
  std::vector<BowlK> bowl;
  std::vector<FieldK> field;
};
const std::vector<CoachArea>& COACH_AREAS();
const CoachArea* coachArea(std::string_view key);
struct CoachTier {
  std::string_view label;
  double cost, mult;
  int lift;
};
const CoachTier* coachTier(int tier);
struct StaffRole {
  std::string_view key, label, area;  // area "" = the bowling coach (pace or spin)
};
const std::vector<StaffRole>& STAFF_ROLES();
struct StaffTier {
  std::string_view label;
  double cost, mult;
};
const std::vector<StaffTier>& STAFF_TIERS();
int staffOf(const Staff& s, std::string_view key);
void setStaffOf(Staff& s, std::string_view key, int tier);

std::vector<std::string> areasFor(const Player& p);

Club newClub(const TeamId& teamId);
Club& ensureClub(World& world, const TeamId& teamId);
Club clubOf(const World& world, const TeamId& teamId);

double capacity(const Club& club, const TeamId& teamId);
double capacity(const Club& club, const TeamId& teamId, const Levels& levels);
int planYear(const World& world, const Club& club);
void transferMoney(World& world, const TeamId& teamId, double amount, const std::string& text);

std::vector<Coach*> coachesIn(Club& club, int year);
std::vector<const Coach*> coachesIn(const Club& club, int year);
int coachSlots(const Club& club);
int coachSlots(const Levels& levels);
double staffCost(const Staff& staff);
double upkeep(const Levels& levels);
Levels levelsAfterBuild(const Club& club);
double committed(const World& world, const Club& club);
double available(const World& world, const Club& club);

bool orderUpgrade(World& world, const TeamId& teamId, const std::string& key);
bool setTicketTier(World& world, const TeamId& teamId, const std::string& tier);
bool setHomePitch(World& world, const TeamId& teamId, std::optional<Pitch> pitch);
bool setPrizeShare(World& world, const TeamId& teamId, int share);
bool setStaff(World& world, const TeamId& teamId, const std::string& role, int tier);
bool assignCoach(World& world, const TeamId& teamId, const PlayerId& playerId, const std::string& area, int tier);
bool removeCoach(World& world, const TeamId& teamId, const PlayerId& playerId);

bool dealCovers(const Sponsor& deal, int year);
const Sponsor* slotDeal(const Club& club, std::string_view slot, int year);
// Three offers for each slot that's free next season: slot -> offers
OrderedMap<std::string, std::vector<Sponsor>> sponsorOffers(const World& world, const TeamId& teamId);
bool signSponsor(World& world, const TeamId& teamId, const std::string& slot, size_t index);

Club& activateClub(World& world, const TeamId& teamId);
void applyHomePitch(Season& season, const TeamId& teamId, const Club* club);

struct GateTake {
  double att, cap, tickets, extras, hospitality, revenue;
};
struct GateFixture {
  int no = 0;
  std::optional<TeamId> away;
  Stage stage = Stage::league;
};
GateTake gateFor(const World& world, const TeamId& teamId, const GateFixture& fixture, double form = 0.5);
std::optional<GateTake> recordGate(World& world, const TeamId& teamId, const GateFixture& fixture, double form);
std::optional<ClubSeasonSummary> closeClubSeason(World& world, const TeamId& teamId, int position);

std::vector<CoachingRow> applyCoaching(World& world, const TeamId& teamId, int year);
void clubOffseason(World& world, OffseasonReport& report);
double academyQuality(const World& world, const TeamId& teamId);
int academyScouts(const World& world, const std::optional<TeamId>& teamId);
double renewalPrice(const World& world, const TeamId& teamId, const Player& p, int years);
double seasonForm(const Season& season, const TeamId& teamId);
GateTake projectGate(const World& world, const TeamId& teamId, const std::string& tier);

}  // namespace t20
