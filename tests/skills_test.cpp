// Port of t20-coach/tests/skills.test.js

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "engine/battle.hpp"
#include "engine/league.hpp"
#include "engine/skills.hpp"

using namespace t20;

namespace {
std::vector<Player> allPlayers() {
  std::vector<Player> out;
  for (const Team& t : generateLeague())
    for (const Player& p : t.squad) out.push_back(p);
  return out;
}
}  // namespace

TEST_CASE("every headline rating is exactly the weighted mix of its sub-skills", "[skills]") {
  for (const Player& p : allPlayers()) {
    CHECK(p.bat == battingOverall(p.skills.bat));
    CHECK(p.bowl == bowlingOverall(p.skills.bowl, p.bowlType));
    CHECK(p.field == fieldingOverall(p.skills.field, p.role == Role::WK));
    CHECK(p.power == p.skills.bat[BatK::lofted]);
    CHECK(p.awareness == p.skills.bat[BatK::awareness]);
  }
}

TEST_CASE("has every sub-skill for the right bowling type, all on a 1-99 scale", "[skills]") {
  for (const Player& p : allPlayers()) {
    for (const auto& d : BAT_SKILLS) CHECK(p.skills.bat[d.key] >= 1);
    const auto& defs = isPaceType(p.bowlType) ? PACE_SKILLS : SPIN_SKILLS;
    for (const auto& d : defs) {
      REQUIRE(p.skills.bowl.contains(d.key));
      CHECK(p.skills.bowl[d.key] >= 1);
      CHECK(p.skills.bowl[d.key] <= 99);
    }
  }
}

TEST_CASE("weights add up to 100%", "[skills]") {
  double bat = 0, pace = 0, spin = 0;
  for (const auto& d : BAT_SKILLS) bat += d.weight;
  for (const auto& d : PACE_SKILLS) pace += d.weight;
  for (const auto& d : SPIN_SKILLS) spin += d.weight;
  CHECK(bat == Catch::Approx(1).margin(1e-6));
  CHECK(pace == Catch::Approx(1).margin(1e-6));
  CHECK(spin == Catch::Approx(1).margin(1e-6));
}

TEST_CASE("gives quicks realistic speeds", "[skills]") {
  double lo = 1e9, hi = -1e9;
  for (const Player& p : allPlayers()) {
    if ((p.bowlType == BowlType::fast || p.bowlType == BowlType::lfast) && p.bowl >= 60) {
      const double kph = *speedKph(p);
      lo = std::min(lo, kph);
      hi = std::max(hi, kph);
    }
  }
  CHECK(lo >= 130);
  CHECK(hi <= 155);
}

TEST_CASE("safe hands drop fewer catches", "[skills]") {
  CHECK(dropChance(90) < dropChance(65));
  CHECK(dropChance(40) > dropChance(65));
}
