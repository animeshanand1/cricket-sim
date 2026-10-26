#include "ui/screens/common.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/fitness.hpp"
#include "engine/popularity.hpp"
#include "ui/widgets/panel.hpp"
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

void popularityMeter(UiContext& ctx, const char* id) {
  const auto& th = currentTheme();
  const auto& career = ctx.game.state().career;
  const auto rows = topPopular(career.popularity, 5, [&ctx](const PlayerId& pid) {
    const Player* p = ctx.player(pid);
    return p && !p->retired;
  });
  beginPanel(id, "Popularity");
  if (rows.empty()) {
    ImGui::TextColored(vec(th.muted), "No games played yet. The fans pick their favourites once the cricket starts.");
    endPanel();
    return;
  }
  int rank = 1;
  for (const auto& r : rows) {
    const Player* p = ctx.player(r.id);
    if (!p) continue;
    ImGui::PushID(r.id.c_str());
    const TeamId team = p->team.value_or("");
    {
      FontScope fs(FontRole::H3);
      ImGui::TextColored(vec(th.muted), "%d", rank++);
    }
    ImGui::SameLine(0, 10);
    avatar(playerMetaOf(*p), teamColorOf(team), 30, false);
    ImGui::SameLine(0, 10);
    ImGui::BeginGroup();
    nameLink(ctx, *p);
    if (!team.empty()) {
      teamInline(team, true, 14);
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    // the value and its trend, then the bar in the team colour
    const float barW = std::min(220.0f, ImGui::GetContentRegionAvail().x * 0.45f);
    const float x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - barW;
    const std::string v = pct(r.v) + (r.d > 0 ? "  \xe2\x96\xb2 " + num(r.d, 1) : r.d < 0 ? "  \xe2\x96\xbc " + num(-r.d, 1) : "");
    ImGui::SetCursorPosX(x);
    ImGui::BeginGroup();
    {
      FontScope fs(FontRole::BodyStrong);
      ImGui::TextColored(vec(r.d > 0 ? th.green : r.d < 0 ? th.red : th.ink), "%s", v.c_str());
    }
    const ImVec2 bp = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bp, ImVec2(bp.x + barW, bp.y + 6), col(th.surface3), 3);
    dl->AddRectFilled(bp, ImVec2(bp.x + barW * static_cast<float>(std::clamp(r.v, 0.0, 100.0) / 100.0), bp.y + 6),
                      hexCol(teamColorOf(team)), 3);
    ImGui::Dummy(ImVec2(barW, 8));
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PopID();
  }
  endPanel();
}

}  // namespace t20::ui
