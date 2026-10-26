# 05: Engine port guide

**The JS engine is the specification.** Port each module line by line, keep the same function names (camelCase is fine in C++), the same constants and the same order of operations. This doc lists what is easy to get wrong and every constant worth double-checking.

Port order follows the dependency graph in [01 §5](01-current-architecture.md#5-engine-modules): A2 (foundations) → A3 (match) → A4 (world and career).

---

## 1. Determinism rules

A match is fully reproducible from its seed. The C++ engine must be too, and should give the **same** results as JS for the same seed (see [10](10-testing-and-parity.md) for how strict the parity check is). These are the traps.

### 1.1 The RNG (mulberry32)

```cpp
// core: JS integer helpers
inline int32_t  js_imul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
inline uint32_t u32(int32_t x) { return uint32_t(x); }        // x >>> 0

struct RandOut { double value; int32_t state; };

// rng.js nextRandom, bit for bit
inline RandOut nextRandom(int32_t state) {
  uint32_t t = uint32_t(state) + 0x6d2b79f5u;                 // (state + 0x6d2b79f5) | 0
  uint32_t r = uint32_t(js_imul(int32_t(t ^ (t >> 15)), int32_t(1u | t)));
  r = (r + uint32_t(js_imul(int32_t(r ^ (r >> 7)), int32_t(61u | r)))) ^ r;
  return { double(r ^ (r >> 14)) / 4294967296.0, int32_t(t) };
}

class Rng {                       // createRng(seed)
  int32_t s;
public:
  explicit Rng(int32_t seed) : s(seed) {}
  double operator()() { auto o = nextRandom(s); s = o.state; return o.value; }
  int32_t state() const { return s; }
};

inline double rand(Match& m) {    // rand(holder): the match's own stream
  auto o = nextRandom(m.rngState); m.rngState = o.state; return o.value;
}
```

- Seeds created in JS with `| 0` or `Math.floor(rng() * 2 ** 31)` are int32. Store seeds and `rngState` as `int32_t`.
- Every `hashRng(...parts)` in the engine (skills, world, finance, auction, club, ageing, fitness, weather, fans) hashes **strings**: `h = imul(h ^ charCode, 2654435761)` over each character of `String(part)`. In C++, convert numbers with `std::to_string(int)` (they are always integers there) and hash bytes (ids and salts are ASCII). `2654435761` does not fit in int32: JS `Math.imul` converts it with ToInt32, so use `int32_t(2654435761u)`.
- Several modules derive **independent** streams from a hash of the seed (weather forecast, fans, shot cosmetics, contracts, retirements). They never touch `match.rngState`. Keep them separate or the match stream shifts.

### 1.2 Object key order (critical)

JS objects iterate **integer-like keys first in ascending order, then string keys in insertion order**. The engine depends on it in `weighted()`:

```js
// rng.js
export function weighted(rng, weights) {
  let total = 0;
  for (const k in weights) total += weights[k];
  let x = rng() * total;
  for (const k in weights) { x -= weights[k]; if (x <= 0) return k; }
  return Object.keys(weights).pop();
}
```

| Weights object | Iteration order the C++ must use |
|----------------|----------------------------------|
| Ball outcome `BASE = { 0, 1, 2, 3, 4, 6, W }` | `0, 1, 2, 3, 4, 6, W` (integer keys first). After a no-ball deletes `W`: `0, 1, 2, 3, 4, 6` |
| Pace dismissals (`dismissalWeights`) | `caught, bowled, lbw, caughtKeeper, runout` |
| Spin dismissals | `caught, bowled, lbw, stumped, caughtKeeper, runout` |
| After `planDismissals` / `fieldDismissals` / `specialDismissals` | Same order (they copy then multiply existing keys) |

Implement outcome weights as a fixed array `[w0, w1, w2, w3, w4, w6, wW]` with a presence flag for `W`, and dismissal weights as a small `std::vector<std::pair<DismissalType,double>>` built in the order above.

Other places where map order decides a result (use `OrderedMap`, i.e. insertion-ordered):

| Place | Why order matters |
|-------|-------------------|
| `canFinishOvers(caps, …)` | `for (id in c)` picks the first bowler with the strictly largest quota |
| `playerOfTheMatch` | First player with the strictly highest score wins ties; score keys go in batting order, then bowling order |
| `world.players` (`Object.values`) | Free-agent lists, auction pool, `fillSquads`, `topUpFreeAgents`: sorts are stable, so ties keep creation order |
| `match.players`, `inn.batters`, `inn.bowling`, `career.players` | Same, and the JSON save order |

`OrderedMap<K,V>`: a `std::vector<std::pair<K,V>>` plus a `std::unordered_map<K,size_t>` index (or `tsl::ordered_map` from vcpkg). Erase keeps the order of the rest (JS `delete` does too).

### 1.3 Sorting

`Array.prototype.sort` is **stable** in JS. Always use `std::stable_sort`. The JS comparators return a number (`b.pts - a.pts || b.w - a.w || …`); port them as a function returning `double` and sort with `[&](a,b){ return cmp(a,b) < 0; }`. Remember `||` in JS falls through on `0` (and `NaN`).

### 1.4 Rounding and integer conversion

| JS | Behaviour | C++ |
|----|-----------|-----|
| `Math.round(x)` | Halves round **up** (towards +∞): `-2.5 → -2` | `inline double js_round(double x){ double r = std::round(x); return (x - r == 0.5) ? r + 1.0 : r; }` (`std::round` rounds halves away from zero) |
| `Math.floor`, `Math.ceil`, `Math.trunc` | Same | `std::floor`, `std::ceil`, `std::trunc` |
| `x \| 0` | ToInt32 (truncate, wrap mod 2³²) | `js_toint32(double)` |
| `x >>> 0`, `x >>> n` | ToUint32 then shift | cast to `uint32_t` first |
| `Math.imul(a,b)` | low 32 bits of the product | `js_imul` above |
| `Math.max(a,b)` with `NaN` | NaN | Avoid producing NaN; the engine never does on valid data |

### 1.5 Floating point

- Keep every intermediate as `double`, in the same order as JS (`reduce` is left to right).
- `Math.exp`, `Math.pow`, `Math.log` in V8 come from fdlibm. MSVC's `std::exp` can differ in the last bit. That changes an outcome only when a random draw lands within ~1e-16 of a boundary, which is practically never, so parity in [10](10-testing-and-parity.md) is checked statistically, plus exact replays of a few hundred seeds. If an exact replay ever diverges, port fdlibm's `exp`/`pow` (both are a few hundred lines, public domain).
- `Math.sqrt` is exact in both.

### 1.6 Formatting

| JS | Used for | C++ helper |
|----|----------|-----------|
| `n.toFixed(d)` | rates, averages | `fmt_fixed(n, d)`; note JS rounds the exact decimal value half-up, `printf` breaks exact ties to even. Only display differs, and only on exact ties |
| `n.toLocaleString('en-IN')` | fans, seats, ticket prices (`1,00,000` grouping) | `fmt_indian(int)` |
| `n.toLocaleString('en-US')` | milestones (`1,000`) | `fmt_thousands(int)` |
| `money(l)` | ₹ values in lakh: `>= 100 → "₹X.XX cr"` with trailing zeros trimmed, else `"₹N L"` | `money(double)` |
| `oversText(balls)` | `"12"`, `"12.3"` | port as is |
| `ballTag(balls)` | `"12.4"` (over.ball of the *next* ball) | port as is |

### 1.7 Non-deterministic inputs

Only seeds use `Math.random()` / `Date.now()`: `startFixture` (seed, and the pitch when "Surprise me"), `startSeason` (season seed), the balance script. In C++ use `std::random_device` and `std::chrono`. Everything else is seeded.

---

## 2. Foundations (step A2)

| Module | Exports to port | Notes |
|--------|-----------------|-------|
| `rng.js` | `nextRandom`, `createRng` (→ `Rng`), `rand`, `randInt`, `pick`, `weighted`, `bell`, `clamp` | `bell` = mean of 3 uniforms, then `Math.round` (use `js_round`) |
| `rules.js` | `OVERS=20`, `BALLS=120`, `MAX_OVERS_PER_BOWLER=4`, `bowlerQuota`, `inningsOvers`, `overIndex`, `phaseOf` (pp <6, mid <15, death), `over20`, `phaseAt`, `PHASE_LABEL`, `oversText`, `ballTag`, `runRate`, `requiredRate`, `oversBowledBy`, `canFinishOvers`, `bowlingOptions` | `bowlerQuota = min(4, ceil(maxBalls/6/5))`. Option reasons: `Quota done`, `Bowled last over`, `Would leave overs unbowlable` |
| `pitch.js` | `PITCHES`, `PITCH_KEYS` (`balanced, flat, green, dusty, slow`) | Par: 175, 198, 165, 158, 155 |
| `names.js` | `freshName(rng, used)` | 400 tries, then `"Player N"` |
| `roster.js` | `ROSTER`, `BAT_STYLE_CODE`, `BOWL_STYLE_CODE` | Generate the C++ table with a script |
| `skills.js` | skill definitions, `isPaceType`, `bowlStyleFor`, `battingOverall`, `bowlingOverall`, `fieldingOverall`, `speedKph`, `deriveSkills` | `deriveSkills` draws from `hashRng(id, seed, 0x5eed)` in this order: bat style pick (only if no rule matched) → 9 bat skills `n(7)` in `BAT_SKILLS` order → `bell` running for tailenders → bowl style pick (if bowl ≥ 40) → lfast 50% swing → 6 or 4 bowl skills `n(7)` → `speedBase` bell → 3 field `n(9)` + keeping. Then `fitToTarget` (4 passes) makes the weighted mix equal the headline rating |
| `league.js` | `LEAGUE_SEED=2014`, `TEAMS`, `MAX_OVERSEAS=4`, `BOWL_TYPES`, `ROLE_LABEL`, `isSpinner`, `generateLeague`, `teamRatings`, `pickBestXI`, `pickCaptain`, `IMPACT_SUBS=5`, `benchOf`, `battingOrder` | `CALIBRATE = { power: -8, bat: -2 }` applied in `fromRow`. `awarenessFor` uses its own hash stream. `pickBestXI`: keeper + 4 BAT + 3 AR + 3 BOWL, tiredness penalty below fitness 65, fill gaps keeping ≥ 5 bowling options (`bowl >= 50`), then injured if still short, then swap overseas down to 4 by smallest loss |

**A2 is done when** a C++ dump of `generateLeague()` (all 160 players, every field) is byte-identical JSON to the JS dump (see [10](10-testing-and-parity.md)).

---

## 3. Match engine (step A3)

### 3.1 The probability model (`match.js` `outcomeWeights`)

Baseline for a legal ball: `{ 0: 0.34, 1: 0.392, 2: 0.07, 3: 0.006, 4: 0.142, 6: 0.055, W: 0.0405 }`.

Multipliers, applied in this order:

| # | Factor | Formula (`c` = confidence term, `rh` = rhythm term, `a` = tempo − 3) |
|---|--------|------|
| 1 | Skill battle | `W *= exp(-1.15(B.survive − T.threat)/100)`; `0 *= exp(-1.0(B.rotate − T.control)/100)`; `1 *= exp(0.45(B.rotate − T.control)/100)`; `2 *= exp(0.5(B.running − 60)/100)`; `3 *= exp(1.0(B.running − 60)/100)`; `4 *= exp(1.1(B.hit4 − T.control)/100)`; `6 *= exp(1.1(B.hit6 − T.control)/100) · exp((B.lofted − 60)/50)` |
| 2 | Specialist edge | `W *= edge.wicket; 4 *= edge.boundary; 6 *= edge.boundary` |
| 3 | Ground fielding | `4 *= exp(-0.5(field.ground − 65)/100)`; `2 *= exp(-0.4(field.ground − 65)/100)` |
| 4 | New / long innings, nineties | `ballsFaced < 6`: W×1.25, 4×0.8, 6×0.7 · `ballsFaced > 40`: W×(1 + 0.028(ballsFaced − 40)) · runs 85–99: W×(1 + 0.7(1 − temperament/100)) |
| 5 | Confidence, `c = (conf − 50)/50` | W×exp(−0.35c), 0×exp(−0.1c), 4×exp(0.15c), 6×exp(0.2c) |
| 6 | Rhythm, `rh = (rhythm − 50)/50` | W×exp(0.12rh), 4×exp(−0.15rh), 6×exp(−0.15rh) |
| 7 | Phase | `PHASE_MODS`: pp `{0:1, 1:0.92, 2:0.9, 4:1.3, 6:0.95, W:0.92}`, mid `{0:1, 1:1.08, 2:1, 4:0.88, 6:0.95, W:0.85}`, death `{0:0.84, 1:1, 2:1.2, 4:1.12, 6:1.5, W:1.3}` |
| 8 | Tempo | 0×exp(−0.1a), 1×exp(−0.02a), 4×exp(0.18a), 6×exp(0.34a), W×exp((a<0 ? 0.38 : 0.3)·a) |
| 9 | Conditions | 4,6 × cond.boundary; 0 × cond.dot; W × cond.wicket |
| 10 | Spin by phase | spin in pp: 4×1.1, 6×1.05 · spin in mid: W×1.05, 0×1.04, 4×0.92 |
| 11 | Free hit | 6×1.8, 4×1.3, `0 += W×0.9`, W×0.05 |

Then in `playBall`: × plan effect (`W,0,1,4,6`), × field effect (`W,0,1,2,4,6`), × head-to-head (`W×exp(−0.14e)`, `4,6×exp(0.06e)`), × special delivery effect if one is bowled.

### 3.2 Wides and no-balls

```
wideP = 0.03 · exp((65 − accuracy)/45) · swingWides(pace & over<4: exp((swing−60)/150))
        · (death ? 1.3 : 1) · (spin ? 0.55 : 1) · cond.wide · exp(−0.4·rh) · plan.wide
nbP   = spin ? 0.002 : 0.006 · exp((60 − accuracy)/40)
x = r();  wide if x < wideP;  no-ball if x < wideP + nbP
```

Wide: 1 run (4% chance of 5). Spinner + 1-run wide + not free hit: 3%×(keeping/70) chance of a stumping off the wide. No-ball: one run off the bat drawn from the weights without W, +1 extra, free hit next ball, the batter's ball counts.

### 3.3 The random-draw order in `playBall`

For bit-identical replays the draws must happen in exactly this sequence (skip a line when its condition is false):

0. Between balls, at the start of each over when the AI picks the bowler (`aiChooseBowler`): one `rand()` per legal option, in option order.
1. `aiPlan` draws (only when the AI chooses the plan): change check `r()`; only if it changes, the weighted pick `r()`.
2. `x = r()` (wide / no-ball / legal).
3. Wide: runs (`r() < 0.04`); stumping check `r()` only if spin, 1 run and not a free hit. (The wide commentary line draws nothing.)
4. No-ball: `weighted(r, w)` with `W` removed. (The no-ball line draws nothing.)
5. Legal: special (`r() < specialChance`, drawn only if `specialReady` and not a free hit), then `weighted(r, w)` outcome.
6. Wicket: dismissal `weighted`; c&b `r() < 0.06` only if caught; caught → catch position `r()`, any-fielder `r()` only if nobody stands there, angle jitter `r()` only if a position was found; run out → ring fielder `r()` if the ring has fielders, any-fielder `r()` if not; catch dropped `r()` or stumping missed `r()`; a dropped catch → single `r()`.
7. Umpire (not a free hit, not a Super Over): lbw / caught-behind miss `r()`; on a clean dot: `u = r()`, then type `r()` for pace only.
8. Runs off the bat (no wicket, no drop, no umpire): 1 run → leg-bye `r()`; 4 → leg-bye `r()`; 0 → bye chance `r()`, and bye runs `r()` if it is a bye; any other run count → `shotDirection` (2 draws), `resolveShot` (0–2 draws), distance `r()` (not for a four or a saved shot), overthrow check `r()` (1 or 2 runs, not saved) and its size `r()`.
9. DRS: `sure = r()`; an AI side not sure → hunch `rand()`.
10. `finishLegalBall`: run out → runs `r()`, which batter `r()`; then the commentary line pick(s) `r()` (wicket, run and dot lines draw, and some dot lines draw up to 4 times through their short-circuit checks; bye, leg-bye, special-delivery and umpire lines draw nothing).

**Commentary consumes the match stream even when nobody reads it.** `ballText` / `shotText` are called *before* `addComment`, and `addComment` only skips the push when `match.quiet` is set (win-probability copies). So a C++ "fast sim" mode must still make the same `r()` calls as the line pickers, or the match diverges. Simplest: always build the text; it is cheap.

The JS is the source of truth; when in doubt, put a breakpoint in Node and log every `rand` call for one seed, then do the same in C++.

### 3.4 Module notes

| Module | Notes and constants |
|--------|---------------------|
| `battle.js` | `bowlerNow`: pace weights swing 0.3 (overs 0–2) / 0.15 (3–5) / 0.04; seam 0.12 + green 0.18·(1−0.7·wear); death 0.3 at the death. Spin turn weight `0.28 · (dusty ? 1.4 + 0.6·wear : 1 + 0.3·wear) · (dew ? 0.7 : 1)`. `dropChance(skill) = clamp(0.12·exp(−(skill−65)/22), 0.03, 0.35)`; `missedStumpingChance = clamp(0.1·exp(−(k−65)/20), 0.02, 0.3)` |
| `mindset.js` | `START_CONFIDENCE=45`. `batterFacedBall`: +0.15 survive, dot −1.0·(1.3 − temperament/100), four +3, six +4, run +1; clamp 5–100. `bowlerEvent`: wicket +9, dot +1.2, four −3, six −4, wide −2, noball −3, expensiveOver −4, maiden +5, drop −2; clamp 10–95. `matchWear` ×1.25 when hot, scaled by `sqrt(total/240)`. Dew from over 9 of the chase (`over >= 8`), halved after over 10 (two-ball rule). `computeMindset` situational parts × awareness factor `clamp((awareness−30)/60, −0.3, 1.1)`, confidence part, tempo clamp 1–5 |
| `tactics.js` | Plans, `weaknesses`/`strengths` thresholds, `planEffect`, `READ_AFTER=4`, fully read after 6 more balls, `readByBatter`, `planDismissals`, `CAPTAIN_STYLES`, `styleTempo` (±0.45), `styleBowlerBonus`, `aiPlan` change chance `0.08 (+0.1 shrewd, −0.04 cautious) + (seen ≥ 5 ? 0.55 : seen ≥ 3 ? 0.25 : 0)` |
| `field.js` | 23 `POSITIONS` (angle, dist, zone), 6 `PRESETS`, `maxDeep` (pp 2, else 5), `fieldProblems`, `legalize` (pull deep to ring via `INSIDE`, max 2 behind square leg), `fieldEffect`, `fieldDismissals`, `assignFielders` (close → deep → ring, best by zone score), `nearestFielder` (gaussian cover, reach 16° deep / 12° ring), `resolveShot` (four saved 0.42·cover deep / 0.2 ring; six caught on rope 0.05·cover; two saved 0.3; single saved 0.1), `catchPosition`, `aiField` |
| `special.js` | 14 `SPECIALS`, `BY_TYPE`, `specialOf` (editor choice, else best-skill), `SPECIAL_RHYTHM=70`, `SPECIAL_GAP=6`, `specialChance = 0.1 + 0.006(rhythm − 70)`, `specialEffect` (W ×1.1–4.5, boundaries ×0.3–1.15) |
| `matchups.js` | Row `[balls, runs, outs, fours, sixes]`, `MIN_BALLS=12`, full weight at 36, edge clamp ±1, kept if `|e| ≥ 0.05`; `edgeEffect = { W: exp(−0.14e), boundary: exp(0.06e) }`; verdict at ±0.25 |
| `shots.js` | `shotFor` is cosmetic: seeded by `ballKey(seed, innings, legalBalls, idxInOver)`, never touches the match stream. `shotDirection` uses the match stream. Angles: degrees from straight, + = off side for a right-hander; mirrored for left-handers when stored on the ball |
| `weather.js` | `forecast(seed, pitch)` from its own hash stream; rain chance overcast 22%, humid 12%, clear 3%, hot 1%; rain kinds pre 35% / first 30% / second 35% (30% of those abandon). DLS table `F = [1, .93, .85, .74, .62, .49, .37, .27, .16, .07]`, `B = 0.028`; `resources(oversLeft, wkts)` scaled so 20 overs, 0 down = 100 |
| `fans.js` | Display only; never draws from the match stream. Allocates nothing per ball in JS; keep it cheap in C++ too |
| `commentary.js` | Text pools; picks use the match stream (`choose(arr)` = `arr[floor(rng()·len)]`). Keep the pools identical or replays diverge |
| `ai.js` | `aiChooseBowler`: score = bowl + style bonus − 25 for part-timers + phase bonus + `(edge.wicket−1)·60 − (edge.boundary−1)·40` + match-up bonus − 14 to save strike bowlers for the death + economy/wickets so far + `rand()·6`. `aiAggression` by phase/chase. `nextBatterCall` (promote a hitter at the death, an anchor in a collapse; +8 threshold). `aiTossDecision` |
| `match.js` | The engine. Keep the structure: `createMatch`, toss, `startInnings`/`newInnings`, `prepareNext`, decisions, `playBall`, DRS (`REVIEWS=2`, umpire misses lbw 12% / caught-behind 10%, false out 0.6%, big appeal 2%), `finishLegalBall`, `afterBall`, `endInnings`, rain (`rainArrives`), `computeResult`, `playerOfTheMatch` (winner ×1.15), batch play, Super Over (max 40 innings), Impact Player, strategic timeouts (after over 8: fielding rhythm +3; after over 14: batters confidence +2) |
| `winprob.js` | Copy without commentary, `userTeam = none`, `quiet = true`; reseed each run `imul(state ^ 0x9e3779b9, 2654435761) + imul(i+1, 40503)`; ties and no-results count half |

**A3 is done when** `tools/balance.cpp` reproduces the JS benchmarks (see [10 §3](10-testing-and-parity.md#3-balance-benchmarks)) and the `engine.test.js` invariants pass in Catch2.

---

## 4. World and career (step A4)

| Module | Key rules and constants |
|--------|-------------------------|
| `sides.js` | `buildSide`: fatigue → start confidence from form → drill; captain = named or `pickCaptain`; bench = `benchOf`. `aiSide` adds `aiTrainingPlan`. `simFixture` builds both AI sides and sims with the fixture's seed |
| `career.js` | `recordMatch` order: skip if recorded (keep last 500 ids) → records book → milestone snapshot → per-player stats (Super Overs excluded) → match-ups → POTM → popularity → development (if a base function is given) → recent form (5) → milestones reached → manager achievements. `formOf`: last 3 innings, bonus `clamp(round((avg − 18)/2), −8, 12)` |
| `development.js` | 9 drills; `applyTraining` (+4 start confidence); `aiTrainingPlan`; expected runs/SR by batting position (`POS_RUNS`, `POS_SR`, `POS_BAT`), `expectedEcon = 8.5 − (bowl − 70)·0.085`; `STEP=0.5`, `CAP=0.08` (min 3 points), captain ×1.5 on good games + awareness 0.3 / temperament 0.2 every match |
| `fitness.js` | Workload: 5 base, +7 keeper, batting `balls·0.09 + running runs·0.12`, bowling per over fast 4 / fast-medium 3.5 / medium 3 / spin 2, × age factor `1 + 0.04·(age−30)`. Recovery 20 between games (−0.8 per year over 31). Injury chance `0.006 + load·0.0004`, ×1.7 pace, tired and age multipliers, cap 0.25. Fatigue below 70 |
| `records.js` | 9 lists of 10; `ballsTo` counts no-balls as balls faced; hall-of-fame thresholds 1,200 runs / 50 wkts / 700+30 / 8 POTM / peak 85 |
| `popularity.js` | Appeal points per match; start `clamp(18 + (max(bat,bowl) − 55)·1.3, 5, 65)`; display only |
| `milestones.js` | Ladders runs/wickets/sixes/fours/catches; "near" 40/3/3/5/2; appearances 50…300; log of 50; display only |
| `achievements.js` | 16 match + 7 season achievements; streak resets on a loss but not a no result |
| `season.js` | Groups by strength snake `A,B,B,A`; 70 league fixtures, `scheduleOrder` (Fisher–Yates then greedy rest); 70% home pitch; table sort; NRR with all-out = full overs; playoffs; `userPlayoffStatus`; stats, analytics (`MVP` 3.5/1/2.5/3.5/2.5), records, awards (emerging = age ≤ 23) |
| `ageing.js` | `CURVE` by age band (≤21, 22–24, 25–29, 30–32, 33–35, 36+); own rate 0.6–1.4×; pace bowling declines ×1.15, spin ×0.7; growth ×`max(0.2, (99 − skill)/40)` × growth room (mental skills at least 0.5) |
| `world.js` | `WORLD_VERSION=3`, squads 18–25, `ROLE_TARGET {WK 2, BAT 6, AR 5, BOWL 7}`, draft 2 rounds × 26 pool, 6 scout reports (+academy level), max 4 retirements per squad; `retireChance` table 33→3% … 39→55%, 40+→70%, 45→100%, ×1.5 weak, ×0.6 stars; `makePlayer` kinds prospect / entrant / journeyman; skill fractions carried in `skillFrac` |
| `finance.js` | All money in lakh. `PURSE=12000`, squad 18–25, 8 overseas, mega every 3rd year, retention slabs 1800/1400/1100/1800/1400, uncapped 400, `PRICE_CAP=2700`; `marketValue = clamp(round(30·exp((worth−55)/8.2)/5)·5, 30, 2700)`; `nextBid` increments; personalities; `teamValuation` |
| `auction.js` | Lots: 8 marquee capped, then Batters, All-rounders, Wicketkeepers, Fast bowlers, Spinners, Uncapped. Pool 170 (mega) / 60 (mini), age ≤ 38. AI bid step weighted by headroom from `hashRng('bid', year, lot, bids)`. Contract length = 3 (mega) or years until the next mega |
| `board.js` | `SACK_AT=15`; targets by rank; verdict deltas +32/+22/+12/−18/−32; match nudge ±1.5; job offers = 3 weakest other squads |
| `club.js` | `START_BANK=3000`, `CENTRAL_SHARE=2500`, `RUNNING_COSTS=1500`, `UPKEEP_PER_LEVEL=15`, `RED_INK_PENALTY=8`; facilities and costs; ticket tiers 800/1500/2500/4000; gate model; prizes 2000/1300/700/650; coaches and staff; `planYear` |
| `trades.js` | Window games 25–45, 2 trades, 2 each way, squad floor 15, premium 1.08, cash step 25, max cash back 1000; injury cover from 3 matches (AI from 5); 4 free agents per role kept |
| `editor.js` | `NAME_MAX=24`, `AGE_MIN=16`, `AGE_MAX=50` (the README says 45; the code says 50: follow the code), skills 1–99; `convertBowling`; `writeEdit` keeps `preEdit`; `undoEdit` adds the years since |

**A4 is done when** the C++ port of `flow.test.js` passes: start a season with RJR, sim it to a champion, review, run contracts → draft → auto auction → season 2 with ≥ 18 players.

---

## 5. Immer and copies

Redux reducers hand Immer drafts to the engine, which mutates them. In C++ the engine mutates `GameState` in place; that is the same thing. Three places copy on purpose, and C++ must copy too:

| Place | Copy |
|-------|------|
| `createMatch` | `players[id] = { ...p }`: the match holds its own player copies. Edits or development after the match starts do not affect it |
| `reviewDecision` | Copies `pendingBall` before finishing the ball (irrelevant in C++, but do not keep a reference into `match.pendingBall` after clearing it) |
| `winprob` `baseForSims` | Deep copy, commentary emptied |

And three engine functions return **new** objects instead of mutating: `applyTraining`, `applyFatigue`, `applyProgress` (and `applyEdit`, `applyAgeing`). Port them as functions returning a new `Player`.
