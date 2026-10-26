#pragma once
// Engine-wide types: enums with their JSON spellings, skill sets and the Player.
// Field names follow the JS (see docs/cpp-migration/04-data-model.md).

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/jsmath.hpp"

namespace t20 {

using PlayerId = std::string;
using TeamId = std::string;

#define T20_ENUM_NAMES(Enum, ...)                                                   \
  inline constexpr std::string_view k##Enum##Names[] = {__VA_ARGS__};               \
  inline std::string_view name(Enum v) { return k##Enum##Names[static_cast<size_t>(v)]; }

enum class Role : uint8_t { WK, BAT, AR, BOWL };
T20_ENUM_NAMES(Role, "WK", "BAT", "AR", "BOWL")

enum class Hand : uint8_t { R, L };
T20_ENUM_NAMES(Hand, "R", "L")

enum class BowlType : uint8_t { fast, fastmed, medium, lfast, lfastmed, lmedium, off, leg, sla, lws };
T20_ENUM_NAMES(BowlType, "fast", "fastmed", "medium", "lfast", "lfastmed", "lmedium", "off", "leg", "sla", "lws")

enum class Pitch : uint8_t { balanced, flat, green, dusty, slow };
T20_ENUM_NAMES(Pitch, "balanced", "flat", "green", "dusty", "slow")

enum class Captain : uint8_t { balanced, aggressive, cautious, shrewd, spin, pace };
T20_ENUM_NAMES(Captain, "balanced", "aggressive", "cautious", "shrewd", "spin", "pace")

enum class Phase : uint8_t { pp, mid, death };
T20_ENUM_NAMES(Phase, "pp", "mid", "death")

// Ways out (DismissalType)
enum class Out : uint8_t { bowled, lbw, caught, caughtKeeper, caughtBowled, stumped, runout };
T20_ENUM_NAMES(Out, "bowled", "lbw", "caught", "caughtKeeper", "caughtBowled", "stumped", "runout")

// Bowling plans (pace and spin share stock and wide)
enum class Plan : uint8_t { stock, yorkers, bouncers, wide, slower, attack, flight, darts, mystery };
T20_ENUM_NAMES(Plan, "stock", "yorkers", "bouncers", "wide", "slower", "attack", "flight", "darts", "mystery")

enum class SpecialKey : uint8_t {
  yorker, outswinger, inswinger, bouncer, seamer, knuckle, cutter, googly, doosra, carrom, armball, flipper, ripper, drifter
};
T20_ENUM_NAMES(SpecialKey, "yorker", "outswinger", "inswinger", "bouncer", "seamer", "knuckle", "cutter", "googly",
               "doosra", "carrom", "armball", "flipper", "ripper", "drifter")

enum class Sky : uint8_t { clear, overcast, humid, hot };
T20_ENUM_NAMES(Sky, "clear", "overcast", "humid", "hot")

enum class RainWhen : uint8_t { pre, first, second };
T20_ENUM_NAMES(RainWhen, "pre", "first", "second")

// ---------------------------------------------------------------------------
// Skills
// ---------------------------------------------------------------------------

enum class BatK : uint8_t { defence, vsPace, vsSpin, grounded, lofted, rotation, running, awareness, temperament, COUNT };
T20_ENUM_NAMES(BatK, "defence", "vsPace", "vsSpin", "grounded", "lofted", "rotation", "running", "awareness", "temperament")

enum class BowlK : uint8_t { pace, accuracy, swing, seam, variations, death, turn, flight, COUNT };
T20_ENUM_NAMES(BowlK, "pace", "accuracy", "swing", "seam", "variations", "death", "turn", "flight")

enum class FieldK : uint8_t { catching, ground, throwing, keeping, COUNT };
T20_ENUM_NAMES(FieldK, "catching", "ground", "throwing", "keeping")

// A fixed-size skill set with a presence mask. Reading a missing skill gives NaN, exactly like
// reading `undefined` in JS arithmetic and comparisons (NaN >= 75 is false). Use get_or() where
// the JS writes `skill ?? fallback`.
template <class K>
struct SkillSet {
  using key_type = K;
  static constexpr size_t N = static_cast<size_t>(K::COUNT);
  std::array<double, N> v{};
  std::bitset<N> has;

  double operator[](K k) const { return has.test(idx(k)) ? v[idx(k)] : kNaN; }
  double get_or(K k, double fallback) const { return has.test(idx(k)) ? v[idx(k)] : fallback; }
  bool contains(K k) const { return has.test(idx(k)); }
  void set(K k, double x) {
    v[idx(k)] = x;
    has.set(idx(k));
  }
  bool operator==(const SkillSet&) const = default;

 private:
  static constexpr size_t idx(K k) { return static_cast<size_t>(k); }
};

struct Skills {
  SkillSet<BatK> bat;
  SkillSet<BowlK> bowl;
  SkillSet<FieldK> field;
  bool operator==(const Skills&) const = default;
};

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

// Where a player came from (world.js makePlayer kinds, the roster, the draft, the editor)
enum class Origin : uint8_t { roster, prospect, journeyman, entrant, academy, draft, custom };
T20_ENUM_NAMES(Origin, "roster", "prospect", "journeyman", "entrant", "academy", "draft", "custom")

enum class ContractVia : uint8_t { retained, auction, rtm, renewed, draft, academy, base, free, trade, replacement, custom };
T20_ENUM_NAMES(ContractVia, "retained", "auction", "rtm", "renewed", "draft", "academy", "base", "free", "trade",
               "replacement", "custom")

struct Injury {
  std::string type, label;
  int matches = 0;
  int year = 0;
  std::optional<PlayerId> replacedBy;  // the free agent signed as cover
};

// All money is in lakh (finance.js)
struct Contract {
  double cost = 0, salary = 0;
  int years = 0;
  ContractVia via = ContractVia::retained;
  int signed_ = 0;  // JSON key "signed"
};

struct Retired {
  int year = 0, age = 0;
  std::optional<TeamId> team;
  bool quiet = false;  // drifted out of the game unsold, not a farewell
};

struct Drafted {
  int year = 0, no = 0;
  TeamId team;
};

// One point per off-season (world.js snapshot)
struct HistoryPt {
  int year = 0, age = 0;
  double bat = 0, bowl = 0, field = 0;
};

// Ageing and coaching remainders, carried to the next off-season (only the keys touched)
struct SkillFrac {
  SkillSet<BatK> bat;
  SkillSet<BowlK> bowl;
  SkillSet<FieldK> field;
};

// What the editor keeps to restore a player (editor.js writeEdit). Saves from before roles and
// types could be edited have only name, age, skills and year (hasKept = false).
struct PreEdit {
  std::string name;
  int age = 0;
  Skills skills;
  int year = 0;
  bool hasKept = false;
  Role role = Role::BAT;
  Hand hand = Hand::R;
  std::optional<BowlType> bowlType;
  std::optional<std::string> bowlStyle;
  std::optional<SpecialKey> special;
};

// Headline ratings and skills before match-by-match development (development.js applyProgress)
struct BaseRatings {
  double bat = 0, bowl = 0, field = 0;
  Skills skills;
};

struct TrainingTag {
  std::string drill, label;
};

struct Player {
  // identity
  PlayerId id;
  std::string name;
  Role role = Role::BAT;
  Hand hand = Hand::R;
  int age = 0;
  std::optional<BowlType> bowlType;
  bool overseas = false;
  // headline ratings (always the weighted mix of the skills)
  double bat = 0, bowl = 0, field = 0, power = 0, awareness = 0;
  Skills skills;
  std::string batStyle;
  std::optional<std::string> bowlStyle;
  std::optional<SpecialKey> special;  // chosen in the editor; else auto from the best skill

  // availability and form (world / match setup)
  std::optional<double> fitness;  // 0-100, missing = 100
  std::optional<Injury> injury;
  bool unavailable = false;
  std::optional<double> startConf;  // from form (+4 if trained)

  // world (the living league; see world.js)
  std::optional<double> potential;
  std::optional<TeamId> team;  // none = free agent or retired
  Origin origin = Origin::roster;
  std::optional<Retired> retired;
  std::vector<HistoryPt> history;
  std::optional<int> joined;
  std::optional<Contract> contract;
  std::optional<bool> capped;
  std::optional<SkillFrac> skillFrac;
  bool edited = false;
  std::optional<PreEdit> preEdit;
  std::optional<Drafted> drafted;
  std::optional<TeamId> formerTeam;  // set on release, cleared after the auction
  std::optional<int> unsoldYears;
  std::optional<int> coachLift;

  // transient: built for screens and matches, never saved
  std::optional<Skills> devBase;       // the skills development is measured from
  std::optional<BaseRatings> base;     // before development
  std::optional<TrainingTag> training; // this match's drill
  bool tired = false;                  // fatigue applied

  // match-only flags
  bool impactIn = false, impactOut = false;
};

struct TeamInfo {
  TeamId id;
  Captain captain = Captain::balanced;
  std::string personality, name, city, color, ground;
  Pitch pitch = Pitch::balanced;
};

struct Team : TeamInfo {
  std::optional<PlayerId> captainId;  // the club captain (world.js clubCaptainOf), if named
  std::vector<Player> squad;
};

}  // namespace t20
