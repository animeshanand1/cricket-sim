#include "ui/screens/common.hpp"

#include <imgui.h>

#include <cmath>
#include <cstdio>

#include "engine/fitness.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

void nameLink(UiContext& ctx, const Player& p) {
  if (playerLink(p.id, p.name)) ctx.openCard(p.id);
}

void nameLink(UiContext& ctx, const PlayerId& id) {
  if (const Player* p = ctx.player(id)) nameLink(ctx, *p);
  else ImGui::TextUnformatted(id.c_str());
}

void playerTags(const Player& p, const PlayerTagOpts& opts) {
  const float sp = 5.0f;
  auto next = [&] { ImGui::SameLine(0, sp); };
  if (opts.captain) {
    next();
    tag("C", TagTone::Hot);
  }
  if (opts.keeper) {
    next();
    tag("WK", TagTone::Default);
  }
  if (p.overseas) {
    next();
    osTag();
  }
  if (opts.fitness && (isInjured(p) || fitnessOf(p) < 90)) {
    next();
    fitnessTag(playerMetaOf(p));
  }
}

void playerIdentity(UiContext& ctx, const Player& p, float avatarSize, bool tags) {
  const auto& th = currentTheme();
  avatar(playerMetaOf(p), teamColorOf(p.team.value_or("")), avatarSize, true);
  ImGui::SameLine(0, 10);
  ImGui::BeginGroup();
  nameLink(ctx, p);
  if (tags) playerTags(p, {false, false, true});
  {
    FontScope fs(FontRole::Hint);
    std::string line = roleText(p.role) + " · " + handText(p) + " · " + std::to_string(p.age) + " yrs";
    if (p.bowlType) line += " · " + bowlText(p);
    ImGui::TextColored(vec(th.muted), "%s", line.c_str());
  }
  ImGui::EndGroup();
}

std::string pct(double v) { return std::to_string(static_cast<int>(std::lround(v))) + "%"; }

std::string num(double v, int dp) {
  if (std::isnan(v)) return "–";
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", dp, v);
  return buf;
}

std::string signedNum(double v, int dp) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%+.*f", dp, v);
  return buf;
}

void ratingNum(double v) {
  if (std::isnan(v)) {
    ImGui::TextColored(vec(currentTheme().muted), "–");
    return;
  }
  rating(static_cast<int>(std::lround(v)));
}

}  // namespace t20::ui
