// The JSON layer: JSON.stringify-compatible output, a forgiving reader, the field-by-field diff
// the parity tests rely on, and save-format round trips on real JS game states.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

#include "core/json.hpp"
#include "engine/state_json.hpp"

using namespace t20;

namespace {
std::string readFile(const std::string& name) {
  std::ifstream in(std::string(T20_GOLDEN_DIR) + "/" + name, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
Json parseOk(const std::string& text) {
  std::string err;
  auto j = Json::parse(text, &err);
  INFO(err);
  REQUIRE(j.has_value());
  return *j;
}
}  // namespace

TEST_CASE("json: writes exactly what JSON.stringify wrote (byte for byte)", "[json]") {
  for (const char* file : {"league.json", "matches.json", "flow.json", "offseasons.json"}) {
    INFO(file);
    const std::string text = readFile(file);
    REQUIRE(parseOk(text).dump() == text);
  }
}

TEST_CASE("json: escapes, unicode and numbers", "[json]") {
  const Json j = parseOk(R"({"a":"line\nbreak \"q\" \\ é 🏏","n":[0,-0,1.5,1e21,0.1,-12],"t":true,"z":null})");
  CHECK(j["a"].str() == "line\nbreak \"q\" \\ \xc3\xa9 \xf0\x9f\x8f\x8f");
  CHECK(j.dump() == "{\"a\":\"line\\nbreak \\\"q\\\" \\\\ \xc3\xa9 \xf0\x9f\x8f\x8f\",\"n\":[0,0,1.5,1e+21,0.1,-12],\"t\":true,\"z\":null}");
  CHECK(j["missing"].isNull());
  CHECK(j["n"].at(2).num() == 1.5);
}

TEST_CASE("json: rejects malformed input instead of guessing", "[json]") {
  for (const char* bad : {"", "{", "[1,]", "{\"a\" 1}", "tru", "\"open", "{\"a\":1} x", "[1 2]"}) {
    INFO(bad);
    std::string err;
    CHECK_FALSE(Json::parse(bad, &err).has_value());
    CHECK_FALSE(err.empty());
  }
}

TEST_CASE("json: the diff finds real differences and ignores JS's empty-value habits", "[json]") {
  const Json a = parseOk(R"({"x":1,"s":"a","arr":[1,2],"o":{"k":1}})");
  CHECK_FALSE(jsonDiff(a, parseOk(R"({"o":{"k":1},"arr":[1,2],"s":"a","x":1})")).has_value());  // key order
  CHECK(jsonDiff(a, parseOk(R"({"x":2,"s":"a","arr":[1,2],"o":{"k":1}})")).value() == "x: 1 vs 2");
  CHECK(jsonDiff(a, parseOk(R"({"x":1,"s":"b","arr":[1,2],"o":{"k":1}})")).has_value());
  CHECK(jsonDiff(a, parseOk(R"({"x":1,"s":"a","arr":[1,2,3],"o":{"k":1}})")).value() == "arr: length 2 vs 3");
  CHECK(jsonDiff(a, parseOk(R"({"x":1,"s":"a","arr":[1,2],"o":{"k":2}})")).value() == "o.k: 1 vs 2");
  CHECK(jsonDiff(a, parseOk(R"({"x":1,"s":"a","arr":[1,2]})")).has_value());  // a real object went missing
  // null, false, [] and {} all mean "nothing" in the JS game
  CHECK_FALSE(jsonDiff(parseOk(R"({"a":null,"b":false,"c":[],"d":{}})"), parseOk("{}")).has_value());
  CHECK(jsonDiff(parseOk(R"({"a":0})"), parseOk("{}")).has_value());  // 0 is a value
  // Mismatched checkpoints of a real career are told apart
  const Json flow = parseOk(readFile("flow.json"));
  CHECK(jsonDiff(flow["checkpoints"]["seasonDone"]["world"], flow["checkpoints"]["season2"]["world"]).has_value());
}

TEST_CASE("save format: world, career and season from the JS game read back and write out the same", "[json][save]") {
  const Json flow = parseOk(readFile("flow.json"));
  for (const char* cp : {"seasonDone", "season2"}) {
    INFO(cp);
    const Json& c = flow["checkpoints"][cp];
    const World w = worldFromJson(c["world"]);
    auto d = jsonDiff(c["world"], toJson(w), "world");
    INFO(d.value_or(""));
    CHECK_FALSE(d.has_value());
    const Career career = careerFromJson(c["career"]);
    d = jsonDiff(c["career"], toJson(career), "career");
    INFO(d.value_or(""));
    CHECK_FALSE(d.has_value());
    const SeasonState s = seasonStateFromJson(c["season"]);
    d = jsonDiff(c["season"], toJson(s), "season");
    INFO(d.value_or(""));
    CHECK_FALSE(d.has_value());
  }
  // An off-season in progress (draft and report) too
  const World mid = worldFromJson(flow["checkpoints"]["offseason"]["world"]);
  CHECK(mid.offseason.has_value());
  CHECK(mid.lastOffseason.has_value());
  CHECK_FALSE(jsonDiff(flow["checkpoints"]["offseason"]["world"], toJson(mid)).has_value());
}

TEST_CASE("save format: missing or odd fields fall back to defaults instead of failing", "[json][save]") {
  const World w = worldFromJson(parseOk(R"({"players":{"X1":{"name":"A B","injury":null,"retired":null}},"teams":{}})"));
  REQUIRE(w.players.contains("X1"));
  CHECK(w.players.at("X1").id == "X1");
  CHECK_FALSE(w.players.at("X1").injury.has_value());
  CHECK(w.manager.confidence == 60);
  const Career c = careerFromJson(parseOk("{}"));
  CHECK(c.players.empty());
  const SeasonState s = seasonStateFromJson(parseOk(R"({"season":null,"history":[{"number":1}]})"));
  CHECK_FALSE(s.season.has_value());
  CHECK(s.history.size() == 1);
}
