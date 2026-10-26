// Golden parity: the C++ engine must print exactly what the JS engine printed
// (tests/golden/*.json, written by `node tools/parity/dump_js.mjs ...`).

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "parity/parity.hpp"

namespace {
std::string readGolden(const std::string& name) {
  std::ifstream in(std::string(T20_GOLDEN_DIR) + "/" + name, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// On a mismatch, show where: the record (seed) and the text around the first difference.
void requireSame(const std::string& want, const std::string& got) {
  if (want == got) {
    SUCCEED();
    return;
  }
  size_t i = 0;
  while (i < want.size() && i < got.size() && want[i] == got[i]) ++i;
  const size_t rec = want.rfind("{\"seed\":", i);
  const size_t from = rec == std::string::npos ? (i > 160 ? i - 160 : 0) : rec;
  INFO("first difference at byte " << i);
  INFO("JS : " << want.substr(from, i - from + 160));
  INFO("C++: " << got.substr(from, i - from + 160));
  FAIL("C++ output differs from the JS golden file");
}
}  // namespace

TEST_CASE("league generation is byte-identical to JS (160 players, every skill)", "[parity]") {
  requireSame(readGolden("league.json"), t20::parity::dumpLeague());
}

TEST_CASE("400 seeded AI matches are identical to JS (results, scorecards, commentary, rng)", "[parity]") {
  requireSame(readGolden("matches.json"), t20::parity::dumpMatches(1, 400));
}

TEST_CASE("40 scripted user-captained matches are identical to JS", "[parity]") {
  requireSame(readGolden("userflow.json"), t20::parity::dumpUserflow(1, 40));
}
