// Bulk-simulate AI v AI matches and print T20 benchmark stats (port of scripts/balance.mjs).
// Usage: t20coach_balance [matches] [pitch] [--seed-base S]
// With --seed-base the teams, pitches and seeds come from a seeded stream, so the run can be
// compared line for line with `node tools/parity/balance_js.mjs N [pitch] --seed-base S`.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/jsmath.hpp"
#include "core/ordered_map.hpp"
#include "core/str.hpp"
#include "engine/league.hpp"
#include "engine/match.hpp"
#include "engine/pitch.hpp"
#include "engine/rng.hpp"
#include "parity/parity.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace t20;

namespace {
// The dismissal from the wicket commentary, as the JS regex / (b |lbw|c & b|c †|st †|run out|c )/
std::string dismissalOf(const std::string& text) {
  static const char* alts[] = {"b ", "lbw", "c & b", "c †", "st †", "run out", "c "};
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != ' ') continue;
    for (const char* alt : alts) {
      const size_t n = std::strlen(alt);
      if (text.compare(i + 1, n, alt) == 0) {
        std::string k(alt);
        while (!k.empty() && k.back() == ' ') k.pop_back();
        return k;
      }
    }
  }
  return "?";
}

std::optional<Pitch> parsePitch(const std::string& s) {
  for (Pitch p : PITCH_KEYS)
    if (name(p) == s) return p;
  return std::nullopt;
}
}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);  // "\n" line endings, as node prints them
#endif
  std::vector<std::string> pos;
  std::optional<int32_t> seedBase;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--seed-base" && i + 1 < argc) {
      seedBase = static_cast<int32_t>(std::atol(argv[++i]));
    } else {
      pos.emplace_back(argv[i]);
    }
  }
  const int N = pos.size() > 0 ? std::atoi(pos[0].c_str()) : 2000;
  std::optional<Pitch> onlyPitch;
  if (pos.size() > 1) {
    onlyPitch = parsePitch(pos[1]);
    if (!onlyPitch) {
      std::fprintf(stderr, "unknown pitch %s\n", pos[1].c_str());
      return 1;
    }
  }

  // Math.random, or a seeded mulberry32 stream
  std::mt19937_64 mt{std::random_device{}()};
  Rng seeded(seedBase.value_or(0));
  auto rand = [&]() -> double {
    if (seedBase) return seeded();
    return static_cast<double>(mt() >> 11) * 0x1.0p-53;
  };

  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<Team> league = generateLeague();

  struct Group {
    double r = 0, b = 0, outs = 0;
  };
  std::vector<double> first, dry, wk1, bowlEcon;
  int rainy = 0, allOut = 0, chaseWins = 0, ties = 0, impacts = 0, inns = 0;
  double fours = 0, sixes = 0, extras = 0, dots = 0, legal = 0;
  Group top, tail;
  double phaseRuns[3] = {0, 0, 0}, phaseOvers[3] = {0, 0, 0};
  OrderedMap<std::string, int> wkTypes;
  int fifties = 0, hundreds = 0;
  int hauls[6] = {0, 0, 0, 0, 0, 0};
  int exB = 0, exSaves = 0, exOverthrows = 0, exReviews = 0, exOverturned = 0;

  for (int i = 0; i < N; ++i) {
    const int a = static_cast<int>(std::floor(rand() * static_cast<double>(league.size())));
    int b = static_cast<int>(std::floor(rand() * static_cast<double>(league.size() - 1)));
    if (b >= a) ++b;
    const Pitch pitch = onlyPitch ? *onlyPitch : PITCH_KEYS[static_cast<size_t>(std::floor(rand() * PITCH_KEYS.size()))];
    MatchSetup setup;
    setup.seed = js_toint32(rand() * 2147483648.0);
    setup.teams = {parity::side(league[static_cast<size_t>(a)], pitch), parity::side(league[static_cast<size_t>(b)], pitch)};
    setup.userTeam.reset();
    setup.pitch = pitch;
    Match m = createMatch(setup);
    simMatch(m);
    const Innings& i1 = m.innings[0];
    const Innings& i2 = m.innings[1];
    first.push_back(i1.runs);
    if (!m.weather.rain)
      dry.push_back(i1.runs);
    else
      ++rainy;
    wk1.push_back(i1.wickets);
    if (i1.wickets == 10) ++allOut;
    if (m.result->winner && *m.result->winner == i2.batting) ++chaseWins;
    if (m.result->superOver) ++ties;
    for (const auto& t : m.teams)
      if (t.impact) ++impacts;
    for (const Innings* inn : {&i1, &i2}) {
      ++inns;
      extras += inn->extras.wd + inn->extras.nb + inn->extras.lb + inn->extras.b;
      legal += inn->legalBalls;
      exB += inn->extras.b;
      for (const Over& o : inn->overs)
        for (const Ball& bl : o.balls) {
          if (bl.saved) ++exSaves;
          if (bl.overthrow) ++exOverthrows;
        }
      for (const auto& r : inn->reviews) {
        ++exReviews;
        if (r.overturned) ++exOverturned;
      }
      for (size_t idx = 0; idx < inn->battedOrder.size(); ++idx) {
        const BatterInnings& st = inn->batters.at(inn->battedOrder[idx]);
        Group* g = idx < 4 ? &top : idx >= 8 ? &tail : nullptr;
        if (st.runs >= 100)
          ++hundreds;
        else if (st.runs >= 50)
          ++fifties;
        if (g) {
          g->r += st.runs;
          g->b += st.balls;
          g->outs += st.out ? 1 : 0;
        }
        fours += st.fours;
        sixes += st.sixes;
      }
      for (const auto& [id, f] : inn->bowling) {
        dots += f.dots;
        if (f.balls >= 24) bowlEcon.push_back((f.runs * 6.0) / f.balls);
        for (int k : {3, 4, 5})
          if (f.wickets >= k) ++hauls[k];
      }
      for (size_t k = 0; k < inn->overs.size(); ++k) {
        const int ph = k < 6 ? 0 : k < 15 ? 1 : 2;
        phaseRuns[ph] += inn->overs[k].runs;
        phaseOvers[ph] += 1;
      }
      for (const auto& c : inn->commentary) {
        if (c.kind != CommentKind::wicket) continue;
        wkTypes[dismissalOf(c.text)] += 1;
      }
    }
  }

  auto avg = [](const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
  };
  std::vector<double> sorted = first;
  std::stable_sort(sorted.begin(), sorted.end());
  auto pct = [&](double p) { return sorted[static_cast<size_t>(std::floor(p * static_cast<double>(sorted.size() - 1)))]; };
  auto f1 = [](double x) { return js_fixed(x, 1); };
  int over200 = 0;
  for (double r : first)
    if (r >= 200) ++over200;

  std::printf("Matches: %d%s\n", N, onlyPitch ? cat(" on ", name(*onlyPitch)).c_str() : "");
  std::printf("%s\n", cat("1st inns avg ", f1(avg(first)), " (dry games ", f1(avg(dry)), ", rain affected ", rainy, ") | p10 ",
                          pct(0.1), " p50 ", pct(0.5), " p90 ", pct(0.9), " | min ", sorted.front(), " max ", sorted.back())
                          .c_str());
  std::printf("%s\n", cat("1st inns wickets avg ", f1(avg(wk1)), " | all out ", f1((100.0 * allOut) / N), "% | 200+ ",
                          f1((100.0 * over200) / N), "%")
                          .c_str());
  std::printf("%s\n", cat("Per innings: 4s ", f1(fours / inns), " 6s ", f1(sixes / inns), " extras ", f1(extras / inns),
                          " dot% ", f1((100 * dots) / legal))
                          .c_str());
  std::printf("%s\n", cat("RPO by phase: PP ", f1(phaseRuns[0] / phaseOvers[0]), " mid ", f1(phaseRuns[1] / phaseOvers[1]),
                          " death ", f1(phaseRuns[2] / phaseOvers[2]))
                          .c_str());
  std::printf("%s\n", cat("Chasing side wins ", f1((100.0 * chaseWins) / N), "% | super overs ", ties).c_str());
  std::printf("%s\n", cat("top  avg ", f1(top.r / std::max(1.0, top.outs)), " SR ", f1((100 * top.r) / top.b)).c_str());
  std::printf("%s\n", cat("tail avg ", f1(tail.r / std::max(1.0, tail.outs)), " SR ", f1((100 * tail.r) / tail.b)).c_str());
  std::printf("%s\n", cat("50s per match ", f1(static_cast<double>(fifties) / N), " | 100s per match ",
                          js_fixed(static_cast<double>(hundreds) / N, 3))
                          .c_str());
  std::printf("%s\n", cat("Impact Player used in ", f1((100.0 * impacts) / (2.0 * N)), "% of team-matches").c_str());
  std::printf("%s\n", cat("Bowler econ (4 overs) avg ", f1(avg(bowlEcon)), " | per match: 3+ wkt hauls ",
                          js_fixed(static_cast<double>(hauls[3]) / N, 2), ", 4+ ", js_fixed(static_cast<double>(hauls[4]) / N, 3),
                          ", 5+ ", js_fixed(static_cast<double>(hauls[5]) / N, 3))
                          .c_str());
  std::printf("%s\n",
              cat("Per innings: byes ", js_fixed(static_cast<double>(exB) / inns, 2), " | boundaries saved ",
                  js_fixed(static_cast<double>(exSaves) / inns, 2), " | overthrows ",
                  js_fixed(static_cast<double>(exOverthrows) / inns, 2), " | reviews ",
                  js_fixed(static_cast<double>(exReviews) / inns, 2), " (",
                  exReviews ? f1((100.0 * exOverturned) / exReviews) : std::string("0"), "% overturned)")
                  .c_str());
  int totalWk = 0;
  for (const auto& [k, v] : wkTypes) totalWk += v;
  std::string line = "Dismissals: ";
  bool firstK = true;
  for (const auto& [k, v] : wkTypes) {
    line += cat(firstK ? "" : ", ", k, " ", f1((100.0 * v) / totalWk), "%");
    firstK = false;
  }
  std::printf("%s\n", line.c_str());
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::fprintf(stderr, "(%d matches in %.2f s, %.3f ms per match)\n", N, secs, 1000 * secs / N);
  return 0;
}
