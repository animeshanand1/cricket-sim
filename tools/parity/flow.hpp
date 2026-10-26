#pragma once
// The C++ twin of tools/parity/flow_js.mjs: the JS flow test's path (season, review, off-season,
// season 2) through the C++ engine, calling it in the same order as the JS store's thunks.
// Returns the same checkpoints as JSON, to compare with tests/golden/flow.json.

#include <cstdint>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace t20::parity {

// seeds: the season seeds the JS run used (golden "seeds"), in order
Json runFlow(const std::string& team, const std::vector<int32_t>& seeds);

// The same path through the app's Game controller, with Math.random / Date.now stubbed exactly as
// flow_js.mjs stubs them (rseed: its RANDOM_SEED argument). Checkpoints include the setup view.
Json runGameFlow(const std::string& team, uint32_t rseed);

// The twin of tools/parity/world_js.mjs: N off-seasons, the AI making every decision
Json runOffseasons(int years);

}  // namespace t20::parity
