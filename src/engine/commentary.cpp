#include "engine/commentary.hpp"

#include <cmath>
#include <vector>

#include "core/str.hpp"

namespace t20 {

std::string surname(const std::string& name) {
  // name.split(' ').slice(1).join(' ') || name
  const auto sp = name.find(' ');
  if (sp == std::string::npos) return name;
  std::string rest = name.substr(sp + 1);
  return rest.empty() ? name : rest;
}

namespace {
using Lines = std::vector<std::string_view>;

// clang-format off
const Lines DOT = {
  "defended solidly back down the pitch",
  "beaten outside off stump! Lovely delivery",
  "straight to cover, no run",
  "full and tight, dug out to the bowler",
  "short of a length, ducks under it",
  "swings hard and misses",
  "punched firmly but finds point",
  "good length, left alone outside off",
  "slower ball, mistimed to mid-on",
  "yorker on the toes, jammed out",
};
const Lines DOT_SWING = {
  "shapes away late and beats the outside edge",
  "swings back in, pad first, not out",
  "late outswing, he plays and misses",
};
const Lines DOT_MYSTERY = {
  "doesn't pick it, the googly spins past the bat",
  "no idea which way that was turning, beaten",
  "carrom ball, squirts it back to the bowler",
};
const Lines DOT_SPIN = {
  "tossed up, defended forward",
  "turns past the outside edge!",
  "skids on, pushed back to the bowler",
  "flighted, blocked to short leg",
  "quicker one, beats the sweep",
};
const Lines ONE = {
  "worked off the pads to square leg for one",
  "pushed to long-on for a single",
  "dabbed down to third man, they take one",
  "tapped into the off side and they scamper through",
  "driven to deep cover, easy single",
  "nudged to fine leg, one run",
  "clipped to deep midwicket for one",
};
const Lines TWO = {
  "driven into the gap at deep cover, they come back for two",
  "pulled to deep square, good running gets them two",
  "flicked wide of long-on, two runs",
  "slashed over point, two taken",
};
const Lines THREE = {
  "placed into the vast gap at long-off, they run three",
  "chased down just short of the rope, three runs",
};
const Lines FOUR = {
  "FOUR! Driven on the up through the covers",
  "FOUR! Pulled hard, beats deep square leg",
  "FOUR! Thick edge flies past the keeper",
  "FOUR! Cut late, races past point",
  "FOUR! Flicked off the pads, fine leg has no chance",
  "FOUR! Straight down the ground past the bowler",
  "FOUR! Scooped over short fine leg",
};
const Lines SIX = {
  "SIX! Launched over long-on",
  "SIX! Clears deep midwicket with ease",
  "SIX! Up and over extra cover, what a shot",
  "SIX! Pulled into the stands",
  "SIX! Picked the slower ball and sent it miles",
  "SIX! Ramped over the keeper",
};
const Lines SIX_SPIN = {
  "SIX! Down the track and over the sightscreen",
  "SIX! Slog-swept into the crowd",
  "SIX! Lofted inside-out over extra cover",
};

const Lines& wicketLines(Out t) {
  static const Lines bowled = {"OUT! Bowled him! Through the gate", "OUT! Clean bowled, the stumps are rattled", "OUT! Yorker, off stump cartwheeling"};
  static const Lines lbw = {"OUT! Trapped in front, the finger goes up", "OUT! LBW, missed the flick and was plumb"};
  static const Lines caught = {"OUT! Caught by {f} in the deep", "OUT! Skied it and {f} takes it comfortably", "OUT! Top edge, {f} runs in and holds on", "OUT! Straight to {f}, gone"};
  static const Lines caughtKeeper = {"OUT! Feathered through to the keeper, {f} makes no mistake"};
  static const Lines caughtBowled = {"OUT! Caught and bowled! Sharp reflexes from {b}"};
  static const Lines stumped = {"OUT! Stumped! Beaten in flight, {f} whips the bails off"};
  static const Lines runout = {"OUT! Run out! Direct hit from {f}", "OUT! Mix-up in the middle, {f} completes the run out"};
  switch (t) {
    case Out::bowled: return bowled;
    case Out::lbw: return lbw;
    case Out::caught: return caught;
    case Out::caughtKeeper: return caughtKeeper;
    case Out::caughtBowled: return caughtBowled;
    case Out::stumped: return stumped;
    default: return runout;
  }
}

// Catches where the fielder actually stood (field.js catchPosition)
const Lines& caughtAt(int zone, bool rope) {
  static const Lines close = {"OUT! Edged, and {f} takes a sharp catch at {p}", "OUT! Bat-pad, {f} snaps it up at {p}"};
  static const Lines ring = {"OUT! Straight to {f} at {p}", "OUT! Mistimed, and {f} takes it at {p}", "OUT! Spooned up, {f} gobbles it up at {p}"};
  static const Lines deep = {"OUT! Holed out, {f} takes it at {p}", "OUT! Skied it, and {f} settles under it at {p}", "OUT! Top edge, {f} runs round at {p} and holds on"};
  static const Lines ropeLines = {"OUT! What a catch! {f} takes it inches inside the rope at {p}", "OUT! Caught on the boundary! {f} juggles, steadies himself and holds it at {p}"};
  if (rope) return ropeLines;
  return zone == 0 ? close : zone == 1 ? ring : deep;
}

struct PlaceLines {
  Lines run, four, six;
};
const PlaceLines& placeFor(Region r) {
  static const PlaceLines straight = {
    {"driven back down the ground", "pushed past the bowler to long-off", "tapped down to long-on"},
    {"Driven straight, past the bowler", "Straight down the ground, nobody stops that"},
    {"Launched straight back over the bowler's head", "Down the track and over the sightscreen"}};
  static const PlaceLines offDrive = {
    {"driven to long-off", "pushed into the gap at extra cover"},
    {"Driven on the up past extra cover", "Lofted over mid-off, one bounce into the rope"},
    {"Up and over extra cover", "Inside-out over long-off"}};
  static const PlaceLines cover = {
    {"driven to deep cover", "punched into the covers"},
    {"Driven through the covers", "Pierces the cover field"},
    {"Lofted over cover, what a shot", "Carved high over deep cover"}};
  static const PlaceLines point = {
    {"cut to deep point", "dabbed to point"},
    {"Cut hard, races past point", "Square cut, beats the man at point"},
    {"Upper-cut over point", "Slashed high over deep point"}};
  static const PlaceLines third = {
    {"dabbed down to third man", "run down late to third man"},
    {"Thick edge flies past the keeper", "Late cut, beats third man"},
    {"Upper-cut over third man", "Ramped over the keeper"}};
  static const PlaceLines midwicket = {
    {"clipped to deep midwicket", "worked into the leg side"},
    {"Whipped through midwicket", "Flicked off the pads, beats midwicket"},
    {"Clears deep midwicket with ease", "Slogged over cow corner"}};
  static const PlaceLines square = {
    {"worked to square leg", "pulled to deep square"},
    {"Pulled hard, beats deep square leg", "Swept fine of square"},
    {"Pulled into the stands", "Slog-swept into the crowd"}};
  static const PlaceLines fine = {
    {"nudged to fine leg", "glanced fine"},
    {"Glanced fine, fine leg has no chance", "Scooped over short fine leg"},
    {"Hooked over fine leg", "Scooped over the keeper for six"}};
  switch (r) {
    case Region::offDrive: return offDrive;
    case Region::cover: return cover;
    case Region::point: return point;
    case Region::third: return third;
    case Region::midwicket: return midwicket;
    case Region::square: return square;
    case Region::fine: return fine;
    default: return straight;
  }
}
// clang-format on

std::string_view choose(RandFn rng, const Lines& arr) {
  return arr[static_cast<size_t>(std::floor(rng() * static_cast<double>(arr.size())))];
}

// String.prototype.replace with a string pattern: the first occurrence only
void replaceFirst(std::string& t, std::string_view pat, const std::string& with) {
  const auto at = t.find(pat);
  if (at != std::string::npos) t.replace(at, pat.size(), with);
}

struct Ctx {
  std::string b, s, f, n, p;
};
std::string fill(std::string_view tmpl, const Ctx& ctx) {
  std::string t(tmpl);
  replaceFirst(t, "{b}", ctx.b);
  replaceFirst(t, "{s}", ctx.s);
  replaceFirst(t, "{f}", ctx.f);
  replaceFirst(t, "{n}", ctx.n);
  replaceFirst(t, "{p}", ctx.p);
  return t;
}
}  // namespace

std::string ballText(RandFn rng, const BallTextArgs& a) {
  Ctx ctx;
  ctx.b = surname(a.bowler->name);
  ctx.s = surname(a.striker->name);
  ctx.f = a.fielder ? surname(a.fielder->name) : "";
  ctx.p = a.catchAt ? lower_ascii(std::string(a.catchAt->label)) : "";
  const std::string lead = cat(ctx.b, " to ", ctx.s);

  if (a.kind == BallTextArgs::Kind::wide)
    return cat(lead, ", wide", a.runs > 1 ? cat(", and it runs away for ", a.runs - 1, " more") : std::string(", drifting down leg"), ".");
  if (a.kind == BallTextArgs::Kind::noball) return cat(lead, ", no ball! Overstepped. Free hit next ball.");
  if (a.wicketType == Out::caught && a.catchAt)
    return cat(lead, ". ", fill(choose(rng, caughtAt(a.catchAt->zone, a.catchAt->rope)), ctx), ".");
  if (a.wicketType) return cat(lead, ". ", fill(choose(rng, wicketLines(*a.wicketType)), ctx), ".");
  if (a.legBye)
    return cat(lead, ", ",
               a.runs == 4 ? std::string("FOUR leg byes, off the pad to fine leg") : cat(a.runs, " leg bye, off the thigh pad"),
               ".");
  if (a.runs == 0) {
    const auto& b = a.bowler->skills.bowl;
    if (!a.spin && a.kph && *a.kph >= 145 && rng() < 0.25)
      return cat(lead, ", ", *a.kph, " kph and too quick for him, hurried into a late jab.");
    if (!a.spin && a.over < 4 && b[BowlK::swing] >= 75 && rng() < 0.35) return cat(lead, ", ", choose(rng, DOT_SWING), ".");
    if (a.spin && b[BowlK::variations] >= 78 && rng() < 0.25) return cat(lead, ", ", choose(rng, DOT_MYSTERY), ".");
    const bool spinLine = a.spin && rng() < 0.5;
    return cat(lead, ", ", choose(rng, spinLine ? DOT_SPIN : DOT), ".");
  }
  if (a.runs == 6) {
    const bool spinLine = a.spin && rng() < 0.5;
    return cat(lead, ", ", choose(rng, spinLine ? SIX_SPIN : SIX), ".");
  }
  const Lines& lines = a.runs == 1 ? ONE : a.runs == 2 ? TWO : a.runs == 3 ? THREE : a.runs == 4 ? FOUR : ONE;
  return cat(lead, ", ", choose(rng, lines), ".");
}

std::string shotText(RandFn rng, const Player& bowler, const Player& striker, Region region, int hit, int runs,
                     const std::optional<Saver>& saver) {
  const std::string lead = cat(surname(bowler.name), " to ", surname(striker.name));
  const PlaceLines& place = placeFor(region);
  if (saver) {
    const std::string f = surname(saver->name);
    const std::string at = lower_ascii(std::string(saver->label));
    const std::string got = runs == 0 ? "no run" : runs == 1 ? "just the single" : cat(runs, " runs");
    if (hit == 4) {
      const std::string four = lower_first(std::string(choose(rng, place.four)));
      return saver->zone == 2 ? cat(lead, ", ", four, "... but ", f, " gets round at ", at, " and cuts it off, ", got, ".")
                              : cat(lead, ", ", four, "... but ", f, " dives full length at ", at, " and stops it, ", got, ".");
    }
    return cat(lead, ", ", choose(rng, place.run), ", but ", f, " is quick to it at ", at, ", ", got, ".");
  }
  if (runs == 6) return cat(lead, ", SIX! ", choose(rng, place.six), ".");
  if (runs == 4) return cat(lead, ", FOUR! ", choose(rng, place.four), ".");
  if (runs == 3) return cat(lead, ", ", choose(rng, place.run), ", and they run three.");
  if (runs == 2) return cat(lead, ", ", choose(rng, place.run), ", they come back for two.");
  return cat(lead, ", ", choose(rng, place.run), " for one.");
}

std::string byeText(const Player& bowler, const Player& striker, int runs) {
  return cat(surname(bowler.name), " to ", surname(striker.name), ", ",
             runs == 4 ? std::string("FOUR byes! It beats the keeper and races away")
                       : cat("misses everything, past the keeper: ", runs, " bye", runs == 1 ? "" : "s"),
             ".");
}

std::string overthrowText(int n) {
  return n == 4 ? " Overthrow! The shy at the stumps misses and runs away for four more."
                : " Wild throw, and they pinch an extra run.";
}

}  // namespace t20
