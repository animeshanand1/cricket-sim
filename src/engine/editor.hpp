#pragma once
// Player editor (editor.js): the user's changes to a player's name, age, role, batting hand,
// bowling type, special delivery and skills, and designing new players. An edit replaces the
// generated values; ids never change, so career stats stay attached.

#include <optional>
#include <string>
#include <vector>

#include "core/ordered_map.hpp"
#include "engine/types.hpp"

namespace t20 {

inline constexpr size_t NAME_MAX = 24;
inline constexpr int AGE_MIN = 16;
inline constexpr int AGE_MAX = 50;
inline constexpr double SKILL_MIN = 1;
inline constexpr double SKILL_MAX = 99;

// What the form holds. Identity fields are the JS keys as strings: role/hand missing = keep;
// bowlType/special missing = keep, "" = none. Skills: NaN or missing = not a valid number.
struct EditDraft {
  std::string name;
  double age = kNaN;
  std::optional<std::string> role, hand, bowlType, special;
  Skills skills;
  bool overseas = false;  // new players only
  bool captain = false;   // handled by the club captain actions, not the edit
};

// A tidied, valid edit (what the world stores)
struct Edit {
  std::string name;
  int age = 0;
  Role role = Role::BAT;
  Hand hand = Hand::R;
  std::optional<BowlType> bowlType;
  std::optional<SpecialKey> special;
  Skills skills;
  bool overseas = false;
};

struct EditResult {
  bool ok = false;
  OrderedMap<std::string, std::string> errors;  // field ("name", "age", "bat.defence"...) -> message
  Edit edit;
};

// Bowling skills for a bowler switched between pace and spin: what carries over is kept
// (accuracy, variations) and the rest starts at his current overall
SkillSet<BowlK> convertBowling(const SkillSet<BowlK>& bowl, std::optional<BowlType> fromType,
                               std::optional<BowlType> toType);

EditResult validateEdit(const Player& p, const EditDraft& draft);
// The player with an edit applied, headline ratings recomputed; devBase = the edited skills
Player applyEdit(const Player& p, const Edit& edit);
// Write an edit into a stored (world) player, keeping the original the first time. The caller
// recomputes the headline ratings.
void writeEdit(Player& p, const Edit& edit, int year);
// Back to how he was before the first edit (a year older for every season since)
bool undoEdit(Player& p, int year);

// A blank player of this role to start the "new player" form from (same numbers every time).
// bowlType: unset = the role's usual type; set to nullopt = does not bowl.
Player newPlayerTemplate(Role role = Role::BAT, std::optional<std::optional<BowlType>> bowlType = std::nullopt,
                         Hand hand = Hand::R, int age = 22);
// Tidy a "new player" draft, or return the problems. `taken`: names already in use.
EditResult validateNewPlayer(const EditDraft& draft, const std::vector<std::string>& taken = {});

// The form's starting values for a player (EditDraft with every skill)
EditDraft draftFrom(const Player& p);

std::optional<Role> parseRole(const std::string& s);
std::optional<BowlType> parseBowlType(const std::string& s);
std::optional<SpecialKey> parseSpecial(const std::string& s);

}  // namespace t20
