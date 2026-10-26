# T20 Coach: C++ migration docs

This folder is the blueprint for rebuilding **T20 Coach v2.2** (React + Redux, in `../../t20-coach/`) as a native **C++ desktop game** that ships as a single Windows `.exe`.

> **The JavaScript code is not deleted or changed.** It stays the reference implementation. Every C++ module and screen is ported *from* it and checked *against* it.

---

## How to use these docs

1. Read the **overview docs** (00–10) once, in order. They describe how the game works today and how the C++ version is laid out.
2. Then migrate **one step at a time** using the roadmap below. Each screen has its own doc in `screens/` with a wireframe, every widget, every action, the rules and an acceptance checklist.
3. Do not start a screen until the steps it depends on are done. A step is done when its checklist passes.

## Overview docs

| # | Doc | What it covers |
|---|-----|----------------|
| 01 | [Current architecture](01-current-architecture.md) | The JS app as it is: layers, files, module dependency graph, how data moves |
| 02 | [Process flows](02-process-flows.md) | Every flow, with diagrams: boot, friendly, match lifecycle, ball pipeline, season, off-season, auction, sacking, persistence |
| 03 | [Target C++ architecture](03-target-cpp-architecture.md) | Tech stack, folder layout, layers, JS → C++ file map, threading |
| 04 | [Data model](04-data-model.md) | Every state object (Player, Match, Innings, Season, World, Career, Club…) as C++ structs |
| 05 | [Engine port guide](05-engine-port-guide.md) | Module-by-module port notes, constants, and the determinism rules (RNG, key order, rounding, stable sort) |
| 06 | [State, actions and controller](06-state-actions-controller.md) | Every Redux action and thunk, and the C++ `Game` controller method that replaces it |
| 07 | [Persistence](07-persistence.md) | Save files, JSON schemas, version upgrades, importing browser saves |
| 08 | [UI design system](08-ui-design-system.md) | Colours, fonts, spacing, every shared widget (badge, avatar, meter, pips, tables…), animations |
| 09 | [Build and release the .exe](09-build-and-release-exe.md) | Toolchain, CMake + vcpkg, static linking, icon, installer, signing |
| 10 | [Testing and parity](10-testing-and-parity.md) | Porting the 28 test files, balance benchmarks, JS-vs-C++ parity checks |

## Screen docs (migration order)

| Step | Screen doc | JS source | Depends on |
|------|-----------|-----------|------------|
| S00 | [App shell and navigation](screens/S00-app-shell.md) | `App.jsx`, `main.jsx`, `ui.jsx` | A1–A6 |
| S01 | [Home (friendly setup)](screens/S01-home.md) | `Home.jsx`, `HeroArt.jsx` | S00 |
| S02 | [Playing XI](screens/S02-pick-xi.md) | `PickXI.jsx` | S01 |
| S03 | [Pre-match training](screens/S03-training.md) | `Training.jsx` | S02 |
| S04 | [Toss](screens/S04-toss.md) | `Toss.jsx`, `Milestones.jsx` | S03 |
| S05 | [Match: live play](screens/S05-match-live.md) | `MatchView.jsx`, `MatchDock.jsx`, `Scoreboard.jsx`, `Crease.jsx`, `CaptainPanel.jsx`, `ImpactPanel.jsx`, `FieldView.jsx`, `Commentary.jsx`, `FansMeter.jsx` | S04 |
| S06 | [Match: break, result and analysis](screens/S06-match-result.md) | `Scorecard.jsx`, `Worm.jsx`, `Manhattan.jsx`, `WinProb.jsx`, `DevelopmentReport.jsx`, `PopularityMeter.jsx` | S05 |
| S07 | [Player card (modal)](screens/S07-player-card.md) | `PlayerCard.jsx`, `SkillBreakdown.jsx` | S00 |
| S08 | [Season: start](screens/S08-season-start.md) | `season/SeasonHub.jsx` (`StartSeason`) | S06 |
| S09 | [Season hub: overview](screens/S09-season-hub-overview.md) | `season/SeasonHub.jsx`, `season/PointsTable.jsx`, `BoardPanels.jsx` | S08 |
| S10 | [Season: table, fixtures, playoffs](screens/S10-season-table-fixtures-playoffs.md) | `PointsTable.jsx`, `Fixtures.jsx`, `Playoffs.jsx` | S09 |
| S11 | [Season: player stats, team analytics, awards](screens/S11-season-stats-analytics-awards.md) | `SeasonStats.jsx`, `TeamAnalytics.jsx`, `Awards.jsx`, `SortableTable.jsx` | S09 |
| S12 | [Season: transfer room](screens/S12-transfer-room.md) | `season/TransferRoom.jsx` | S09 |
| S13 | [Off-season shell and contracts](screens/S13-offseason-contracts.md) | `offseason/OffseasonView.jsx`, `offseason/ContractsStage.jsx`, `OffSeasonReport.jsx` | S09 |
| S14 | [Off-season: retentions (mega years)](screens/S14-offseason-retention.md) | `offseason/RetentionStage.jsx` | S13 |
| S15 | [Off-season: youth draft](screens/S15-youth-draft.md) | `Draft.jsx` | S13 |
| S16 | [Off-season: auction](screens/S16-auction.md) | `offseason/AuctionStage.jsx` | S15 |
| S17 | [Sacked](screens/S17-sacked.md) | `SackedView.jsx` | S09 |
| S18 | [Club: board, contracts, medical](screens/S18-club-board-contracts-medical.md) | `ClubView.jsx`, `BoardPanels.jsx` | S09 |
| S19 | [Club: accounts, sponsors, ground, youth](screens/S19-club-business.md) | `club/ClubBusiness.jsx` | S18 |
| S20 | [Players: squads, records, hall of fame, achievements](screens/S20-players.md) | `Players.jsx`, `RecordsBook.jsx`, `Achievements.jsx` | S07 |
| S21 | [Player editor](screens/S21-player-editor.md) | `PlayerEditor.jsx` | S07 |

## The step-by-step roadmap

Work top to bottom. Each step ends with something you can run.

### Phase A: foundations (no screens yet)

| Step | Deliverable | Docs | Done when |
|------|-------------|------|-----------|
| **A1** | Empty C++ project that builds `T20Coach.exe` and opens a dark window with ImGui | 03, 09 | Double-click the exe on a clean Windows PC and the window opens |
| **A2** | Core engine: `rng`, `rules`, `pitch`, `roster`, `skills`, `league`, `names` | 04, 05 | `generateLeague()` in C++ produces the same 160 players and skills as JS (JSON diff = 0) |
| **A3** | Match engine: `battle`, `mindset`, `tactics`, `field`, `shots`, `special`, `matchups`, `commentary`, `weather`, `fans`, `ai`, `match`, `winprob` | 05, 10 | Headless `t20coach_balance.exe 3000` matches the JS balance benchmarks within tolerance; all invariants pass |
| **A4** | World and career engine: `career`, `development`, `records`, `popularity`, `milestones`, `achievements`, `season`, `sides`, `fitness`, `ageing`, `world`, `finance`, `auction`, `board`, `club`, `trades`, `editor` | 05, 10 | Headless "flow" test plays a season, the off-season and starts season 2 |
| **A5** | `GameState`, `Game` controller (replaces Redux + thunks + listener) and the save system | 06, 07 | Headless flow test runs through `Game`; quitting and relaunching restores the career |
| **A6** | Theme, fonts, icons and the shared widget kit | 08 | A widget gallery screen shows every widget in the design system |

### Phase B: screens, one at a time

Do S00 → S21 in the order of the table above. The first playable milestone is **S06**: a full friendly match, captained ball by ball. The second is **S12**: a full season. The third is **S16**: a full off-season into season 2.

### Phase C: release

| Step | Deliverable | Docs |
|------|-------------|------|
| **C1** | Parity QA pass over every screen checklist | 10 |
| **C2** | Release build, icon, version info, installer (or zip), optional code signing | 09 |

### Progress

| Step | State (24 Oct 2026) |
|------|---------------------|
| A2 | **Done.** `generateLeague()` JSON is byte-identical to JS |
| A3 | **Done.** 400 seeded AI matches and 40 scripted user-captained matches replay exactly (scores, every ball, commentary, final RNG state); `engine.test.js`, `skills.test.js`, `ipl.test.js` ported and passing; balance benchmark identical on shared seeds and within tolerance on random seeds |
| A1 | **Done.** `t20coach.exe`: SDL3 3.2.10 (static) + Dear ImGui 1.91.9b on SDL_Renderer, Barlow fonts embedded, night-broadcast theme; `--screenshot` flag for visual checks. UI types now reuse the engine's (`core/types.hpp` includes `engine/types.hpp`) |
| S05 | **5a + 5b + 5c done** (+ parts of 5d/5f). 5a: Match screen wired to the engine through `app/MatchSession` (scoreboard, dock, crease, commentary, scorecard, win chance, fans, moments). 5b: `ui/match/captain_panel.cpp` — DRS review call, bowler and batter pickers, tempo per batter, target fold, bowling plan with scouting and predictability warning, field presets and custom editor, change bowler, auto-bowl / auto-review; dock shows the pending call, Watch waits on calls and can stop at wickets, keys N/O/W/Esc/R/A. One deliberate difference from the JS: with no plan set the plan chips show **Auto** (what the engine does, `match.js:509`); the JS UI highlights Stock. 5c: `ui/match/crease.cpp` (batter cards, mind line, bowler card with plan / "Batter reading it" / rhythm / special delivery, last wicket) and `ui/match/impact_panel.cpp` (window rules, suggestion, Replace / Bring in dropdowns, overseas check, used state, opponent note). The crease chase lists wait for the career (A4: `milestones.js` needs career stats and records; in a friendly they are always empty). Next: 5e field view, 5d full commentary panel, 5f watch polish |
| A4 | **Done (24 Oct 2026).** All 17 world and career modules ported (`src/engine/`: career, development, records, popularity, milestones, achievements, season, sides, fitness, ageing, world, finance, auction, board, club, trades, editor, plus `league_view` for data.js), with today's create-player and club-captain feature. Parity: the JS flow test's path (season 1 to a champion, the board's review, contracts, draft, auction, season 2) driven through the **real JS Redux store** leaves exactly the same world, career and season as the C++ engine (`tests/golden/flow.json`, compared field by field), and so do five more teams and seeds checked by hand and four AI off-seasons with a mega auction (`offseasons.json`). Ports of the world, fitness, ageing, finance, club, development, career, season and editor tests pass |
| A5 | **Done (24 Oct 2026).** `src/app/game.{hpp,cpp}`: every slice reducer, thunk and the store's match-finished listener in one `Game` controller, with injectable seed sources. `src/app/save_manager`: saves in `%APPDATA%\T20Coach\T20Coach\saves\slot1\` in the browser's JSON (both ways: the JS game loads a C++ save and plays on), atomic writes with `.bak` copies, corrupt-file fallback, autosave and a log. `Game` replays the JS store's run exactly, setup screen and XI included; quitting mid-season and relaunching leaves the same career as playing straight through; the exe records every finished match and keeps it across launches. Not done: saving the match in progress (`match.json`) and `settings.json` (the browser saves neither) |
| A6, other screens | Not started. Of the UI widgets, sortable table, field view, modal, stepper and icons are still stubs (the avatar shows a role letter until icons exist) |

Toolchain actually used so far: MinGW-w64 GCC 16 (WinLibs, via winget) + CMake + Ninja, static linking. See `t20-coach-cpp/README.md`.

## Key decisions (recommended, change them here if you choose otherwise)

- **Language:** C++20.
- **Window, input, OpenGL context:** SDL3.
- **UI:** Dear ImGui (tables, tabs, sliders, modals) + ImPlot (charts), styled to match the web game's night-broadcast look. Custom 2D drawing (scoreboard, field view, badges) with `ImDrawList`.
- **SVG art** (icons, team shields, the stadium hero): rasterised at start-up with `nanosvg`.
- **Saves:** JSON with `nlohmann/json`, **the same schema as the browser saves**, so a web career can be imported.
- **Build:** CMake + vcpkg, MSVC, static runtime (`x64-windows-static`), so the exe needs no DLLs.
- **Tests:** Catch2.

Why this stack: T20 Coach is a dense management game (tables, forms, tabs, charts) with a little custom 2D art. Immediate-mode ImGui is the fastest way to build that much UI in C++, it redraws from state every frame (no data binding to maintain, much like React), and it compiles into one small exe. See [03](03-target-cpp-architecture.md) for alternatives.

## What is deliberately dropped

| Web feature | Why it goes | Desktop replacement |
|-------------|-------------|---------------------|
| PWA: service worker, manifest, install banner, update toast (`pwa.js`, `pwa/`, `InstallBanner.jsx`) | A desktop exe is already installed and offline | Version shown in an About box; new versions ship as a new exe |
| `claude.hot` snapshot (`main.jsx`) | Only for the claude.ai artifact host | Autosave of the match in progress (see 07) |
| Lazy-loaded screens (`lazyScreens.js`, `React.lazy`) | Everything is compiled in | None needed |
| `?screen=` URL shortcuts | No URL | Optional command-line flag `--screen season` |
| Phone layouts (pane switch on Playing XI, collapsed panels by default under 720px) | Desktop window | Two-column layouts; window minimum 1280×720 |
| Vercel / Netlify configs, `dist/`, `dist-artifact/` | Web hosting | — |
