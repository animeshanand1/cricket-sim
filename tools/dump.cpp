// Prints the same JSON as `node tools/parity/dump_js.mjs <command>`, from the C++ engine.
//   t20coach_dump league | matches FROM TO | userflow FROM TO | balllog SEED | flow TEAM SEED1 SEED2

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "core/json.hpp"
#include "parity/flow.hpp"
#include "parity/parity.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);  // byte-for-byte output, no "\r\n" translation
#endif
  const std::string cmd = argc > 1 ? argv[1] : "";
  auto arg = [&](int i, int fallback) { return argc > i ? std::atoi(argv[i]) : fallback; };
  std::string out;
  if (cmd == "league")
    out = t20::parity::dumpLeague();
  else if (cmd == "matches")
    out = t20::parity::dumpMatches(arg(2, 1), arg(3, 400));
  else if (cmd == "userflow")
    out = t20::parity::dumpUserflow(arg(2, 1), arg(3, 40));
  else if (cmd == "balllog")
    out = t20::parity::dumpBallLog(arg(2, 1));
  else if ((cmd == "flowcheck" || cmd == "gamecheck") && argc > 2) {
    // Compare a JS flow dump (node tools/parity/flow_js.mjs TEAM SEED > FILE) with the C++ flow
    std::ifstream in(argv[2], std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const auto js = t20::Json::parse(ss.str());
    if (!js) {
      std::fprintf(stderr, "cannot read %s\n", argv[2]);
      return 1;
    }
    std::vector<int32_t> seeds;
    for (const auto& x : (*js)["seeds"].elements()) seeds.push_back(static_cast<int32_t>(x.num()));
    const bool viaGame = cmd == "gamecheck";
    const t20::Json cpp = viaGame ? t20::parity::runGameFlow((*js)["team"].str(), static_cast<uint32_t>((*js)["rseed"].num(7)))
                                  : t20::parity::runFlow((*js)["team"].str(), seeds);
    int bad = 0;
    for (const auto& [cp, sections] : (*js)["checkpoints"].items())
      for (const char* sec : {"world", "career", "season", "setup"}) {
        if (!viaGame && std::string(sec) == "setup") continue;
        if (!sections.find(sec)) continue;
        const auto d = t20::jsonDiff(sections[sec], cpp["checkpoints"][cp][sec], cp + "." + sec);
        std::printf("%-22s %s\n", (cp + "." + sec).c_str(), d ? d->c_str() : "same");
        if (d) ++bad;
      }
    return bad ? 2 : 0;
  }
  else if (cmd == "flow" && argc > 2) {
    std::vector<int32_t> seeds;
    for (int i = 3; i < argc; ++i) seeds.push_back(static_cast<int32_t>(std::atoll(argv[i])));
    out = t20::parity::runFlow(argv[2], seeds).dump();
  }
  else {
    std::fprintf(stderr, "usage: t20coach_dump league | matches FROM TO | userflow FROM TO | balllog SEED\n");
    return 1;
  }
  std::fwrite(out.data(), 1, out.size(), stdout);
  return 0;
}
