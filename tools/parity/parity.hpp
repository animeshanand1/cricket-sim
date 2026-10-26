#pragma once
// The C++ twin of tools/parity/dump_js.mjs. Each function returns the same JSON text the JS
// engine produces for the same command, so a golden file can be compared byte for byte.

#include <string>
#include <vector>

#include "engine/match.hpp"

namespace t20::parity {

std::string dumpLeague();
std::string dumpMatches(int from, int to);
std::string dumpUserflow(int from, int to);
std::string dumpBallLog(int seed);

// A side for a match, as the balance script and tests build it
SideInput side(const Team& team, Pitch pitch, bool withBench = true);

// FNV-1a over UTF-8 bytes
uint32_t fnv(const std::string& s);

}  // namespace t20::parity
