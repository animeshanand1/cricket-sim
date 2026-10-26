#pragma once
// Detailed player skills (skills.js). Each headline rating (batting, bowling, fielding) is a
// weighted mix of the sub-skills, and the match engine reads the sub-skills directly.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/types.hpp"

namespace t20 {

template <class K>
struct SkillDef {
  K key;
  std::string_view label;
  std::string_view group;  // batting only
  double weight;           // 0 for fielding (not a weighted mix)
  std::string_view help;
};

extern const std::vector<SkillDef<BatK>> BAT_SKILLS;
extern const std::vector<SkillDef<BowlK>> PACE_SKILLS;
extern const std::vector<SkillDef<BowlK>> SPIN_SKILLS;
extern const std::vector<SkillDef<FieldK>> FIELD_SKILLS;

bool isPaceType(std::optional<BowlType> t);
// A bowling style named after the skill that stands out (editor: moving between pace and spin)
std::optional<std::string> bowlStyleFor(const SkillSet<BowlK>& bowl, std::optional<BowlType> type);

double battingOverall(const SkillSet<BatK>& bat);
double bowlingOverall(const SkillSet<BowlK>& bowl, std::optional<BowlType> type);
double fieldingOverall(const SkillSet<FieldK>& f, bool isKeeper);

// Pace score 0-100 to an approximate speed gun reading (null when he doesn't bowl)
std::optional<double> speedKph(const Player& p);

// The input to deriveSkills: a roster row turned into ratings
struct RawPlayer {
  PlayerId id;
  std::string name;
  Role role;
  Hand hand;
  int age;
  double bat, power, bowl;
  std::optional<BowlType> bowlType;
  double field;
  double awareness;
  std::string_view batHint;   // style name or empty
  std::string_view bowlHint;  // style name or empty
};

// Generation, deterministic per player
Player deriveSkills(const RawPlayer& p, int32_t seed);

}  // namespace t20
