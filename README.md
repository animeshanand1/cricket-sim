# T20 Coach: C++ port

The C++ port of the T20 Coach engine. The JavaScript game in `../t20-coach/` is the reference
and is never changed; the C++ is checked against it. Plan and rules: `../docs/cpp-migration/`.

## Status

| Step | What | State |
|------|------|-------|
| A2 | Foundations: `rng`, `rules`, `pitch`, `names`, `roster`, `skills`, `league` | Done. `generateLeague()` is byte-identical to JS |
| A3 | Match engine: `battle`, `mindset`, `tactics`, `field`, `special`, `matchups`, `shots`, `weather`, `commentary`, `fans`, `ai`, `match`, `winprob` | Done. Seeded matches replay exactly as in JS |
| A1 | Window: SDL3 + Dear ImGui, theme, fonts embedded | Done |
| S05 5a–5c | Match screen wired to the engine (`src/app/match_session`, `src/ui/screens/match_screen`), captain's calls (`captain_panel`), crease cards (`crease`), Impact Player (`impact_panel`) in `src/ui/match/` | Done |
| A4 | World and career: `career`, `development`, `records`, `popularity`, `milestones`, `achievements`, `season`, `sides`, `fitness`, `ageing`, `world`, `finance`, `auction`, `board`, `club`, `trades`, `editor` (+ create player and club captain), `league_view` (data.js) | Done. A season, the board's review, the off-season and season 2 leave exactly the JS game's state (world, career, season compared field by field) |
| A5 | `app/Game` (Redux slices, thunks and the store's listener in one controller), `app/SaveManager` (saves), JSON layer (`core/json`, `engine/state_json`) | Done. `Game` replays the real JS store's runs exactly; quitting and relaunching restores the career; saves are the browser's JSON, both ways |
| A6, other screens | Widget kit, then the screens | Not started |

## Build (Windows, MinGW-w64 GCC from WinLibs)

The toolchain (GCC 16, CMake, Ninja) is installed by winget under
`%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin`. Put that
folder on `PATH`, then:

```sh
cmake --preset mingw-release
cmake --build --preset mingw-release
```

Outputs in `build/mingw-release/` (static, no DLLs needed):

| Exe | What it does |
|-----|--------------|
| `t20coach.exe` | The game (no console, no DLLs). Loads the career from the save folder (below) and records every finished match into it. `--screenshot out.bmp [--balls N] [--moment] [--until bowler\|batter\|review\|batting\|bowling] [--click X,Y]... [--size WxH] [--seed S]` plays to a state (the AI answers calls on the way), clicks, renders one frame to a file and exits (no saves unless `--save-dir` is given). `--save-dir DIR` uses another save folder; `--no-save` plays without saves |
| `t20_tests.exe` | Catch2 suite: unit tests and golden parity tests |
| `t20coach_balance.exe [N] [pitch] [--seed-base S]` | Simulates N AI v AI matches and prints the benchmark stats (port of `scripts/balance.mjs`) |
| `t20coach_dump.exe league \| matches A B \| userflow A B \| balllog SEED` | Prints the same JSON as `tools/parity/dump_js.mjs` |
| `t20coach_dump.exe flow TEAM SEED1 SEED2` | The career flow (season, review, off-season, season 2) through the engine, as JSON |
| `t20coach_dump.exe flowcheck FILE` / `gamecheck FILE` | Replays a `flow_js.mjs` dump through the engine (`flowcheck`) or the `Game` controller (`gamecheck`, setup screen included) and prints `same` or the first difference per section |

## Saves

`%APPDATA%\T20Coach\T20Coach\saves\slot1\` (from `SDL_GetPrefPath`): `world.json`, `career.json`,
`season.json`, `setup.json` (the same JSON the browser game keeps in localStorage, so saves move
between the two in both directions) and `meta.json`. Each write goes to `X.tmp`, the old file is
kept as `X.bak`, then the new one replaces it; a missing or corrupt file falls back to its `.bak`,
then to a fresh start. The game autosaves at most once a second, saves straight after every match
and on quit, and logs what it loaded and saved in `...\T20Coach\T20Coach\logs\t20coach.log`. The
match in progress is not saved (the browser game doesn't save it either).

Third-party code lives in `third_party/` (downloaded once with curl, because WinLibs' CMake has
no CA bundle for HTTPS): SDL3 3.2.10 (built from source as a static library), Dear ImGui 1.91.9b,
Barlow / Barlow Condensed (OFL, from google/fonts; `cmake/embed_fonts.cmake` compiles them into
the exe). Build without the game with `-DT20_BUILD_APP=OFF`.

Catch2 comes from `third_party/Catch2-3.8.1` (downloaded once with curl, because WinLibs' CMake
has no CA bundle for HTTPS). Delete that folder to let CMake download it instead.

## Tests

```sh
build/mingw-release/t20_tests.exe            # everything, about 2 s
build/mingw-release/t20_tests.exe "[parity]" # just the JS-vs-C++ golden checks
```

| Test file | Ported from | Checks |
|-----------|-------------|--------|
| `jsmath_test.cpp` | (new) | `Math.round`, `Math.imul`, `Math.exp` (bit-identical to V8 on 4,008 inputs), mulberry32, `toFixed`, `toString`, `en-IN` grouping |
| `skills_test.cpp` | `skills.test.js` | ratings are the weighted mix of sub-skills; skill ranges; speeds; drop chance |
| `ipl_test.cpp` | `ipl.test.js` | overseas limits, Impact Player rules, roster shape |
| `engine_test.cpp` | `engine.test.js` | 600 matches reconcile; determinism; user decisions; feasibility; changing the bowler; short-handed sides |
| `session_test.cpp` | (new) | MatchSession: the user makes the calls; Next ball waits on a pending call; bowler / batter picks, auto-bowl, plan, custom field, tempo, target and review reach the engine; full matches finish; Rematch; Impact Player window, suggestion, overseas limit, once per match |
| `parity_test.cpp` | (new) | League JSON identical; 400 seeded AI matches identical (scores, commentary, every ball, final RNG state, win probability); 40 scripted user-captained matches identical |
| `flow_test.cpp` | `flow.test.js` | A season, the board's review, contracts, draft, auction and season 2 leave the JS game's exact state; four AI off-seasons (a mega auction among them) match JS |
| `json_test.cpp` | (new) | JSON written byte for byte like `JSON.stringify`; escapes and numbers; malformed input refused; the diff finds differences; JS saves read back and write out the same |
| `world_test.cpp` | `world`, `fitness`, `ageing`, `finance`, `club` tests | World from the roster; an off-season; ten seasons stay playable; fitness and injuries; ageing curves; money and bid steps; retention; the auction and RTM; the board; the club business (12 cases) |
| `career_test.cpp` | `development`, `career`, `season`, `editor` tests | Training; development caps and position-aware judging; career stats reconcile with 60 scorecards; season format, playoffs, NRR; editor; new players and the club captain; milestones and achievements |
| `game_test.cpp` | `lineup_and_settings`, board-through-the-store, create-player store tests | `Game` matches the JS store with the same seeds (setup screen too); quitting mid-season and relaunching changes nothing; saves round-trip, keep backups, survive a corrupt file, load browser saves; lineups and settings; friendlies and season fixtures through the listener; sacking and re-hiring; club captain and new players |

Not ported as unit tests: `field`, `fielding`, `tactics`, `mindset`, `matchups`, `weather`, `v2`,
`milestones` (details) and `trades` (their code is covered by the parity runs).

## Parity with the JS engine

Golden files in `tests/golden/` come from the JS engine. Regenerate them after any change to the
JS engine (Node 22+):

```sh
node tools/parity/dump_js.mjs league        > tests/golden/league.json
node tools/parity/dump_js.mjs matches 1 400 > tests/golden/matches.json
node tools/parity/dump_js.mjs userflow 1 40 > tests/golden/userflow.json
node tools/parity/gen_roster.mjs            > src/engine/roster_data.cpp
node tools/parity/flow_js.mjs RJR 7         > tests/golden/flow.json        # the real JS store
node tools/parity/world_js.mjs 4            > tests/golden/offseasons.json
```

`flow_js.mjs` drives the JS game's Redux store with `Math.random` and `Date.now` stubbed (only the
game's own draws advance the stub; Redux's internal ids get a constant), so the C++ `Game` can
replay the same run. Any team and seed can be checked without a golden file:

```sh
node tools/parity/flow_js.mjs MUI 11 > mui.json
build/mingw-release/t20coach_dump.exe gamecheck mui.json
```

Balance, same matches in both engines (output must be identical):

```sh
node tools/parity/balance_js.mjs 3000 --seed-base 2026 > js.txt
build/mingw-release/t20coach_balance.exe 3000 --seed-base 2026 > cpp.txt
diff js.txt cpp.txt
```

If a replay ever diverges, `balllog SEED` from both tools gives the RNG state after every ball;
the first differing line is where to look.

What makes exact parity work (see `docs/cpp-migration/05-engine-port-guide.md`):
- `js_exp` is fdlibm's `exp` as V8 ships it (including `exp(1) === Math.E`); `x ** 0.5` is `sqrt`.
- `-ffp-contract=off`, no fast-math: every double operation happens in the JS order.
- Reading a skill a player doesn't have gives NaN, like `undefined` in JS arithmetic.
- `OrderedMap` iterates in insertion order, like JS objects; sorts are stable.
- Expressions never contain two random draws (C++ leaves their order unspecified).
