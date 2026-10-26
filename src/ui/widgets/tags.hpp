#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>

namespace t20::ui {

// Fitness and injury tag:
// Injured: red pill "Injured · Hamstring (3)"
// Fitness < 90: mini 40x5 bar + label (Fit/Tired/Exhausted + value)
// Fitness >= 90: nothing
void fitnessTag(const PlayerMeta& player);

// Career phase pill: Rising (green), Prime (blue), Experienced (gold), Veteran (orange), Twilight (red)
void phaseTag(int age);

// Overseas pill: "OS" blue outline
void osTag();

// Qualified player pill: "Q" green fill
void qTag();

// Impact Player tag: "IP" pill
void ipTag();

// Small grey attribute pill (batting style, kph, special delivery, "Edited" in gold)
void styleChip(const std::string& text, bool isGold = false);

enum class TagTone {
  Default,
  Hot,    // gold
  Strong, // green
  Weak,   // red
  Muted
};

// Generic pill tag with tone styling
void tag(const std::string& text, TagTone tone = TagTone::Default);

// Form pips (recent matches): 20px squares (W green, L grey, N dashed)
void formPips(const std::vector<char>& results);

// KPI hero number tile: uppercase eyebrow + big condensed italic number + tone
void kpiTile(const std::string& label, const std::string& value, TagTone tone = TagTone::Default);

// 32-px match ball pip on dark board with pop-in animation
void ballPip(const BallPipData& ball);
// The same pip drawn at a centre point (used inside the scoreboard)
void drawBallPip(void* drawList, const BallPipData& ball, float cx, float cy, float size = 32.0f);

} // namespace t20::ui
