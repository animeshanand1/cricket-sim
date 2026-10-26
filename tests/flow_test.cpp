// A4 parity: a season, the board's review, the off-season and the start of season 2, played by the
// C++ engine, must leave exactly the state the JS game left (tests/golden/flow.json, written by
// `node tools/parity/flow_js.mjs RJR 7`). Compared field by field with jsonDiff.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "core/json.hpp"
#include "parity/flow.hpp"

using namespace t20;

namespace {
Json readGoldenJson(const std::string& name) {
  std::ifstream in(std::string(T20_GOLDEN_DIR) + "/" + name, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string err;
  auto j = Json::parse(ss.str(), &err);
  INFO(err);
  REQUIRE(j.has_value());
  return *j;
}

const Json& golden() {
  static const Json g = readGoldenJson("flow.json");
  return g;
}

const Json& cppFlow() {
  static const Json f = [] {
    std::vector<int32_t> seeds;
    for (const auto& s : golden()["seeds"].elements()) seeds.push_back(static_cast<int32_t>(s.num()));
    return parity::runFlow(golden()["team"].str(), seeds);
  }();
  return f;
}

void requireSection(const char* checkpoint, const char* section) {
  const Json& want = golden()["checkpoints"][checkpoint][section];
  const Json& got = cppFlow()["checkpoints"][checkpoint][section];
  REQUIRE_FALSE(want.isNull());
  REQUIRE_FALSE(got.isNull());
  const auto diff = jsonDiff(want, got, std::string(checkpoint) + "." + section);
  INFO("JS vs C++: " << diff.value_or(""));
  REQUIRE_FALSE(diff.has_value());
}
}  // namespace

TEST_CASE("offseasons: four AI off-seasons, mega auction included, match JS", "[parity][flow]") {
  const Json want = readGoldenJson("offseasons.json");
  const Json got = parity::runOffseasons(want["years"].integer());
  const auto r = jsonDiff(want["results"], got["results"], "results");
  INFO("JS vs C++: " << r.value_or(""));
  REQUIRE_FALSE(r.has_value());
  const auto w = jsonDiff(want["world"], got["world"], "world");
  INFO("JS vs C++: " << w.value_or(""));
  REQUIRE_FALSE(w.has_value());
}

TEST_CASE("flow: season 1 is created exactly as in JS", "[parity][flow]") { requireSection("start", "season"); }

TEST_CASE("flow: a whole season plays out exactly as in JS", "[parity][flow]") {
  requireSection("seasonDone", "season");
  requireSection("seasonDone", "career");
  requireSection("seasonDone", "world");
}

TEST_CASE("flow: the board's review and the off-season match JS", "[parity][flow]") {
  requireSection("offseason", "season");
  requireSection("offseason", "world");
}

TEST_CASE("flow: contracts, draft, auction and season 2 match JS", "[parity][flow]") {
  requireSection("season2", "world");
  requireSection("season2", "career");
  requireSection("season2", "season");
  // The JS flow test's own checks
  const Json& s2 = cppFlow()["checkpoints"]["season2"];
  CHECK(s2["season"]["season"]["number"].integer() == 2);
  CHECK(s2["world"]["manager"]["target"].isObject());
  CHECK(s2["world"]["teams"][golden()["team"].str()]["squad"].size() >= 18);
}
