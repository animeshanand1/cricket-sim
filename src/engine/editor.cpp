#include "engine/editor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/str.hpp"
#include "engine/development.hpp"
#include "engine/skills.hpp"
#include "engine/special.hpp"

namespace t20 {

std::optional<Role> parseRole(const std::string& s) {
  for (size_t i = 0; i < std::size(kRoleNames); ++i)
    if (kRoleNames[i] == s) return static_cast<Role>(i);
  return std::nullopt;
}
std::optional<BowlType> parseBowlType(const std::string& s) {
  for (size_t i = 0; i < std::size(kBowlTypeNames); ++i)
    if (kBowlTypeNames[i] == s) return static_cast<BowlType>(i);
  return std::nullopt;
}
std::optional<SpecialKey> parseSpecial(const std::string& s) {
  for (size_t i = 0; i < std::size(kSpecialKeyNames); ++i)
    if (kSpecialKeyNames[i] == s) return static_cast<SpecialKey>(i);
  return std::nullopt;
}

namespace {

// String(draft.name).replace(/\s+/g, ' ').trim()
std::string tidyName(const std::string& s) {
  std::string out;
  bool space = false;
  for (unsigned char ch : s) {
    if (std::isspace(ch)) {
      space = true;
      continue;
    }
    if (space && !out.empty()) out += ' ';
    space = false;
    out += static_cast<char>(ch);
  }
  return out;
}
// Length in UTF-16 code units, like JS string.length (every engine name is in the BMP)
size_t jsLength(const std::string& s) {
  size_t n = 0;
  for (unsigned char ch : s)
    if ((ch & 0xC0) != 0x80) ++n;
  return n;
}
std::string lowerAscii(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

struct Identity {
  std::string role, hand;
  std::optional<std::string> bowlType, special;  // nullopt = none
};

// What kind of player the draft describes; anything the draft leaves out stays as it is
Identity identityOf(const Player& p, const EditDraft& d) {
  Identity w;
  w.role = d.role ? *d.role : std::string(name(p.role));
  w.hand = d.hand ? *d.hand : std::string(name(p.hand));
  if (d.bowlType)
    w.bowlType = d.bowlType->empty() ? std::nullopt : std::optional<std::string>(*d.bowlType);
  else if (p.bowlType)
    w.bowlType = std::string(name(*p.bowlType));
  if (d.special)
    w.special = d.special->empty() ? std::nullopt : std::optional<std::string>(*d.special);
  else if (p.special)
    w.special = std::string(name(*p.special));
  return w;
}

template <class K>
void mergeInto(SkillSet<K>& dst, const SkillSet<K>& src) {
  for (size_t i = 0; i < SkillSet<K>::N; ++i) {
    const K k = static_cast<K>(i);
    if (src.contains(k)) dst.set(k, src[k]);
  }
}

template <class K>
void checkGroup(const std::vector<SkillDef<K>>& defs, const char* area, const SkillSet<K>& draft, SkillSet<K>& out,
                OrderedMap<std::string, std::string>& errors) {
  for (const auto& d : defs) {
    const double v = draft[d.key];
    if (!std::isfinite(v) || v < SKILL_MIN || v > SKILL_MAX)
      errors.set(cat(area, '.', name(d.key)), cat(SKILL_MIN, " to ", SKILL_MAX));
    else
      out.set(d.key, std::max(SKILL_MIN, std::min(SKILL_MAX, js_round(v))));
  }
}

struct Starter {
  double bat, power, bowl;
  std::optional<BowlType> bowlType;
  double field;
};
Starter starterFor(Role r) {
  switch (r) {
    case Role::WK: return {58, 55, 5, std::nullopt, 66};
    case Role::AR: return {56, 58, 58, BowlType::fastmed, 62};
    case Role::BOWL: return {22, 30, 64, BowlType::fast, 58};
    case Role::BAT:
    default: return {62, 60, 20, BowlType::medium, 62};
  }
}

}  // namespace

SkillSet<BowlK> convertBowling(const SkillSet<BowlK>& bowl, std::optional<BowlType> fromType,
                               std::optional<BowlType> toType) {
  if (isPaceType(fromType) == isPaceType(toType)) return bowl;
  const double level = bowlingOverall(bowl, fromType);
  SkillSet<BowlK> out;
  for (const auto& d : (isPaceType(toType) ? PACE_SKILLS : SPIN_SKILLS)) out.set(d.key, bowl.get_or(d.key, level));
  return out;
}

EditResult validateEdit(const Player& p, const EditDraft& draft) {
  EditResult res;
  auto& errors = res.errors;
  const std::string nm = tidyName(draft.name);
  if (nm.empty())
    errors.set("name", "Enter a name.");
  else if (jsLength(nm) > NAME_MAX)
    errors.set("name", cat("Keep it to ", static_cast<int>(NAME_MAX), " characters."));
  const double age = draft.age;
  if (!std::isfinite(age) || age < AGE_MIN || age > AGE_MAX) errors.set("age", cat("Age must be ", AGE_MIN, " to ", AGE_MAX, "."));
  Identity who = identityOf(p, draft);
  const auto role = parseRole(who.role);
  if (!role) errors.set("role", "Choose a role.");
  const bool handOk = who.hand == "R" || who.hand == "L";
  if (!handOk) errors.set("hand", "Choose a batting hand.");
  std::optional<BowlType> type;
  if (who.bowlType) {
    type = parseBowlType(*who.bowlType);
    if (!type) {
      errors.set("bowlType", "Choose a bowling type.");
      type = p.bowlType;
    }
  }
  std::optional<SpecialKey> special;
  if (who.special) {
    special = parseSpecial(*who.special);
    const auto& allowed = specialsFor(type);
    if (!special || std::find(allowed.begin(), allowed.end(), *special) == allowed.end()) special.reset();
  }
  const bool switched = isPaceType(type) != isPaceType(p.bowlType);
  Skills skills;
  skills.bat = p.skills.bat;
  checkGroup(BAT_SKILLS, "bat", draft.skills.bat, skills.bat, errors);
  if (!switched) skills.bowl = p.skills.bowl;
  checkGroup(isPaceType(type) ? PACE_SKILLS : SPIN_SKILLS, "bowl", draft.skills.bowl, skills.bowl, errors);
  skills.field = p.skills.field;
  checkGroup(FIELD_SKILLS, "field", draft.skills.field, skills.field, errors);
  if (!errors.empty()) return res;
  res.ok = true;
  res.edit.name = nm;
  res.edit.age = static_cast<int>(js_round(age));
  res.edit.role = *role;
  res.edit.hand = who.hand == "L" ? Hand::L : Hand::R;
  res.edit.bowlType = type;
  res.edit.special = special;
  res.edit.skills = skills;
  return res;
}

Player applyEdit(const Player& p, const Edit& edit) {
  const bool switched = isPaceType(edit.bowlType) != isPaceType(p.bowlType);
  Skills skills;
  skills.bat = p.skills.bat;
  mergeInto(skills.bat, edit.skills.bat);
  skills.bowl = switched ? convertBowling(p.skills.bowl, p.bowlType, edit.bowlType) : p.skills.bowl;
  mergeInto(skills.bowl, edit.skills.bowl);
  skills.field = p.skills.field;
  mergeInto(skills.field, edit.skills.field);
  Player q = p;
  q.name = edit.name;
  q.age = edit.age;
  q.role = edit.role;
  q.hand = edit.hand;
  q.bowlType = edit.bowlType;
  q.special = edit.special;
  if (switched) q.bowlStyle = bowlStyleFor(skills.bowl, edit.bowlType);
  q.skills = skills;
  recomputeRatings(q);
  q.edited = true;
  q.devBase = skills;
  return q;
}

void writeEdit(Player& p, const Edit& edit, int year) {
  if (!p.preEdit) {
    PreEdit pe;
    pe.name = p.name;
    pe.age = p.age;
    pe.skills = p.skills;
    pe.year = year;
    p.preEdit = pe;
  }
  if (!p.preEdit->hasKept) {  // also fills in saves from before these could be edited
    p.preEdit->hasKept = true;
    p.preEdit->role = p.role;
    p.preEdit->hand = p.hand;
    p.preEdit->bowlType = p.bowlType;
    p.preEdit->bowlStyle = p.bowlStyle;
    p.preEdit->special = p.special;
  }
  const Player q = applyEdit(p, edit);
  p.name = q.name;
  p.age = q.age;
  p.role = q.role;
  p.hand = q.hand;
  p.bowlType = q.bowlType;
  p.bowlStyle = q.bowlStyle;
  p.special = q.special;
  p.skills = q.skills;
  p.skillFrac = SkillFrac{};
  p.edited = true;
}

bool undoEdit(Player& p, int year) {
  if (!p.preEdit) return false;
  const PreEdit o = *p.preEdit;
  p.name = o.name;
  p.age = o.age + (year - o.year);
  if (o.hasKept) {
    p.role = o.role;
    p.hand = o.hand;
    p.bowlType = o.bowlType;
    p.bowlStyle = o.bowlStyle;
    p.special = o.special;
  }
  p.skills = o.skills;
  p.preEdit.reset();
  p.edited = false;
  return true;
}

Player newPlayerTemplate(Role role, std::optional<std::optional<BowlType>> bowlType, Hand hand, int age) {
  const Starter s = starterFor(role);
  RawPlayer raw;
  raw.id = "NEW";
  raw.name = "";
  raw.role = role;
  raw.hand = hand;
  raw.age = age;
  raw.bat = s.bat;
  raw.power = s.power;
  raw.bowl = s.bowl;
  raw.bowlType = bowlType ? *bowlType : s.bowlType;
  raw.field = s.field;
  raw.awareness = 50;
  return deriveSkills(raw, 0);
}

EditResult validateNewPlayer(const EditDraft& draft, const std::vector<std::string>& taken) {
  const auto role = draft.role ? parseRole(*draft.role) : std::nullopt;
  std::optional<std::optional<BowlType>> type;  // unset: role default
  if (!draft.bowlType || draft.bowlType->empty())
    type = std::optional<BowlType>();  // missing or "" -> does not bowl
  else if (auto t = parseBowlType(*draft.bowlType))
    type = t;
  const Player tmpl = newPlayerTemplate(role.value_or(Role::BAT), type);
  EditResult res = validateEdit(tmpl, draft);
  if (!role) res.errors.set("role", "Choose a role.");
  const std::string key = lowerAscii(tidyName(draft.name));
  if (!res.errors.contains("name"))
    for (const auto& n : taken)
      if (lowerAscii(tidyName(n)) == key) {
        res.errors.set("name", "Another player already has this name.");
        break;
      }
  if (!res.errors.empty()) {
    res.ok = false;
    return res;
  }
  res.edit.overseas = draft.overseas;
  return res;
}

EditDraft draftFrom(const Player& p) {
  EditDraft d;
  d.name = p.name;
  d.age = p.age;
  d.role = std::string(name(p.role));
  d.hand = std::string(name(p.hand));
  d.bowlType = p.bowlType ? std::string(name(*p.bowlType)) : std::string();
  d.special = p.special ? std::string(name(*p.special)) : std::string();
  d.skills = p.skills;
  d.overseas = p.overseas;
  return d;
}

}  // namespace t20
