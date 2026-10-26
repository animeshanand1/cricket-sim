// JS number semantics: Math.round, Math.exp (fdlibm as in V8), mulberry32, toFixed, toString,
// toLocaleString('en-IN'). Reference values come from node (tests/golden/jsmath.txt).

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/jsmath.hpp"
#include "engine/rng.hpp"
#include "engine/rules.hpp"

using namespace t20;

namespace {
std::vector<std::vector<std::string>> goldenLines() {
  std::ifstream in(std::string(T20_GOLDEN_DIR) + "/jsmath.txt");
  REQUIRE(in.good());
  std::vector<std::vector<std::string>> out;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::vector<std::string> parts;
    std::string p;
    while (ss >> p) parts.push_back(p);
    if (!parts.empty()) out.push_back(parts);
  }
  return out;
}

double fromHex(const std::string& h) {
  const uint64_t bits = std::stoull(h, nullptr, 16);
  double d;
  std::memcpy(&d, &bits, sizeof d);
  return d;
}

uint64_t bitsOf(double d) {
  uint64_t b;
  std::memcpy(&b, &d, sizeof b);
  return b;
}
}  // namespace

TEST_CASE("Math.round rounds halves up", "[jsmath]") {
  CHECK(js_round(2.5) == 3);
  CHECK(js_round(-2.5) == -2);
  CHECK(js_round(0.49999999999999994) == 0);
  CHECK(js_round(-0.5) == 0);
  CHECK(js_round(1.4999) == 1);
}

TEST_CASE("Math.imul and ToInt32", "[jsmath]") {
  CHECK(js_imul(123456789, kGolden) == 2146089093);  // Math.imul(123456789, 2654435761) in node
  CHECK(js_toint32(2147483648.0) == -2147483648);
  CHECK(js_toint32(-1.9) == -1);
  CHECK(js_toint32(4294967297.5) == 1);
}

TEST_CASE("mulberry32 matches rng.js", "[jsmath]") {
  int checked = 0;
  for (const auto& p : goldenLines()) {
    if (p[0] != "rng") continue;
    Rng r(static_cast<int32_t>(std::stol(p[1])));
    for (size_t i = 2; i < p.size(); ++i) {
      CHECK(r() == std::strtod(p[i].c_str(), nullptr));
      ++checked;
    }
  }
  CHECK(checked == 36);
}

TEST_CASE("exp is bit-identical to V8's Math.exp", "[jsmath]") {
  int checked = 0, wrong = 0;
  for (const auto& p : goldenLines()) {
    if (p[0] != "exp") continue;
    const double x = fromHex(p[1]);
    const double want = fromHex(p[2]);
    if (bitsOf(js_exp(x)) != bitsOf(want)) ++wrong;
    ++checked;
  }
  CHECK(checked == 4008);
  CHECK(wrong == 0);
}

TEST_CASE("toFixed, toString and en-IN grouping match JS", "[jsmath]") {
  for (const auto& p : goldenLines()) {
    if (p[0] == "fixed") {
      INFO(p[1] << " toFixed(" << p[2] << ")");
      CHECK(js_fixed(std::strtod(p[1].c_str(), nullptr), std::stoi(p[2])) == p[3]);
    } else if (p[0] == "num") {
      INFO(p[1]);
      CHECK(js_num(std::strtod(p[1].c_str(), nullptr)) == p[2]);
    } else if (p[0] == "indian") {
      CHECK(fmt_indian(std::stod(p[1])) == p[2]);
    }
  }
}

TEST_CASE("over and ball labels", "[rules]") {
  CHECK(oversText(0) == "0");
  CHECK(oversText(13) == "2.1");
  CHECK(oversText(120) == "20");
  CHECK(ballTag(0) == "0.1");
  CHECK(ballTag(119) == "19.6");
}
