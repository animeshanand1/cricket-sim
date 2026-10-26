# S05: Match: live play

| | |
|---|---|
| **Step** | S05 (the heart of the game; split the work into 5a–5f below) |
| **JS source** | `MatchView.jsx`, `MatchDock.jsx`, `Scoreboard.jsx` (`BallChip`, `useMoment`), `FansMeter.jsx`, `WinProb.jsx` (`WinBar`), `Crease.jsx`, `Milestones.jsx` (`useChases`, `ChaseList`), `CaptainPanel.jsx` (`BowlerPicker`, `BatterPicker`, `Tempo`, `BatterTarget`, `BowlingPlan`, `FieldControl`, `ReviewCall`, `Fold`), `ImpactPanel.jsx`, `FieldView.jsx`, `Commentary.jsx` |
| **C++ files** | `ui/screens/match_screen.{hpp,cpp}`, `ui/match/{scoreboard,match_dock,crease,captain_panel,impact_panel,field_view,commentary,fans_meter}.cpp`, `state/jobs.cpp` (WinProbJob) |
| **Depends on** | S04, engine A3 |

| Sub-step | Build | Done when |
|----------|-------|-----------|
| 5a | Layout + scoreboard (no animation) + dock (Next ball / Next over / Sim) | You can play a whole match with the AI making your calls (auto-bowl on) |
| 5b | Captain panel: bowler and batter pickers, review call, tempo, target, plan, field, change bowler | Every decision of [02 F6](../02-process-flows.md#f6-user-decisions-during-a-match) works |
| 5c | Crease cards + Impact panel | |
| 5d | Commentary panel | |
| 5e | Field view | |
| 5f | Moments, pip animation, win bar (WinProbJob), fans meter, Watch mode, keyboard | |

## 1. Purpose

Play the match: watch the scoreboard, make captain's calls when asked, set tempo / plans / fields, and advance ball by ball, over by over, or sim.

## 2. Navigation

| In | Out |
|----|-----|
| Toss → decision · nav Match · Season → Resume your match | The match stays here until complete; then see [S06](S06-match-result.md). Nav links leave at any time (the match stays in memory and in `match.json`) |

## 3. Layout

```
┌─DOCK─┐  SEASON MATCH 23 · Wankade Stadium   Rajasthan Regals v Mumbay Islanders
│RJR   │  Flat deck · Mumbay Islanders won the toss and chose to bowl
│87/2  │ ┌──────────────────────────── SCOREBOARD (dark, full width) ───────────────────────────┐
│9.3 ov│ │ ▰[⛨] 1ST INNINGS                87/2   9.3OV  │ RUN RATE │ PARTNERSHIP │   [ SIX! ]   │
│need..│ │      RAJASTHAN REGALS                         │   9.16   │   41 (26)   │  ← moment    │
│      │ ├────────────────────────────────────────────────────────────────────────────────────┤
│[▶Next│ │ PROJECTED 178   THIS OVER (1)(•)(4)(6)(W)  12 runs   [MIDDLE OVERS] [New ball…] [Free hit] │
│ ball]│ ├────────────────────────────────────────────────────────────────────────────────────┤
│[⏩Next│ │ WIN CHANCE  RJR 58% ███████████████░░░░░░░░░ 42% MUI                                 │
│ over]│ │ [⛨] 18,340 ▲120  RJR fans · In full voice  ████████░░░░  [⛨] 9,870 ▼40 MUI fans · …  │
│[👁    │ │ 28,210 in the ground · 85% full                      Rajasthan Regals at home         │
│Watch]│ ├────────────────────────────────────────────────────────────────────────────────────┤
│Slow  │ │ ▤ Flat deck · pitch wear 22% · evening game, dew expected in the chase               │
│Med   │ └────────────────────────────────────────────────────────────────────────────────────┘
│Fast  │ ┌──── LEFT COLUMN ───────────────────────┐ ┌──── RIGHT COLUMN ─────────────────────┐
│☑stop │ │ CAPTAIN PANEL "Your call" (S05 §5)     │ │ FIELD VIEW (§8)                        │
│at wkt│ │ AT THE CREASE (§6)                     │ │ COMMENTARY (§9)                        │
│[⏭Sim │ │ IMPACT PLAYER (§7)                     │ │                                        │
│ inns]│ └────────────────────────────────────────┘ └────────────────────────────────────────┘
│[⏭Sim │ ┌──── [Scorecard] [Worm] [Manhattan] [Win chance]      [RJR] [MUI]  (S06) ────────────┐
│match]│ └────────────────────────────────────────────────────────────────────────────────────┘
│Keys: │
└──────┘
```

- **Match dock**: a vertical rail fixed to the left edge while `status == live`; content shifts right to make room.
- Two columns ≥ 980 px wide; one column below.
- The fixture line: `Season match {no}` or the playoff stage label, venue, team names; muted second line: pitch label · toss result.

## 4. Scoreboard (`Scoreboard.jsx`)

Always dark (board colours), radius 18, team colour of the **batting** side as an accent.

| Part | Content |
|------|---------|
| Team block | Badge 46 px, `1st innings` / `2nd innings` / `Super Over`, team name (condensed italic uppercase) |
| Score | `{runs}/{wickets}` giant, `{oversText} OV` |
| Stats | `Run rate` (2 dp); then either `Target` (or `DLS target`) + `Req. rate` (gold, `–` when complete) when chasing, or `Partnership runs (balls)` |
| Headline (gold) | Break: `Target {n}` or `DLS target {n} from {overs} overs` · Chasing: `Need {n} from {balls} ball(s)` · Super Over · First innings: `Projected {round(runs + crr·ballsLeft/6)}` after 12 balls, else `Early days` · Complete: result text |
| Over pips | `This over` (if the current over has balls) else `Last over`; one `BallPip` per ball; over runs |
| Tags | phase (`Powerplay` / `Middle overs` / `Death overs` / `Super Over`) unless complete; condition notes from `conditionsSummary` (dusty "Turning square"/"Gripping for spinners", green "Seaming around"/"Grass drying out", slow "Getting slower", "New ball swinging", "Heavy dew" with droplet icon, overcast note, DLS target note); `Free hit` (gold) when on |
| Win bar | see §10 (not before the toss) |
| Fans meter | see §10 |
| Conditions strip | pitch label · pitch wear % · dew note |
| Moment banner | see §10 |

## 5. Captain panel (`CaptainPanel.jsx`)

States in order of priority:

| State | Shows |
|-------|-------|
| `status == break` | **Innings break**: "{team} posted {r}/{w}. {chaser} need {target} to win (from {overs} overs (DLS)), a required rate of {target/overs}." Buttons: **Start the chase** (user chasing) / **Take the field** → `startChase()`; **Sim to result** → `simMatch()` |
| `status == complete` | Rematch / New fixture (friendly) or Back to season (fixture) — see S06 |
| live | "Your call" header + role pill (`You're batting` / `You're in the field`) + `DRS: n reviews left` (not in a Super Over) |

Live, then exactly one of these blocks at the anchor `#decision` (the dock's "jump to decision" scrolls here):

### 5.1 Review call (`awaiting == review`)
- Heading: batting: "{Striker} is given out lbw / caught behind!" · fielding: "Not out! The umpire turns down the lbw / caught behind appeal". Tag `{n} reviews left`.
- Text: "{Batter} is on {runs} ({balls})." + the player's read: sure batter "That was going down leg!" / "Never touched it!"; unsure "…isn't sure. He has a word with his partner and shrugs."; sure fielders "That was hitting middle!" / "He nicked it!"; unsure "…isn't sure, and {bowler} only half appealed."
- Hint: players are usually right when sure; a successful review is kept, a failed one lost.
- Buttons: **Review** (R) → `reviewCall(true)`; **Walk off** (batting) / **Accept, play on** (fielding) (A) → `reviewCall(false)`.
- Switch: "Let my vice-captain make review calls" → `setAutoReview`.

### 5.2 Bowler picker (`awaiting == bowler`)
- Heading "Who bowls over {n}?" + switch "Pick bowlers for me" (`setAutoBowl`).
- List of `bowlingOptions(match)` sorted: legal first, then by bowling rating. Each row (a button, disabled when not legal):
  - avatar, name, meta `{type short} · {kph} kph · {bowl style} · {O-M-R-W or 'yet to bowl'}` + green "Suits these conditions" (`specialistEdge.wicket ≥ 1.08`) or "Hard to hit now" (`boundary ≤ 0.93`)
  - head-to-head line per batter at the crease: `v {surname}: {runs} off {balls}, out once/twice/N times | not out | first meeting` + tag "Bowler on top" / "Batter on top"
  - right side: `{quota − bowled} left` or the reason (Quota done / Bowled last over / Would leave overs unbowlable) + rating.

### 5.3 Batter picker (`awaiting == batter`)
- Heading "Wicket! Who goes in next?"
- Vice-captain hint when `nextBatterCall` promotes someone: "Vice-captain: send in {name}. Promoted: a hitter for the last overs / a steady head to stop the collapse."
- One button per `yetToBat` player: avatar, name (+ "Suggested" tag), `{batStyle} · next in your order | no. {n} in your order`, head-to-head v the current bowler, Bat and Power ratings → `pickBatter(id)`.

### 5.4 Batting controls (user batting, nothing awaited)
- **Tempo per batter** (striker and non-striker): fieldset "{Name} · on strike", segmented `Auto | Defend | Careful | Balanced | Attack | All-out` → `setBatterTempo({ batterId, level })`. Help text per level: auto "Your batters read the match situation themselves." · 1 "Survive. Few boundaries, very few wickets." · 2 "Rotate strike and pick off bad balls." · 3 "Play on merit." · 4 "Look for the boundary. Wickets get more likely." · 5 "Swing at everything. Big overs or a collapse." + head-to-head v the bowler.
- **Fold "Both batters v {bowler}"** (summary = current mode): segmented `Normal | Target him | See him off` → `setBatterTarget({ bowlerId, mode })`; hint with his figures and "Their plan last ball: …".

### 5.5 Bowling controls (user fielding, nothing awaited)
- **Plan for {bowler}**: scouting line (weaknesses as red tags, strengths as green tags, or "No obvious weakness."), head-to-head lines, plan chips `Auto + plans for pace/spin`; a chip that hits a weakness gets a dot → `setBowlingPlan(key)`; hint = plan help + "Depends on his {skill} ({value})." + "Last ball hit his weakness."; predictability warning after 3+ balls of the same plan: "{batter} has had N balls of {plan} in a row. Keep going and he'll start waiting for it." / "He's reading it now… Mix it up."
- **Fold "Field"** (summary `{preset label} · {n} on the boundary`): chips `Auto + 6 presets` + **Customise** → `setField({ preset })`. Hint: preset help + "{close} close, {ring} in the ring, {deep} on the boundary; {off} on the off side." + powerplay pull-in note. Custom editor: three chip groups (Close catchers / Inside the circle / Boundary) of the 23 positions, toggle up to 9; live problems from `fieldProblems`; **Set this field** (disabled with problems) → `setField({ preset: 'custom', positions })`.
- Opponent captain line: "Their captain: **Shrewd**. Reads every batter and bowls to his weakness."
- **Change bowler** row when `canChangeBowler`: "{bowler} is marking out his run-up for over {n}." [Change bowler] → `changeBowler()`.
- Switch "Pick bowlers for me".
- Inline Sim innings / Sim match buttons.

## 6. At the crease (`Crease.jsx`, live only)

| Part | Content |
|------|---------|
| Header | "At the crease" · `Partnership {runs} ({balls})` |
| Batter card × 2 | strike mark (bat icon) on the striker; avatar; name link; `Left/Right-hand · {batStyle}`; big runs + `(balls)`; `4s · 6s · SR`; confidence `Meter` with `confidenceLabel`; chase list (milestones he is chasing) |
| Mind line | (striker) target icon, **{mindset label}**, "playing {tempo name}" + "(told: {instructed})" when they differ |
| Bowler card | avatar, `Bowling` / `Bowled last over`, name, `{type} · {kph} kph · {style}` + `Plan: {label}` (if not stock) + tag "Batter reading it"; figures `W-R`, `{overs} OV · ECON`; rhythm `Meter` with `rhythmLabel`; special delivery line: "**{special}** ready: he can produce it any ball" (gold) / "just bowled, needs an over to set it up again" / "needs rhythm 70 (now N)" + "· bowled N"; bowler's chase list |
| Last wicket | "Last wicket: **Name** 23 (18) at 87/2, over 9.3" |

Chase items (`playerChases`): reached (check, green), chase (target icon) "14 runs to 1,000 career runs", best "9 to beat his best score (87)", record "13 to beat the league record score (112)". Empty in a Super Over and once the match is recorded.

## 7. Impact Player panel (`ImpactPanel.jsx`, live or break)

| State | Shows |
|-------|-------|
| Used | "Impact Player used: {in} came in for {out} ({when})." + opponent note |
| Not available now | reason: "Impact Player already used" / "No substitutes named" / "Only at an innings break, the end of an over, or after a wicket" |
| Available | "Available now: one substitution from your 5 named substitutes." [Make a substitution] → expands: suggestion (`aiImpactPlan`) with "Use suggestion"; combos **Replace** (`impactOptions.outs`) and **Bring in** (`ins`); overseas check (> 4 → error); hint; **Bring him in** → `impactSub({ side, out, in })` |

Opponent note: "{team} used theirs: {in} for {out}."

## 8. Field view (`FieldView.jsx`)

Collapsible (`settings.fieldView`). Header tabs: `This over | Last 5 overs | Innings | Wagon wheel` (+ batter combo for the wagon wheel: `Name · runs (balls)`).

Canvas (virtual 420×420, scale to fit): see [08 §6](../08-ui-design-system.md#6-scoreboard-and-match-graphics-custom-drawing). Geometry:

```
C = 210, R = 168 (rope), BAT = (C, C + 30) (striker's end), inner circle r = 86
landing(dir, dist): unit vector u = (sin a, −cos a) for a = dir°; ray from BAT to the rope:
  off = BAT − C; b = off·u; c = |off|² − R²; t = −b + sqrt(b² − c); point = BAT + u·t·dist
```

- Balls with `dir` defined; skip dots/blocks with `dist < 0.2` (except wickets).
- Wickets: red ✕ at the landing point (at the batter if `dist == 0`).
- Saved shots: dashed line to the fielder + green ring; tooltip "A four cut off: 2 runs".
- Sixes: quadratic arc (control point offset ±0.18 perpendicular), gold, glow, end dot.
- Fours blue, 1–3 runs white-ish, width 2.6 for boundaries else 1.6.
- Fielders (switch "Show fielders (preset name)" → `settings.fielders`): 6-px dark dots with gold ring at `landing(±angle, dist)` (mirror for a left-hander); tooltip `{position}: {fielder name}`.
- OFF / LEG labels swap sides for a left-hander.
- Legend and stats strip: Bat runs, Fours, Sixes, Off side %, Cut off.
- Collapsed summary: "{mode}: {runs} bat runs · {n} fours · {n} sixes".

## 9. Commentary (`Commentary.jsx`)

- Off (`settings.commentaryOn == false`): "Commentary is off — the scoreboard and field view still show every ball." [Turn on].
- Collapsed (`settings.commentary == false`): one line with the latest non-info line.
- Open: header (minimise button), switch "Key moments only" (`commentaryKeyOnly`: wicket, four, six, milestone, drop, review, over, result), [Turn off], innings tabs (`RJR`, `MUI`, `RJR SO`).
- Feed newest first: over tag (not for over/info/result lines), badge (W red, 4 blue ring, 6 gold, • muted, + dashed, ★ purple, D red dashed, R), text.
- Paging: latest 20; "Showing the latest 20 of N." [Show 20 more] [Show all]; [Show fewer] when expanded. Render only visible lines (`ImGuiListClipper`).

## 10. Live extras

| Feature | Rule |
|---------|------|
| **Moment banner** | After a state change, look at the new commentary lines since last frame (same innings). If 1–14 new lines: pick the highest-ranked kind (result 5, wicket 4, milestone 3, six 2, four 1). A **career milestone** reached in this ball (`reachedInMatch` diff, not yet recorded, not in a Super Over) outranks all but the result: text e.g. "1,000 runs", sub "Career milestone · {name}". Otherwise text "Six!", "Four", "Wicket", the milestone line ("Fifty for X" → "Fifty for X"), or "Result"; sub = batter name parsed from "Bowler to Batter, …". Show 1.9 s. Skip when > 14 lines arrived (a sim) |
| **Win bar** | From `WinProbJob` ([03 §8](../03-target-cpp-architecture.md#8-threading)): checkpoint when the over ends, a wicket falls, at the break, at the end, or each ball when ≤ 18 balls are left in a chase. 100 sims (60 in a Super Over). Series 1 = side batting first. Hidden until the first result. Store points for the chart (S06) |
| **Fans meter** | `match.fans`: per side badge, count (`en-IN`), trend arrow vs the start of the over, `{id} fans · {mood label}`; two-colour bar; `{total} in the ground · {pct}% full` · "{team} at home" / "Neutral ground" |
| **Watch** | Dock toggle; speeds Slow 1400 ms / Med 700 / Fast 260 (saved `watchSpeed`); "Stop at wickets" (saved `watchStopWicket`, default on); pauses on any awaited decision, wicket (if on), innings end, match end |
| **Keyboard** | N / Space next ball, O next over, W watch, Esc pause, R / A on a review call (see [08 §9](../08-ui-design-system.md#9-keyboard)) |

## 11. Match dock (`MatchDock.jsx`, live only)

Score block `{team id} {runs}/{wkts}`, `{overs} ov`, `need {n} off {balls}`; then either the decision button ("Pick your bowler" / "Send in your next batter" / "Umpire's call: review it?" → scrolls to `#decision`) or **Next ball** / **Next over**; **Watch** / **Pause** (+ speed radio and stop-at-wickets); **Sim innings** / **Sim match** (stop watching first); "Keys: N ball · O over · W watch".

## 12. Data read

`state.match` only (plus `career.players`, `career.records`, `career.matchups`, `career.recorded` for chases, head-to-head and moments). Everything else is computed per frame with engine helpers: `bowlingOptions`, `bowlerQuota`, `phaseAt`, `specialistEdge`, `matchWear`, `dewActive`, `weaknesses`, `strengths`, `plansFor`, `planByKey`, `describeField`, `fieldProblems`, `presetByKey`, `impactOptions`, `aiImpactPlan`, `canChangeBowler`, `batterTempoOf`, `nextBatterCall`, `matchupsOf`, `headToHead`, `edgeVerdict`, `conditionsSummary`, `confidenceLabel`, `rhythmLabel`, `specialLabel`, `specialReady`, `speedKph`, `playerChases`, `matchCounts`.

Cache `matchupsOf(match)` per ball (it walks every ball of the match).

## 13. Acceptance checklist

- [ ] Play a whole match ball by ball: totals, pips, run rates, required rate and headline match the web for the same seeded fixture.
- [ ] Bowler picker blocks illegal bowlers with the right reason; picking works; "Pick bowlers for me" resolves a pending pick.
- [ ] Change bowler only before the first delivery of the over.
- [ ] Batter picker shows the vice-captain suggestion in the right situations.
- [ ] Review call: both texts, both buttons, R / A keys, auto-review switch; reviews left count down only on failed reviews.
- [ ] Tempo per batter, target/see off, plan chips (weakness dots, predictability warning), field presets and the custom field editor (9 fielders, powerplay limit) all work and change play.
- [ ] Crease cards: confidence and rhythm meters move; special delivery line states; chase items.
- [ ] Impact Player: only in the allowed windows, once, overseas check, AI suggestion.
- [ ] Field view: modes, wagon wheel per batter, left-hander mirroring, fielders toggle, saved-shot rings.
- [ ] Commentary: key-moments filter, paging, turn off/on, innings tabs, preferences remembered after restart.
- [ ] Moment banner on fours, sixes, wickets, fifties and career milestones; none during a sim.
- [ ] Win bar appears after the first over and moves sensibly; the UI never stutters while it computes.
- [ ] Watch mode at all three speeds; stops at wickets (when ticked) and decisions.
- [ ] Rain: delayed start, first-innings cut with DLS target, chase interrupted, abandoned/no result all display correctly.
- [ ] Super Over: board says Super Over, line-ups automatic, repeats on a tie.
