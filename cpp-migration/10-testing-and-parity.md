# 10: Testing and parity

The JS game is the oracle. The C++ port is done when it plays the same game: same rules, same numbers, same screens. Three kinds of checks get there.

| Check | What it proves | When |
|-------|----------------|------|
| **Unit tests** (Catch2 ports of the 28 vitest files) | Each rule works | Every build |
| **Golden dumps** (JS and C++ write the same JSON for the same input) | Bit-for-bit parity of generation and seeded matches | A2, A3, A4 |
| **Balance benchmarks** (thousands of AI matches) | The game *feels* the same: scores, wickets, phases | A3 and before each release |
| **Screen checklists** (in each screen doc) | The UI does what the web UI did | Each S-step |

Baseline today (JS, 21 Oct 2026): **28 test files, 145 tests, all passing** (`npx vitest run`, 11.6 s).

---

## 1. Porting the unit tests

Port the engine suites one-to-one into `t20-coach-cpp/tests/`. Keep the test names so failures map back to the JS file.

| JS file | Tests | C++ file | Step |
|---------|-------|----------|------|
| `skills.test.js` | weighted mix equals headline; right sub-skills per type, 1–99; weights sum to 100%; realistic kph; safe hands drop fewer | `skills_test.cpp` | A2 |
| `ipl.test.js` | 10 teams, 4–6 overseas per squad, ≤ 4 in an XI; Impact Player rules; roster 16 per squad in 2/5/4/5 shape, unique names and ids | `ipl_test.cpp` | A2–A3 |
| `engine.test.js` | 600 seeded matches reconcile (runs = batter runs + extras; over runs; bowler runs = runs − byes − leg byes; legal balls; wickets = outs = FoW; ≤ 4 overs; no consecutive overs; keeper never bowls; 6 legal balls an over); determinism; user decisions; feasibility; change bowler before the first ball; short-handed sides | `engine_test.cpp` | A3 |
| `field.test.js`, `fielding.test.js` | presets legal; powerplay and leg-side rules; field effects; sweepers cut off fours; safest hands in the slips; plan reading; seeing off a bowler; promoting a hitter; win probability never touches the match; DRS limits; waits for the user's review | `field_test.cpp`, `fielding_test.cpp` | A3 |
| `tactics.test.js` | weaknesses from skills; plans that hit a weakness; shrewd AI; user plan and batting targets | `tactics_test.cpp` | A3 |
| `mindset.test.js` | awareness after a collapse; chase maths; shortened innings; in the zone; confidence; wear, green tops, dew from over 9 of the chase | `mindset_test.cpp` | A3 |
| `matchups.test.js` | reconcile with scorecards; head-to-head reads; edges; AI prefers the bowler on top; per-batter tempo | `matchups_test.cpp` | A3 |
| `weather.test.js` | DLS table; forecast variety; shortened games; revised targets; abandoned chases | `weather_test.cpp` | A3 |
| `v2.test.js` | editor role/hand/type; special deliveries; fans meter never changes the cricket; captain; popularity | `v2_test.cpp` | A3–A4 |
| `development.test.js` | training boosts; 10 strong innings +3–6%; cap; position-aware judging | `development_test.cpp` | A4 |
| `career.test.js` | career stats reconcile with every scorecard | `career_test.cpp` | A4 |
| `milestones.test.js` | ladders, chases, bests, records, log, upcoming; achievements | `milestones_test.cpp` | A4 |
| `fitness.test.js` | drain and recovery; injury risk; injured out of the XI; realistic injury count | `fitness_test.cpp` | A4 |
| `ageing.test.js` | young improve, prime hold, veterans decline; pace ages faster than spin | `ageing_test.cpp` | A4 |
| `season.test.js` | groups and 70 games; full season to a champion; NRR all-out rule; user playoff flow never skipped | `season_test.cpp` | A4 |
| `finance.test.js` | increments and money format; prices; retention slabs; auction pauses for the user; mega auction RTM; board targets, sacking and re-hiring | `finance_test.cpp` | A4–A5 |
| `world.test.js` | world from roster; off-season; ten seasons stay playable; through the store | `world_test.cpp` | A4–A5 |
| `club.test.js` | club business rules (12 tests) | `club_test.cpp` | A4 |
| `trades.test.jsx` (engine parts) | trade window; valuations; moves; limits; injury cover; AI cover | `trades_test.cpp` | A4 |
| `editor.test.js` | validation; ratings recomputed; save through the store, clear development, restore | `editor_test.cpp` | A5 |
| `lineup_and_settings.test.js` | swaps; reorders; line-up kept across fixtures; match settings carried | `lineup_test.cpp` (through `Game`) | A5 |
| `flow.test.js` | season → review → off-season → season 2 | `flow_test.cpp` (through `Game`) | A5 |
| `club-view.test.jsx`, `match-ui.test.jsx`, `milestones-view.test.jsx`, `season-hub.test.jsx`, `v2-view.test.jsx`, `trades.test.jsx` (render parts) | UI renders | Become **manual checklists** in the screen docs (S05, S09, S12, S19, S21). Optional: ImGui Test Engine for automated UI tests |

Tests that simulate hundreds of matches are fast in C++ (expect 10–50× faster than JS).

## 2. Golden dumps (exact parity)

Put a small Node tool **in the C++ repo** (it only imports the JS engine; nothing in `t20-coach/` changes):

```
t20-coach-cpp/tools/parity/dump_js.mjs
```

```js
// node tools/parity/dump_js.mjs > golden/league.json  (and similar sub-commands)
import { generateLeague, pickBestXI, benchOf } from '../../../t20-coach/src/engine/league.js';
import { createMatch, simMatch } from '../../../t20-coach/src/engine/match.js';
const cmd = process.argv[2];
if (cmd === 'league') console.log(JSON.stringify(generateLeague()));
if (cmd === 'matches') {
  const league = generateLeague();
  const side = (t) => { const { ids, keeperId } = pickBestXI(t, 'balanced');
    const by = Object.fromEntries(t.squad.map((p) => [p.id, p]));
    return { id: t.id, name: t.name, color: t.color, captain: t.captain, keeperId,
             players: ids.map((i) => by[i]), bench: benchOf(t, ids).map((i) => by[i]) }; };
  const out = [];
  for (let seed = 1; seed <= 300; seed++) {
    const m = createMatch({ seed, pitch: 'balanced', userTeam: null,
      teams: [side(league[seed % 10]), side(league[(seed + 3) % 10])] });
    simMatch(m);
    out.push({ seed, result: m.result, potm: m.potm, innings: m.innings.map((i) => ({ runs: i.runs, wickets: i.wickets, balls: i.legalBalls })) , rng: m.rngState });
  }
  console.log(JSON.stringify(out));
}
```

The C++ `t20_tests` has matching dumps and compares:

| Golden | Must match | Step |
|--------|-----------|------|
| `league.json` | Exactly (every player, every skill) | A2 |
| `matches.json` (300 seeded AI matches) | Exactly: result text, POTM, every innings total, final `rngState`. If a handful differ, see [05 §1.5](05-engine-port-guide.md#15-floating-point) (fdlibm) | A3 |
| `ball-log.json` (seed 1, every `rand` call and every ball) | Exactly; the fastest way to find the first divergence | A3 debugging |
| `season.json` (seeded season, all fixtures simmed) | Exactly: table, champion | A4 |
| `offseason.json` (seeded world, one off-season, auto everything) | Exactly: squads, contracts | A4 |

For the season and world, make the random seed injectable (the JS uses `Date.now()` there; the dump tool passes a fixed seed through `createSeason({ seed })` and the world year-seeded functions).

## 3. Balance benchmarks

`tools/balance.cpp` is a port of `scripts/balance.mjs` (random seeds, random pitches, AI v AI, no match-up history). Baseline from the JS today:

```
node scripts/balance.mjs 3000      (21 Oct 2026, v2.2.0)

1st inns avg 163.7 (dry games 166.6, rain affected 240) | p10 109 p50 164 p90 218 | min 27 max 320
1st inns wickets avg 5.9 | all out 16.8% | 200+ 20.5%
Per innings: 4s 11.4 6s 7.5 extras 6.8 dot% 40.8
RPO by phase: PP 8.5 mid 7.7 death 9.4
Chasing side wins 51.2% | super overs 31
top  avg 34.7 SR 139.0
tail avg 5.0 SR 78.0
50s per match 1.6 | 100s per match 0.238
Impact Player used in 100.0% of team-matches
Bowler econ (4 overs) avg 7.8 | per match: 3+ wkt hauls 0.97, 4+ 0.294, 5+ 0.081
Per innings: byes 0.12 | boundaries saved 4.38 | overthrows 0.41 | reviews 0.78 (33.7% overturned)
Dismissals: caught 44.4%, caught behind 7.9%, bowled 21.3%, stumped 5.5%, lbw 14.3%, c&b 2.6%, run out 4.0%
```

Seeds are random, so run both tools with 10,000 matches before comparing. Tolerances:

| Metric | Tolerance |
|--------|-----------|
| 1st innings average | ± 2 runs |
| Wickets per 1st innings | ± 0.15 |
| Fours / sixes per innings | ± 0.4 |
| Dot % | ± 0.8 |
| Run rate by phase | ± 0.2 |
| Chasing side wins | ± 2% |
| Hundreds per match | ± 0.03 |
| Dismissal shares | ± 1.5% each |

Better still: give the balance tool a `--seed-base N` option in C++ and the same in a copy of the JS script inside `tools/parity/`; then both runs use identical seeds and must print identical numbers.

## 4. Behaviour checks

Port `scripts/behaviour.mjs` (awareness under a collapse, pitch wear, dew, conversion of 30s into 50s, skill-to-stat correlations). The README table under "Measured effect" lists the expected numbers (e.g. awareness 95 v 10: 0.56 v 0.83 wickets in the 12 balls after two quick wickets; chasing side wins 57% with dew v 50% without; catches dropped by catching skill 5% / 10% / 18%).

## 5. Screen checklists

Every screen doc ends with an **Acceptance checklist**. Before marking a step done:
1. Run the web game (`npm run dev` in `t20-coach/`) and the C++ game side by side.
2. Use the same save (import the browser save, see [07 §5](07-persistence.md#5-importing-a-browser-career-optional-but-cheap)).
3. Walk through every item. For seeded parts (a season fixture's match), the ball-by-ball result should match exactly.

## 6. Performance targets

| Operation | Target (C++) |
|-----------|-------------|
| One AI match, headless | < 2 ms |
| Sim a full 74-game season | < 0.5 s |
| Win probability (100 sims) | < 150 ms on the worker thread |
| Frame time, match screen | < 4 ms (60+ fps; idle uses `SDL_WaitEventTimeout`) |
| Cold start to Home | < 1 s |
| Save world (late career) | < 50 ms |
