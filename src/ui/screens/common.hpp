#pragma once
// Bits the management screens share: a player's name as a card link, his tags, a compact player
// row, and money / percentage text.

#include <optional>
#include <string>

#include "engine/types.hpp"
#include "ui/screens/ui_context.hpp"

namespace t20::ui {

// The player's name; clicking opens his card
void nameLink(UiContext& ctx, const Player& p);
void nameLink(UiContext& ctx, const PlayerId& id);

// OS / C / WK / injury / fitness tags on one line (call after SameLine)
struct PlayerTagOpts {
  bool captain = false;
  bool keeper = false;
  bool fitness = true;
};
void playerTags(const Player& p, const PlayerTagOpts& opts = {});

// avatar · name (link) · role and style line; leaves the cursor on the same line after it
void playerIdentity(UiContext& ctx, const Player& p, float avatarSize = 34.0f, bool tags = true);

std::string pct(double v);           // "64%"
std::string num(double v, int dp = 0);
std::string signedNum(double v, int dp = 0);  // "+3" / "-2"

// A coloured bat / bowl / field rating number
void ratingNum(double v);

// S06 §6 The popularity meter: the league's five most popular active players (rank, avatar, name,
// team, popularity with its last move, a bar in the team colour). Used on the match result, the
// season overview and the Players page.
void popularityMeter(UiContext& ctx, const char* id = "##popularity");

}  // namespace t20::ui
