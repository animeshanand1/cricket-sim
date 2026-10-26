#pragma once
// Field placements (field.js): nine fielders from a catalogue of real positions, with the T20
// restrictions (2 outside the circle in the powerplay, 5 after it, no more than 2 behind square
// on the leg side). Angles are degrees from straight (0), positive on the off side for a
// right-hander; distance is a share of the boundary radius.

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/match_types.hpp"
#include "engine/rng.hpp"

namespace t20 {

enum class Zone : uint8_t { close, ring, deep };

struct PositionDef {
  std::string_view label;
  double angle, dist;
  Zone zone;
};
const PositionDef& position(Pos p);

struct PresetDef {
  FieldPreset key;
  std::string_view label, help;
  std::vector<Pos> positions;
};
extern const std::vector<PresetDef> PRESETS;
const PresetDef& presetByKey(FieldPreset k);

// Fielders allowed outside the circle for this over
inline int maxDeep(Phase phase) { return phase == Phase::pp ? 2 : 5; }

struct FieldShape {
  int close = 0, ring = 0, deep = 0, off = 0, leg = 0, legBehind = 0;
};
FieldShape describeField(const std::vector<Pos>& positions);
// Problems with a field for this phase (empty if it is legal)
std::vector<std::string> fieldProblems(const std::vector<Pos>& positions, Phase phase);
// Pull deep fielders into the ring until the field is legal for this phase
std::vector<Pos> legalize(const std::vector<Pos>& positions, Phase phase);

struct FieldEffect {
  double W, o0, o1, o2, o4, o6;
};
FieldEffect fieldEffect(const std::vector<Pos>& positions, const Player& bowler, const Player* batter, Plan plan);
std::vector<std::pair<Out, double>> fieldDismissals(const std::vector<Pos>& positions,
                                                    std::vector<std::pair<Out, double>> weights);

// Put players into positions: best catchers close in, then the deep, then the ring.
std::array<PlayerId, kPosCount> assignFielders(const std::vector<Pos>& positions, const std::vector<PlayerId>& ids,
                                               const OrderedMap<PlayerId, Player>& P);

struct Nearest {
  Pos pos;
  double cover;
};
std::optional<Nearest> nearestFielder(const std::vector<Pos>& positions, double dir, std::initializer_list<Zone> zones);

// What the field does to a shot hit for `runs` in direction `dir`
struct ShotResolution {
  int runs = 0;
  std::optional<Pos> pos;
  bool saved = false;
  bool catchOnRope = false;
};
using FielderAt = std::function<const Player*(Pos)>;
ShotResolution resolveShot(RandFn r, int runs, double dir, const std::vector<Pos>& positions, const FielderAt& fielderAt,
                           bool freeHit);

// Where a catch goes
std::optional<Pos> catchPosition(RandFn r, const std::vector<Pos>& positions, bool spin,
                                 const std::function<double(double)>& lean);

struct FieldSet {
  FieldPreset preset;
  std::vector<Pos> positions;
};
// The AI captain's field for this ball
FieldSet aiField(Captain style, Phase phase, Plan plan, const Player& bowler);

}  // namespace t20
