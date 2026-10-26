#pragma once
// The living league (world.js): every player ever, the squads, the off-season, the youth draft,
// the auction, the board and the user's club business (club.js). Field names follow the JS so
// the JSON save is the same as the browser's.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/match_types.hpp"
#include "engine/types.hpp"

namespace t20 {

struct WorldTeam {
  std::vector<PlayerId> squad;
  std::optional<int> rtm;             // Right to Match cards left (mega years)
  std::optional<PlayerId> captainId;  // club captain; counts only while he is in the squad
};

// ---- The board (board.js)
struct Target {
  std::string key, label;
  int need = 0;
  int rank = 0, season = 0;
};
struct BoardEntry {  // a season review, or an event ("moved" / "sacked")
  std::optional<int> season;
  std::optional<TeamId> team;
  std::optional<std::string> target;
  std::optional<int> position;
  std::optional<std::string> verdict;
  std::optional<double> delta, confidence;
  std::optional<bool> inRed;
  std::optional<std::string> event;
  std::optional<TeamId> from, to;
};
struct Sacked {
  int season = 0;
  TeamId team;
  std::vector<TeamId> offers;
  std::optional<std::vector<TeamId>> order;
};
struct Manager {
  std::optional<TeamId> team;
  double confidence = 60;
  std::optional<Target> target;
  std::vector<BoardEntry> history;
  std::optional<Sacked> sacked;
};

// ---- The off-season
struct Offseason {
  int year = 0;
  std::string stage;  // contracts | retention | draft | auction
  bool mega = false;
  std::vector<TeamId> order;
  std::optional<TeamId> userTeam;
  std::optional<std::string> then;  // season | players
  std::vector<PlayerId> retain;
};

struct DraftPick {
  TeamId team;
  std::optional<PlayerId> id;
  int no = 0;
  bool skipped = false;
};
struct Draft {
  int year = 0;
  std::vector<PlayerId> pool;
  std::vector<TeamId> order;
  std::vector<DraftPick> made;
  OrderedMap<PlayerId, bool> scouted;
  int scoutsLeft = 0;
  std::optional<TeamId> userTeam;
  std::optional<std::string> then;
};

// ---- The auction (auction.js)
struct Bid {
  TeamId team;
  double price = 0;
};
struct Lot {
  PlayerId id;
  double base = 0;
  std::optional<double> price;
  std::optional<TeamId> leader;
  std::vector<Bid> bids;
  OrderedMap<TeamId, bool> out;
  std::string stage = "bidding";  // bidding | rtm-invoke | rtm-raise | rtm-match
  OrderedMap<TeamId, double> vals;
  std::optional<TeamId> rtmTeam;
  std::optional<double> userMax;
};
struct Sale {
  PlayerId id;
  TeamId team;
  double price = 0;
  bool rtm = false;
  int no = 0;
  std::string set;
  std::optional<TeamId> former;
};
struct Signing {  // signed at base price after the auction (or by fillSquads, without a price)
  PlayerId id;
  TeamId team;
  std::optional<double> price;
};
struct Auction {
  int year = 0;
  std::string type;  // mini | mega
  std::vector<PlayerId> lots;
  OrderedMap<PlayerId, std::string> setOf;
  int idx = 0;
  std::optional<Lot> lot;
  std::vector<Sale> results;
  std::vector<PlayerId> unsold;
  std::optional<TeamId> userTeam;
  bool userAuto = false;
  std::vector<std::string> log;
};
struct AuctionResult {
  std::string type;
  std::vector<Sale> sold;
  std::vector<PlayerId> unsold;
  std::vector<Signing> signed_;  // JSON key "signed"
};

// ---- The off-season report (world.js runOffseason, club.js clubOffseason)
struct Rating3 {
  double bat = 0, bowl = 0, field = 0;
};
struct AgedRow {
  PlayerId id;
  TeamId team;
  int age = 0;
  Rating3 before, after;
};
struct RetiredRow {
  PlayerId id;
  TeamId team;
  int age = 0;
  double main = 0;
};
struct MoveRow {  // released, academy
  PlayerId id;
  TeamId team;
};
struct RenewedRow {
  PlayerId id;
  TeamId team;
  double cost = 0;
  int years = 0;
};
struct RetainedRow {
  PlayerId id;
  TeamId team;
  double cost = 0;
};
struct DraftedRow {
  PlayerId id;
  TeamId team;
  int no = 0;
};
struct CoachingRow {
  PlayerId id;
  double before = 0;
  std::optional<double> potBefore;
  double gain = 0;
  std::vector<std::string> notes;
  double after = 0;
  std::optional<double> potAfter;
  std::optional<TeamId> team;  // in the league report
};
struct OffseasonReport {
  int year = 0;
  bool mega = false;
  std::vector<AgedRow> aged;
  std::vector<RetiredRow> retired;
  std::vector<MoveRow> released;
  std::vector<RenewedRow> renewed;
  std::vector<RetainedRow> retained;
  std::vector<MoveRow> academy;
  std::vector<DraftedRow> drafted;
  std::vector<Signing> signed_;
  std::optional<AuctionResult> auction;
  std::optional<std::vector<CoachingRow>> coaching;
};

// ---- Transfers and injuries (trades.js, fitness.js)
struct Transfer {
  int year = 0;
  std::string kind;  // trade | replacement
  TeamId team;
  std::optional<TeamId> with;
  std::vector<PlayerId> got;
  std::optional<std::vector<PlayerId>> gave;
  std::optional<double> cash;
  std::optional<PlayerId> for_;  // JSON key "for"
  std::string text;
};
struct InjuryNews {
  PlayerId id;
  TeamId team;
  std::string label;
  int matches = 0;
};

// ---- The club as a business (club.js)
struct Building {
  int level = 0, ready = 0;
  double cost = 0;
};
struct ClubPlan {
  std::string tier = "standard";
  std::optional<Pitch> pitch;
  int prizeShare = 0;
};
struct Staff {
  int bat = 0, bowl = 0, field = 0;
};
struct SponsorBonus {
  double playoffs = 0, title = 0;
};
struct Sponsor {
  std::string slot, brand, type;
  double fee = 0;
  std::optional<SponsorBonus> bonus;
  int years = 0;
  int from = 0;
};
struct Coach {
  PlayerId playerId;
  std::string area;
  int tier = 1;
  int from = 0;
  std::optional<int> ends, paid;
};
struct Goodwill {
  int year = 0;
  double pct = 0;
};
struct Books {
  OrderedMap<std::string, double> income, spend;
};
struct Gate {
  int year = 0, no = 0;
  std::optional<TeamId> opp;
  Stage stage = Stage::league;
  double att = 0, cap = 0, revenue = 0;
};
struct ClubSeasonSummary {
  int year = 0, position = 0;
  double prize = 0, shared = 0;
  double popBefore = 0, popAfter = 0;  // JSON: popularity { before, after }
  double income = 0, spend = 0, net = 0, bank = 0;
  int homeGames = 0;
  double fill = 0;
  bool inRed = false;
};
struct LastCoaching {
  int year = 0;
  std::vector<CoachingRow> rows;
};
struct ClubNote {
  int year = 0;
  std::string text;
};
// Facility levels by key, in FACILITY_KEYS order
struct Levels {
  OrderedMap<std::string, int> v;
  int operator[](const std::string& k) const {
    const int* x = v.find(k);
    return x ? *x : 0;
  }
};
struct Club {
  double bank = 3000;
  double popularity = 65;
  Levels levels;
  OrderedMap<std::string, Building> building;
  ClubPlan plan, active;
  Staff staffPlan, staff;
  std::optional<int> staffPaid;
  std::vector<Sponsor> sponsors;
  std::vector<Coach> coaches;
  std::optional<Goodwill> goodwill;
  bool inSeason = false;
  std::optional<int> activatedYear, closedYear;
  std::map<int, Books> books;  // by year (JS integer keys iterate in ascending order)
  std::vector<Gate> gates;
  std::vector<ClubSeasonSummary> history;
  std::optional<LastCoaching> lastCoaching;
  std::vector<ClubNote> log;
};

struct World {
  int version = 3;
  int year = 0;
  int nextId = 1;
  OrderedMap<PlayerId, Player> players;
  OrderedMap<TeamId, WorldTeam> teams;
  std::optional<Draft> draft;
  std::optional<Auction> auction;
  std::optional<Offseason> offseason;
  std::optional<OffseasonReport> lastOffseason;
  Manager manager;
  OrderedMap<TeamId, Club> clubs;
  std::vector<Transfer> transfers;       // newest first, at most 40
  std::vector<InjuryNews> lastInjuries;  // newest first, at most 20
};

}  // namespace t20
