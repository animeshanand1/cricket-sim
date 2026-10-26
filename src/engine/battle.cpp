#include "engine/battle.hpp"

#include <algorithm>
#include <initializer_list>

#include "engine/skills.hpp"

namespace t20 {

namespace {
double avg(std::initializer_list<std::pair<double, double>> entries) {
  double s = 0, w = 0;
  for (const auto& [weight, v] : entries) {
    s += weight * v;
    w += weight;
  }
  return s / w;
}
}  // namespace

BowlerNow bowlerNow(const Player& bowler, const BallContext& ctx) {
  const auto& b = bowler.skills.bowl;
  if (isPaceType(bowler.bowlType)) {
    // Swing is king with the new ball; seam on a green top (less as it dries); yorkers at the death.
    const double swingW = ctx.over < 3 ? 0.3 : ctx.over < 6 ? 0.15 : 0.04;
    const double seamW = 0.12 + (ctx.pitch == Pitch::green ? 0.18 * (1 - 0.7 * ctx.wear) : 0);
    const double deathW = ctx.phase == Phase::death ? 0.3 : 0;
    const double varW = ctx.phase == Phase::death ? 0.15 : ctx.phase == Phase::mid ? 0.12 : 0.05;
    const double threat = avg({{0.2, b[BowlK::pace]},
                               {0.25, b[BowlK::accuracy]},
                               {swingW, b[BowlK::swing]},
                               {seamW, b[BowlK::seam]},
                               {varW, b[BowlK::variations]},
                               {deathW, b[BowlK::death]}});
    const double control = avg({{0.5, b[BowlK::accuracy]},
                                {0.2, ctx.phase == Phase::death ? b[BowlK::death] : b[BowlK::variations]},
                                {0.15, b[BowlK::pace]},
                                {0.15, b[BowlK::seam]}});
    return {threat, control, true};
  }
  // Spin: turn grows on a dry, wearing pitch; dew takes away grip and dip.
  const double grip = ctx.dew ? 0.7 : 1;
  const double turnW = 0.28 * (ctx.pitch == Pitch::dusty ? 1.4 + 0.6 * ctx.wear : 1 + 0.3 * ctx.wear) * grip;
  const double flightW = 0.2 * (ctx.dew ? 0.75 : 1);
  const double threat =
      avg({{turnW, b[BowlK::turn]}, {flightW, b[BowlK::flight]}, {0.3, b[BowlK::accuracy]}, {0.22, b[BowlK::variations]}});
  const double control = avg({{0.55, b[BowlK::accuracy]}, {0.25, b[BowlK::variations]}, {0.2, b[BowlK::turn]}});
  return {threat, control, false};
}

BatterNow batterNow(const Player& batter, bool spin, bool pressure) {
  const auto& s = batter.skills.bat;
  const double matchup = spin ? s[BatK::vsSpin] : s[BatK::vsPace];
  return {
      0.45 * s[BatK::defence] + 0.35 * matchup + 0.2 * (pressure ? s[BatK::temperament] : s[BatK::defence]),
      0.55 * s[BatK::grounded] + 0.3 * matchup + 0.15 * s[BatK::lofted],
      0.75 * s[BatK::lofted] + 0.25 * matchup,
      0.7 * s[BatK::rotation] + 0.3 * s[BatK::running],
      s[BatK::running],
      s[BatK::lofted],
      s[BatK::temperament],
  };
}

FieldingNow fieldingNow(const Match& match, const Innings& inn) {
  const MatchTeam& team = match.teams[static_cast<size_t>(inn.bowlingSide)];
  double c = 0, g = 0, t = 0;
  for (const PlayerId& id : team.players) {
    const auto& f = match.players.at(id).skills.field;
    c += f[FieldK::catching];
    g += f[FieldK::ground];
    t += f[FieldK::throwing];
  }
  const double n = static_cast<double>(team.players.size());
  const Player& keeper = match.players.at(team.keeperId);
  return {c / n, g / n, t / n, keeper.skills.field[FieldK::keeping]};
}

DismissalWeights dismissalWeights(const Player& bowler, Phase phase, bool spin, double running,
                                  const FieldingNow& field) {
  const auto& b = bowler.skills.bowl;
  const double runOut = js_exp((60 - running) / 40) * js_exp((field.throwing - 65) / 60);
  if (!spin) {
    return {
        {Out::caught, 55},
        {Out::bowled, 20 * (1 + (b[BowlK::pace] - 60) / 120 + (phase == Phase::death ? (b[BowlK::death] - 60) / 150 : 0))},
        {Out::lbw, 13 * (1 + (b[BowlK::swing] - 60) / 90)},
        {Out::caughtKeeper, 8 * (1 + (b[BowlK::seam] - 60) / 80 + (b[BowlK::swing] - 60) / 150)},
        {Out::runout, 4 * runOut},
    };
  }
  return {
      {Out::caught, 52 * (1 + (b[BowlK::flight] - 60) / 150)},
      {Out::bowled, 13 * (1 + (b[BowlK::variations] - 60) / 80)},
      {Out::lbw, 15 * (1 + (b[BowlK::turn] - 60) / 120)},
      {Out::stumped, 11 * (1 + (b[BowlK::flight] - 60) / 70) * (field.keeping / 70)},
      {Out::caughtKeeper, 2},
      {Out::runout, 7 * runOut},
  };
}

double dropChance(double skill) { return std::min(0.35, std::max(0.03, 0.12 * js_exp(-(skill - 65) / 22))); }

double missedStumpingChance(double keeping) {
  return std::min(0.3, std::max(0.02, 0.1 * js_exp(-(keeping - 65) / 20)));
}

Edge specialistEdge(const Player& bowler, const BallContext& ctx) {
  const auto& b = bowler.skills.bowl;
  const double ov = bowler.bowl;
  double w = 0;   // wicket edge
  double bd = 0;  // boundary edge (positive = more boundaries conceded)
  if (isPaceType(bowler.bowlType)) {
    w += (b[BowlK::swing] - ov) * (ctx.over < 3 ? 1.0 : ctx.over < 6 ? 0.5 : 0.1);
    w += (b[BowlK::seam] - ov) * (ctx.pitch == Pitch::green ? 1.0 * (1 - 0.7 * ctx.wear) : 0.3);
    w += (b[BowlK::pace] - ov) * 0.3;
    if (ctx.phase == Phase::death) {
      bd -= (b[BowlK::death] - ov) * 1.0;
      w += (b[BowlK::death] - ov) * 0.4;
    }
    if (ctx.phase != Phase::pp) w += (b[BowlK::variations] - ov) * 0.4;
  } else {
    const double grip = ctx.dew ? 0.5 : 1;
    w += (b[BowlK::turn] - ov) * (ctx.pitch == Pitch::dusty ? 1.0 + ctx.wear : 0.3 + 0.5 * ctx.wear) * grip;
    w += (b[BowlK::flight] - ov) * 0.4 * grip;
    bd += (b[BowlK::flight] - ov) * 0.2;  // flight invites the big shot
    w += (b[BowlK::variations] - ov) * 0.5;
    bd -= (b[BowlK::accuracy] - ov) * 0.5;
  }
  return {js_exp((1.2 * w) / 100), js_exp(bd / 100)};
}

}  // namespace t20
