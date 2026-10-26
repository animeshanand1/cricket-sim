# 03: Target C++ architecture

## 1. Tech stack

| Concern | Choice | Why |
|---------|--------|-----|
| Language | **C++20** | `std::optional`, `std::variant`, designated initialisers, `std::span`, `<ranges>`, `std::jthread` |
| Window, input, timing, file paths | **SDL3** | Mature, one dependency, gives `SDL_GetPrefPath` for saves, works on Windows/macOS/Linux if you ever port |
| Rendering | **OpenGL 3.3** (through the ImGui backend) | Simple, everywhere |
| UI | **Dear ImGui** (with the `freetype` feature for crisp text) | Immediate mode: draw from state every frame, like React re-rendering. Tables (sortable), tabs, combos, sliders, modals, child windows built in |
| Charts | **ImPlot** | Worm, Manhattan, win chance, table-position chart |
| SVG art | **nanosvg** (+ `nanosvgrast`) | The icons, team shields and stadium art are SVG today. Rasterise once at start-up into textures |
| JSON | **nlohmann/json** | Save files keep the browser's schema |
| Tests | **Catch2 v3** | Port the 28 vitest suites |
| Build | **CMake ≥ 3.25 + vcpkg (manifest mode)** | One command builds everything |
| Compiler | **MSVC (Visual Studio 2022)**, static runtime | A single exe with no DLLs |
| Fonts | **Barlow** + **Barlow Condensed** (SIL Open Font License) | Same look as the web game; embedded in the exe |

### Alternatives considered

| Option | Verdict |
|--------|---------|
| raylib + raygui | Lovely for 2D games; too thin for this many tables, forms and scroll areas |
| Qt 6 (Widgets or QML) | Best native look, but a heavy dependency, larger download, LGPL deployment rules |
| SFML + TGUI | Workable, smaller ecosystem for tables and charts |
| Unreal / Unity | Not C++-first for UI this dense; huge builds |

If you swap ImGui for something else, only the `ui/` layer changes. The engine, state and saves are UI-agnostic.

## 2. Repository layout

The C++ project lives **next to** the JS project; nothing in `t20-coach/` is touched.

```
t20-coach-v1.8/
├─ t20-coach/                     # JS reference implementation (unchanged)
├─ docs/cpp-migration/            # these docs
└─ t20-coach-cpp/                 # the new C++ game
   ├─ CMakeLists.txt
   ├─ CMakePresets.json
   ├─ vcpkg.json
   ├─ assets/
   │  ├─ fonts/  Barlow-*.ttf, BarlowCondensed-*.ttf
   │  ├─ icons/  app.ico, icon.svg
   │  └─ svg/    hero.svg (stadium)
   ├─ cmake/embed_assets.cmake     # turns assets into C++ byte arrays
   ├─ src/
   │  ├─ main.cpp                  # SDL + ImGui init, main loop
   │  ├─ core/                     # small utilities shared by every layer
   │  │  ├─ ordered_map.hpp        # insertion-ordered map (JS object semantics)
   │  │  ├─ jsmath.hpp             # js_round, imul, toFixed, number formatting
   │  │  ├─ ids.hpp                # PlayerId, TeamId, enums
   │  │  └─ json_io.hpp            # to_json / from_json helpers
   │  ├─ engine/                   # pure game logic (no SDL, no ImGui)
   │  │  ├─ rng.{hpp,cpp}  rules  pitch  names  roster  skills  league
   │  │  ├─ battle  mindset  tactics  field  special  matchups  shots  weather
   │  │  ├─ commentary  fans  ai  match  winprob
   │  │  ├─ sides  career  development  fitness  records  popularity  milestones  achievements
   │  │  ├─ season  world  ageing  finance  auction  board  club  trades  editor
   │  ├─ state/
   │  │  ├─ game_state.hpp         # SetupState, UiState, SeasonState + engine types
   │  │  ├─ game.{hpp,cpp}         # the controller: actions + thunks + listener
   │  │  ├─ derived.{hpp,cpp}      # effectiveLeague, player lookup, caches
   │  │  ├─ jobs.{hpp,cpp}         # SimFixturesJob, WinProbJob
   │  │  ├─ save_manager.{hpp,cpp} # load/save/upgrade/import
   │  │  └─ settings.{hpp,cpp}     # view preferences (was t20coach.ui.*)
   │  └─ ui/
   │     ├─ app.{hpp,cpp}          # shell: nav bar, router, modal host, toasts
   │     ├─ theme.{hpp,cpp}        # colours, fonts, ImGui style
   │     ├─ icons.{hpp,cpp}        # SVG icon atlas
   │     ├─ anim.{hpp,cpp}         # tiny tween helpers
   │     ├─ widgets/               # TeamBadge, Avatar, Meter, Rating, Stepper, PageHeader,
   │     │                         # PlayerLink, Chip, Tabs, Panel, SortableTable, BallPip,
   │     │                         # FitnessTag, PhaseTag, KpiTile, Notice, ConfirmInline
   │     ├─ screens/               # one class per screen doc (S01..S21)
   │     ├─ match/                 # Scoreboard, MatchDock, CaptainPanel, Crease, ImpactPanel,
   │     │                         # FieldView, Commentary, FansMeter, Scorecard
   │     └─ charts/                # Worm, Manhattan, WinProbChart, PositionChart
   ├─ tools/
   │  ├─ balance.cpp               # = scripts/balance.mjs
   │  └─ behaviour.cpp             # = scripts/behaviour.mjs
   └─ tests/                       # Catch2 ports of tests/*.test.js
```

## 3. Layers and the rules between them

```
 ui/            draws GameState, calls Game methods.        May use: state, engine (read-only helpers), core
 state/         owns GameState, runs actions/thunks/jobs.   May use: engine, core
 engine/        pure rules and simulation.                  May use: core only
 core/          tiny utilities.                             May use: STL only
```

- `engine/` must compile and run headless (the balance tool and tests link only `engine` + `core`).
- `ui/` never mutates state directly; it calls `Game` methods. (Read-only engine helpers such as `bowlingOptions`, `weaknesses`, `evaluateTrade`, `projectGate` are fine to call from the UI, exactly as the React components do.)
- No globals except the `Game` instance owned by `App`.

Build targets:

| Target | Type | Links |
|--------|------|-------|
| `t20_core` | static lib | — |
| `t20_engine` | static lib | `t20_core` |
| `t20_state` | static lib | `t20_engine`, `nlohmann_json` |
| `T20Coach` | WIN32 executable | `t20_state`, SDL3, imgui, implot, nanosvg |
| `t20coach_balance` | console exe | `t20_engine` |
| `t20_tests` | console exe | `t20_state`, Catch2 |

## 4. JS → C++ file map

| JS | C++ |
|----|-----|
| `src/engine/<name>.js` | `src/engine/<name>.{hpp,cpp}`, namespace `t20::engine` |
| `src/store/setupSlice.js`, `matchSlice.js`, `careerSlice.js`, `seasonSlice.js`, `worldSlice.js`, `uiSlice.js` | `state/game_state.hpp` (data) + methods on `state/game.cpp` |
| `src/store/thunks.js` | methods on `Game` (`startFixture()`, `startSeason()`, …) |
| `src/store/store.js` listener | `Game::onMatchMaybeCompleted()` called after every match-mutating method |
| `src/store/store.js` subscriber | `SaveManager::markDirty()` + debounced save in `Game::update()` |
| `src/data.js`, `src/hooks.js` | `state/derived.{hpp,cpp}` |
| `src/App.jsx`, `main.jsx` | `ui/app.cpp`, `main.cpp` |
| `src/components/ui.jsx`, `common.jsx` | `ui/widgets/*`, `ui/icons.cpp` |
| `src/components/<Screen>.jsx` | `ui/screens/<Screen>.{hpp,cpp}` (see each screen doc) |
| `src/styles.css` | `ui/theme.cpp` (tokens) + widget drawing code |
| `scripts/balance.mjs` | `tools/balance.cpp` |
| `tests/*.test.js` | `tests/*_test.cpp` |
| `src/pwa.js`, `pwa/`, `InstallBanner.jsx`, `lazyScreens.js`, `artifact/` | dropped |

## 5. Main loop

```cpp
int main(int, char**) {
  App app;                       // creates SDL window, GL context, ImGui, ImPlot, theme, fonts, icons
  app.game().boot();             // F1: load saves / migrate / fresh world, validate XI
  while (app.running()) {
    app.pollEvents();            // SDL events → ImGui; keyboard shortcuts
    app.game().update(dt);       // jobs: season sims a few fixtures per frame, win-prob results,
                                 // match "Watch" timer, debounced autosave
    app.beginFrame();
    app.drawNavBar();
    app.drawCurrentScreen();     // router on setup.screen
    app.drawPlayerCardModal();   // if ui.cardPlayerId
    app.drawToasts();
    app.endFrame();              // render, swap; vsync on
  }
  app.game().saveNow();          // flush on exit
}
```

Idle frames: when nothing animates and no job runs, use `SDL_WaitEventTimeout(…, 100)` instead of spinning, so the game uses no CPU at rest.

## 6. The `Game` controller (replaces Redux)

```cpp
class Game {
public:
  void boot();
  const GameState& state() const;
  void update(double dt);                       // background jobs + autosave

  // navigation (setupSlice)
  void goTo(Screen s);  void openPlayers();  void closePlayers();
  // friendly setup
  void chooseTeam(TeamId);  void selectOpponent(TeamId);  void setPitch(PitchChoice);
  void autoPick();  void togglePlayer(PlayerId);  void swapPlayers(PlayerId, PlayerId);
  void movePlayer(PlayerId, int dir);  void setCaptain(PlayerId);  void setKeeper(PlayerId);
  void setTraining(int slot, std::optional<PlayerId>, DrillKey);  void clearTraining();
  void startFixture();  void rematch();  void newGame();
  // match (matchSlice) — each forwards to engine::match on state.match
  void callToss(Coin);  void chooseToss(TossDecision);  void nextBall();  void nextOver();
  void simInnings();  void simMatch();  void startChase();  void pickBowler(PlayerId);
  void changeBowler();  void setBowlingPlan(PlanKey);  void setField(FieldChoice);
  void setBatterTarget(PlayerId bowler, TargetMode);  void pickBatter(PlayerId);
  void setBatterTempo(PlayerId, Tempo);  void impactSub(int side, PlayerId out, PlayerId in);
  void setAutoBowl(bool);  void reviewCall(bool);  void setAutoReview(bool);
  // season, off-season, club, transfers, editor … (full list in doc 06)
private:
  void afterMatchMutation();                    // the listener (F7)
  GameState s_;  SaveManager saves_;  Jobs jobs_;  Revision rev_;
};
```

Every public method:
1. mutates `s_` (directly or through engine functions),
2. bumps the revision counter of the slices it touched (`rev_.world++` etc.),
3. calls `saves_.markDirty(slice)`,
4. if it can complete a match, calls `afterMatchMutation()`.

## 7. Derived data and caching

The React code memoises on object identity (`effectiveLeague(world, progress)` returns the same array until `world` or `progress` changes). C++ uses revision counters:

```cpp
struct Revision { uint64_t world = 0, career = 0, season = 0, setup = 0, match = 0; };

class Derived {                          // state/derived.cpp
  const League& effectiveLeague();       // rebuilt when rev.world or rev.career changed
  const Player* player(const PlayerId&); // every player incl. free agents and retired, developed
  const TeamMeta& teamOf(const PlayerId&);
};
```

Screens that compute heavy tables (`seasonStats`, `teamAnalytics`, `pointsTable`, `positionHistory`) cache their result keyed on `rev.season` (+ `rev.world` where names matter).

## 8. Threading

Keep the game single-threaded except for two jobs.

| Job | JS today | C++ |
|-----|----------|-----|
| **Season sims** (`simFixtures`) | async loop, yields every 2 games | `SimFixturesJob` stepped from `Game::update()`: sim 1–2 fixtures per frame on the main thread, update `busy`. Deterministic and simple; no locking. |
| **Win probability** (`winChanceRun`) | 4 sims per `setTimeout(0)` | `WinProbJob` on a `std::jthread`: take a **copy** of the match (no commentary, `userTeam = none`, `quiet = true`), run 100 sims with reseeded RNG, post the result back through a mutex-guarded slot read in `update()`. A newer checkpoint cancels the older job (`std::stop_token`). |
| Match "Watch" | `setTimeout(nextBall, ms)` | Timer accumulated in `update(dt)`; calls `nextBall()` when due, pauses on `awaiting`, wicket (if "stop at wickets") or innings end |

The engine itself has no shared mutable globals, so copying a `Match` and simming it on another thread is safe.

## 9. Error handling

- Engine functions that JS lets return `false` on illegal input (e.g. `setBowler` with a blocked bowler) return `bool` in C++. The UI disables such buttons anyway.
- `aiChooseBowler` throws when no legal bowler exists (should never happen); in C++ use `assert` in debug and fall back to the first non-keeper in release.
- Save files: never crash on a corrupt save. Log, keep a `.bak`, start fresh (exactly as the JS `try { … } catch { start fresh }`).

## 10. Platform notes

- Save folder: `SDL_GetPrefPath("T20Coach", "T20Coach")` → `%APPDATA%\T20Coach\T20Coach\` on Windows.
- High-DPI: enable `SDL_WINDOW_HIGH_PIXEL_DENSITY`; scale ImGui fonts and style by the display scale.
- Window: default 1440×900, minimum 1280×720, resizable, remembers size and position in `settings.json`.
