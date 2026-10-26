# S09: Season hub: overview

| | |
|---|---|
| **Step** | S09 (with S10–S12 this is the second milestone: a full season) |
| **JS source** | `season/SeasonHub.jsx` (`SeasonHub`, `Kpis`, `Overview`, `NextUp`, `Poster`, `TransferNudge`), `season/PointsTable.jsx` (compact), `BoardPanels.jsx` (`BoardPanel` compact, `InjuryList` compact), `OffSeasonReport.jsx` (collapsed), `PopularityMeter.jsx` |
| **C++ files** | `ui/screens/season_hub_screen.{hpp,cpp}`, `ui/widgets/{board_panel,injury_list,points_table,kpis}.cpp` |
| **Depends on** | S08 |

## 1. Purpose

The season's home. Shows where you stand and what is next, and drives the season forward: play the AI games before yours, captain or quick-sim your game, sim ahead, and start the next season when the final is done. Hosts the 8 season tabs.

## 2. Navigation

| In | Out |
|----|-----|
| Nav Season · after `startSeason` · Back to season from a match · after the off-season | Captain this match → `xi` · Resume your match → `toss`/`match` · Start season N+1 → `offseason` or `sacked` · tabs (S10–S12) |

## 3. Wireframe

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│[⛨] SEASON 3 · 23 OF 70 GAMES PLAYED                                                    │
│    RAJASTHAN REGALS                                                                    │
│    Sawai Mansing Stadium, Jaipur   (or "Chennai Supreme Kings won the title.")        │
└──────────────────────────────────────────────────────────────────────────────────────┘
[POSITION 2nd] [POINTS 10] [WON · LOST 5–2] [NET RUN RATE +0.432] [FORM W W L W W]   ← KPI tiles
[BOARD  Make the playoffs  ███████████▏░░░░░  62 · Happy]                              ← board strip
[OFF-SEASON 2 · A year older … (collapsed)                                    Show ✕]
[⌂ Overview] [☰ Points table] [📅 Fixtures] [🏆 Playoffs] [⇄ Transfers] [📈 Player stats] [👥 Team analytics] [★ Awards]
⚠ The trade window is open for 12 more league games. 1 injured player can be covered…  [⇄ Transfers]
┌──── NEXT UP ─────────────────────────────┐ ┌──── POINTS TABLE (compact: top 4 + you) ─┐
│ YOUR NEXT GAME · MATCH 24 OF 70   2 other games first │ Pos Team        P W L Pts NRR │
│   [⛨ xl]  v  [⛨ xl]                      │ …                                         │
│   Rajasthan   Mumbay                     │├────────────────────────────────────────┤
│   Home        Away                        ││ ORANGE CAP (av) Name 412 runs │ PURPLE CAP … │
│ ⛳ Wankade Stadium  ▤ Flat deck           │└────────────────────────────────────────┘
│ [▶ Play the 2 games before yours]         │ POPULARITY METER (top 5)                   │
│ [Sim to end of league] [Sim everything…]  │                                            │
├──── LATEST RESULTS ──────────────────────┤                                            │
│ Match 23  Mumbay Islanders won by 12 runs │                                            │
└──────────────────────────────────────────┘                                            
┌──── MEDICAL ROOM (only if anyone is hurt/tired) ───────────────────────────────────────┐
                                                               [Abandon season]
```

## 4. Header, KPIs, board

| Widget | Content |
|--------|---------|
| Page header | Team badge; eyebrow `Season {n} · {played} of {total} games played` (total = 70, or 74 once the league is complete); title team name; subtitle ground, city, or "{champion} won the title." |
| KPI tiles | Position (`–` before any game), Points, Won · lost, NRR (`+0.432`, green > 0, red < 0, `–` before any game), Form pips (last 5, "No games yet") |
| Board strip (`BoardPanel compact`) | eyebrow Board, target label (or "Between seasons"), confidence track 0–100 with a red marker at 15 (sack line), `{confidence} · {label}`; colour good ≥ 60, mid ≥ 35, bad below. Hidden if no manager team |
| Off-season report | collapsed by default ([S13](S13-offseason-contracts.md#6-off-season-report-offseasonreportjsx)) |

## 5. Tabs

`Overview` (this doc) · `Points table`, `Fixtures`, `Playoffs` ([S10](S10-season-table-fixtures-playoffs.md)) · `Transfers` ([S12](S12-transfer-room.md)) · `Player stats`, `Team analytics`, `Awards` ([S11](S11-season-stats-analytics-awards.md)). Local state `tab`, default overview.

## 6. Overview

### Transfer nudge (overview only)
Shown unless the season has a champion, when the window is open with trades left, or there are injured players who can be covered: "The trade window is open for N more league games. N injured player(s) can be covered by a free agent." [Transfers] switches tab.

### Next up (`NextUp`) — the season's driver

Let `next = nextFixture(season)` (first without a result), `mine = nextUserFixture(season)`, `aiFirst = next && mine && next.no < mine.no`, `pending = mine.no − next.no`, `waiting = !mine && userPlayoffStatus == 'waiting'`, `live` = a match in memory for this fixture that is not complete.

| Situation | Eyebrow / body | Buttons |
|-----------|----------------|---------|
| `next == null` (season over) | "Season complete" / "{champion} are the champions" + hint about the off-season | **Start season {n+1}** → `nextSeason()` |
| `mine` exists | "Your next game · Match {no} of 70" or stage label; hint "{pending} other games first" if `aiFirst`; **Poster** (home/away badges xl, names, Home/Away, venue, pitch) | `aiFirst`: **Play the {pending} games before yours** → `simFixtures({ label: 'Playing the other games' })` · else if `live`: **Resume your match** → `goTo(toss or match)` · else: **Captain this match** → `playNextFixture()` and **Quick sim** → `simFixtures({ includeUser: true, stopAfter: mine.no, label: 'Simulating your match' })` |
| `waiting` (won Q1 → final waits; lost Q1 → Q2 waits) | "Still in it · the Final / Qualifier 2 awaits" / "{team} are in the final" or "play Qualifier 2 next" + explanation | **Play the games before yours** → `simFixtures({ label: 'Playing the other playoff games' })` |
| otherwise | "Your season is over" / "{team} finished {pos}" (league done, outside top 4) or "are out" | — |
| always (unless season over) | | **Sim to end of league** (league not complete) → `simFixtures({ includeUser: true, stopAfter: 70, label: 'Simulating the league stage' })` · **Sim to end of season** (when eliminated or out of the top 4) / **Sim everything, including my games** → `simFixtures({ includeUser: true, label: 'Simulating the season' })` |

While a sim job runs (`season.busy`), the buttons are replaced by a spinner line: "{label}… {done} games done".

### Latest results
Last 5 completed fixtures, newest first: `Match {no}` or stage label + result text. Empty: "No games played yet."

### Right column
- Compact points table: rows with `pos ≤ 4` or your team (columns Pos, Team, P, W, L, Pts, NRR); cut line under 4th; your row highlighted.
- Caps: Orange Cap (most runs) and Purple Cap (most wickets, ties by economy) with avatar, name link, `{runs} runs` / `{wkts} wickets` (from `seasonStats`).
- Popularity meter (shared widget, S06 §6).

### Medical room (compact, overview only)
`InjuryList compact`: hidden if nobody is injured or tired. Injured: name, red injury label, "out for N more matches". Tired (< 70): name + FitnessTag. "Elsewhere:" up to 5 recent injuries at other clubs.

### Abandon season
Quiet button → inline confirm: "Abandon this season? Results and season stats are lost. Career stats and player development are kept." [Abandon] → `abandonSeason()` · [Keep playing].

## 7. C++ notes

- The sim job: `simFixtures` starts a `SimFixturesJob`; `Game::update()` sims 1–2 fixtures per frame and updates `busy`. Disable every season action while it runs (JS: `if (busy) return`). Keep the UI responsive (no blocking loop).
- Cache `pointsTable`, `seasonStats` by `rev.season`.

## 8. Acceptance checklist

- [ ] KPIs, board strip and compact table match the web after the same results.
- [ ] Next up handles: AI games first, your game (captain / quick sim), resume a live match, playoffs waiting after Q1 (won and lost), eliminated, season over.
- [ ] Quick sim stops right after your game; "Sim to end of league" stops after match 70; "Sim everything" runs to the champion.
- [ ] The busy spinner counts games and the UI stays responsive during a full-season sim.
- [ ] Start season N+1 runs the board review and goes to the off-season (or Sacked).
- [ ] Transfer nudge appears in the window or with long injuries; its button opens the Transfers tab.
- [ ] Abandon season needs confirmation and clears only the season.
