# S10: Season: points table, fixtures, playoffs

| | |
|---|---|
| **Step** | S10 |
| **JS source** | `season/PointsTable.jsx`, `season/Fixtures.jsx`, `season/Playoffs.jsx`; engine `season.js` (`pointsTable`, `leagueComplete`, `STAGE_LABEL`) |
| **C++ files** | `ui/screens/season_tabs/{points_table_tab,fixtures_tab,playoffs_tab}.cpp` |
| **Depends on** | S09 |

Three tabs of the season hub.

---

## A. Points table tab

```
POINTS TABLE / FINAL LEAGUE TABLE
Pos  Team                        Grp  P  W  L  NR  Pts   NRR     Last 5
 1   [⛨] Chennai Supreme Kings Q  A  14 10  4  0   20  +0.812   W W L W W
 2   …
 4   … ───────────────────────── cut line ───────────────────────────────
 …   (your row highlighted)
Top four reach the playoffs. Ties in points are split by wins, then net run rate (a side bowled out
counts as facing its full 20 overs), then wickets taken per ball. Group A: CHK, … · Group B: …
```

| Column | Value |
|--------|-------|
| Pos | `row.pos` (bold) |
| Team | badge sm + name + `Q` tag when `pos ≤ 4` and the league is complete |
| Grp | `A`/`B` |
| P W L NR Pts | counts (Pts bold) |
| NRR | `+0.812` / `-1.205` (3 dp), green when ≥ 0, red below |
| Last 5 | form pips W/L/N |

Caption "Final league table" once 70 results are in, else "Points table". Compact mode (overview) hides Grp, NR, Last 5 and shows only the top 4 + you.

Table order (engine): points → wins → NRR → wickets per ball bowled → per-season seeded lots. NRR: an all-out innings counts its full quota of balls; no-result games are left out of NRR.

---

## B. Fixtures tab

```
(My team) (All games) (Results) (Upcoming)                         ← filter chips, default "My team"
#23  [⛨] Rajasthan Regals v [⛨] Mumbay Islanders   Mumbay Islanders won by 12 runs   ← red if you lost
#24  [⛨] … v …                                       Wankade Stadium · Flat deck       ← upcoming
Qualifier 1 …
   ▼ expanded summary (click a completed row):
   RJR 165/7 (20)   Name 67 (45)  Name 34* (22)  Name 20 (12)   ·   Name 3/28 (4)  Name 2/30 (4)
   MUI 166/5 (19.2) …
   Player of the match: Name · Super Over: RJR 11/1 · MUI 12/0 · Impact Players: X for Y · …
```

| Element | Rule |
|---------|------|
| Filters | mine (`home` or `away` is you), all, results (has result), upcoming (no result) |
| Row | `#{no}` or stage label; both teams with small badges; result text (green if you won, red if you lost, plain for others) or `venue · pitch` when upcoming; your rows highlighted |
| Expand | only completed rows; toggles one open row at a time |
| Summary per innings | `{team} {runs}/{wkts} ({overs})`; top 3 batters by runs `Name runs(*) (balls)`; top 2 bowlers by wickets then fewest runs `Name w/r (overs)` |
| Extras | POTM, Super Over lines `{team} {runs}/{wkts}`, Impact substitutions `{in} for {out}` |

---

## C. Playoffs tab

```
┌─ QUALIFIER 1 ─────────────┐ ┌─ ELIMINATOR ──────────────┐ ┌─ QUALIFIER 2 ─────────────┐ ┌─ FINAL ───────────────────┐
│ [⛨] Chennai…  (win green)  │ │ 3rd place (now RJR)        │ │ Loser of Qualifier 1        │ │ Winner of Qualifier 1      │
│ [⛨] Mumbay…   (lost grey)  │ │ 4th place (now KNR)        │ │ Winner of Eliminator        │ │ Winner of Qualifier 2      │
│ result or "1st v 2nd…"     │ │ "3rd v 4th. Loser out"     │ │ note                        │ │ note                       │
└───────────────────────────┘ └───────────────────────────┘ └───────────────────────────┘ └───────────────────────────┘
[⛨] Chennai Supreme Kings are the champions.
```

| Tie | Placeholders before teams are known | Note before it's played |
|-----|-------------------------------------|-------------------------|
| Qualifier 1 | `1st place (now XXX)`, `2nd place (now XXX)` (projection from the live table; blank once the league is complete) | "1st v 2nd. Winner goes straight to the final" |
| Eliminator | `3rd place (now …)`, `4th place (now …)` | "3rd v 4th. Loser is knocked out" |
| Qualifier 2 | "Loser of Qualifier 1", "Winner of Eliminator" | "Loser of Qualifier 1 v winner of the Eliminator" |
| Final | "Winner of Qualifier 1", "Winner of Qualifier 2" | "Winner of Qualifier 1 v winner of Qualifier 2" |

Team styling once played: winner green; loser of Q1 grey (it has a second chance); other losers grey and struck through. After the final: "{champion} are the champions."

---

## Acceptance checklist

- [ ] Table order and NRR match the web for the same results, including an all-out innings and a no result.
- [ ] Q tags appear only after the league is complete; the cut line sits under 4th.
- [ ] Fixtures filters work; summaries match the scorecards; Super Over and Impact lines show when relevant.
- [ ] Playoff placeholders show the live projection during the league, then real teams; styles for winners, Q1 loser and knocked-out teams are right.
