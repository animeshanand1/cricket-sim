#include "ui/view_data.hpp"

#include <imgui.h>

#include <cmath>
#include <unordered_map>

#include "engine/ageing.hpp"
#include "engine/fitness.hpp"
#include "engine/league.hpp"
#include "engine/world.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {
const std::unordered_map<std::string, TeamInfo>& teamIndex() {
  static const std::unordered_map<std::string, TeamInfo> idx = [] {
    std::unordered_map<std::string, TeamInfo> m;
    for (const auto& t : TEAMS()) m.emplace(t.id, t);
    return m;
  }();
  return idx;
}
}  // namespace

TeamMeta teamMetaOf(const TeamId& id) {
  TeamMeta tm;
  tm.id = id;
  tm.shortName = id;
  const auto& idx = teamIndex();
  if (auto it = idx.find(id); it != idx.end()) {
    tm.name = it->second.name;
    tm.color = it->second.color;
  } else {
    tm.name = id.empty() ? "?" : id;
    tm.color = "#4a6a8a";
  }
  return tm;
}

const std::string& teamColorOf(const TeamId& id) {
  static const std::string grey = "#4a6a8a";
  const auto& idx = teamIndex();
  const auto it = idx.find(id);
  return it == idx.end() ? grey : it->second.color;
}

std::string teamNameOf(const TeamId& id) { return teamMetaOf(id).name; }

PlayerMeta playerMetaOf(const Player& p) {
  PlayerMeta m;
  m.id = p.id;
  m.name = p.name;
  m.role = p.role;
  m.teamId = p.team.value_or("");
  m.rating = headlineRating(p);
  m.batSkill = static_cast<int>(std::lround(p.bat));
  m.bowlSkill = static_cast<int>(std::lround(p.bowl));
  m.fitness = static_cast<float>(fitnessOf(p));
  m.age = p.age;
  m.overseas = p.overseas;
  m.batStyle = handText(p);
  if (p.bowlType) m.bowlStyle = std::string(bowlTypeDef(*p.bowlType).shortName);
  if (p.injury && p.injury->matches > 0) m.injury = PlayerMeta::Injury{p.injury->label, p.injury->matches};
  return m;
}

int headlineRating(const Player& p) { return static_cast<int>(std::lround(mainRating(p))); }

std::string roleText(Role r) { return std::string(roleLabel(r)); }

std::string bowlText(const Player& p) {
  if (!p.bowlType) return "–";
  return std::string(bowlTypeDef(*p.bowlType).label);
}

std::string handText(const Player& p) { return p.hand == Hand::L ? "LHB" : "RHB"; }

int fitnessPct(const Player& p) { return static_cast<int>(std::lround(fitnessOf(p))); }

void teamInline(const TeamId& id, bool showName, float size) {
  const TeamMeta tm = teamMetaOf(id);
  const float y = ImGui::GetCursorPosY();
  teamBadge(tm, size);
  if (showName) {
    ImGui::SameLine(0, 6);
    ImGui::SetCursorPosY(y + (size * 1.1f - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextUnformatted(id.c_str());
  }
}

void hint(const std::string& text) {
  FontScope fs(FontRole::Hint);
  ImGui::PushTextWrapPos(0);
  ImGui::TextColored(vec(currentTheme().muted), "%s", text.c_str());
  ImGui::PopTextWrapPos();
}

void eyebrowText(const std::string& text) {
  FontScope fs(FontRole::Eyebrow);
  ImGui::TextColored(vec(currentTheme().muted), "%s", upper(text).c_str());
}

void sectionHeading(const std::string& text) {
  FontScope fs(FontRole::H3);
  ImGui::TextUnformatted(text.c_str());
}

}  // namespace t20::ui
