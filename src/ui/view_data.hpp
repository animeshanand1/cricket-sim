#pragma once
// Engine data -> the widgets' view data (TeamMeta, PlayerMeta) and small text helpers the screens
// share: role and bowling labels, the headline rating, contract lines.

#include <optional>
#include <string>

#include "core/types.hpp"
#include "engine/types.hpp"

namespace t20::ui {

// A franchise by id (falls back to a grey "?" badge for an unknown id)
TeamMeta teamMetaOf(const TeamId& id);
const std::string& teamColorOf(const TeamId& id);
std::string teamNameOf(const TeamId& id);

PlayerMeta playerMetaOf(const Player& p);

// The rating that matters for his role (bat for batters, bowl for bowlers, the better for ARs)
int headlineRating(const Player& p);
std::string roleText(Role r);  // "Batter", "Wicketkeeper", ...
std::string bowlText(const Player& p);  // "Right-arm fast", "–"
std::string handText(const Player& p);  // "RHB" / "LHB"
int fitnessPct(const Player& p);

// Small badge + name, inline (tables, lists)
void teamInline(const TeamId& id, bool showName = true, float size = 18.0f);

// Muted helper line / eyebrow / section heading, used across screens
void hint(const std::string& text);
void eyebrowText(const std::string& text);
void sectionHeading(const std::string& text);

}  // namespace t20::ui
