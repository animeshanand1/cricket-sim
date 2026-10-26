# S06: Match: innings break, result and analysis

| | |
|---|---|
| **Step** | S06 (first playable milestone: a full friendly) |
| **JS source** | `MatchView.jsx` (`ResultBanner`, detail tabs), `Scorecard.jsx`, `Worm.jsx`, `Manhattan.jsx`, `WinProb.jsx` (`WinProbChart`), `DevelopmentReport.jsx`, `PopularityMeter.jsx`, `Milestones.jsx` (`MatchMilestones`), `CaptainPanel.jsx` (complete and break states) |
| **C++ files** | `ui/match/{result_banner,scorecard,dev_report}.cpp`, `ui/charts/{worm,manhattan,win_prob_chart}.cpp`, `ui/widgets/popularity_meter.cpp` |
| **Depends on** | S05 |

## 1. Purpose

The parts of the match screen that are about the record rather than the next ball: the detail tabs (scorecard, worm, Manhattan, win chance), the innings break panel, and everything shown when the match is complete.

## 2. Layout when complete

```
┌──── RESULT BANNER (team colour of the winner) ──────────────────────────────────────────┐
│[⛨ xl] VICTORY / DEFEAT / TIED                                                           │
│       RAJASTHAN REGALS WON BY 6 WICKETS WITH 4 BALLS TO SPARE                            │
│       Player of the match: Name 78* (49), 1/22 (2 ov)       [🏆 Back to season]          │
│                                                     or      [⇄ Rematch] [New fixture]    │
└─────────────────────────────────────────────────────────────────────────────────────────┘
┌──── MILESTONES AND ACHIEVEMENTS ──┐  (only if any)
│ 🏆 ACHIEVEMENT UNLOCKED  Heart-stopper — Win by three runs or fewer…                     │
│ [⛨] Name  1,000 career runs                                                              │
└──────────────────────────────────────────────────────────────────────────────────────────┘
┌──── PLAYER DEVELOPMENT ─────────────────────────────── [RJR] [MUI] ─────────────────────┐
│ (av) Name [Captain] [Trained: Range hitting]  Bat 78* (49): Excellent   Batting 82 ▲ +0.4 │
│ …                                                                                        │
└──────────────────────────────────────────────────────────────────────────────────────────┘
┌──── POPULARITY METER (top 5 in the league) ─────────────────────────────────────────────┐
SCOREBOARD (final)
LEFT: captain panel buttons · "How the match unfolded" (Worm + Win probability)   RIGHT: field view, commentary
┌──── [Scorecard] [Worm] [Manhattan] [Win chance]          [RJR] [MUI] [RJR (SO)] ─────────┐
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

## 3. Result banner

| Element | Content |
|---------|---------|
| Tone | win (user won) / loss / tie (`winner == null`); accent = winner's colour |
| Badge | winner's, xl |
| Eyebrow | `Tied` / `Victory` / `Defeat` |
| Title | `match.result.text`, built with full team names (e.g. "Mumbay Islanders won by 23 runs", "Rajasthan Regals won by 6 wickets with 4 balls to spare", "… off the last ball", "Match tied. Rajasthan Regals won the Super Over" / "…won the second Super Over", "Match tied", "No result: rain has the final say", "… (DLS method, par 87)") |
| POTM | "Player of the match: **Name** {bat line per innings} {bowl line per innings}" |
| Buttons | Fixture: **Back to season** → `backToSeason()` · Friendly: **Rematch** → `rematch()` (back to Training, same XI) and **New fixture** → `newGame()` |

The captain panel in the left column shows the same buttons when complete.

## 4. Milestones and achievements (`MatchMilestones`)

Shown only for the match just recorded (`career.lastMilestones.matchId == match.id`, `career.lastUnlocked.matchId == match.id`):
- Achievement cards: trophy icon, "Achievement unlocked", title, text.
- Up to 8 milestones reached: badge, name, `milestoneText(type, value)` ("1,000 career runs", "50 matches", "First hundred").

## 5. Player development (`DevelopmentReport`)

From `career.lastReport` when `matchId == match.id`. Tabs for each team (default the user's side).
Per player row: avatar, name, chips `Captain`, `Trained: {drill}`; performance labels `Bat 78* (49): Excellent` / `Bowl 2/31 (4 ov): Good` / fielding `2 taken, 1 dropped`, coloured up (≥ 0.2), flat, down (≤ −0.2); rating changes `Batting 82 ▲ +0.4` (shown when |Δ| ≥ 0.05). Empty state: "Nobody from {team} did enough to be judged this match." Explanatory hint text from the JS.

Performance labels: ≥ 0.6 Excellent, ≥ 0.2 Good, > −0.2 Par, > −0.6 Poor, else Very poor.

## 6. Popularity meter

Top 5 of `career.popularity` among active players (`topPopular(pop, 5, not retired)`): rank, avatar, name, team badge + id, `{v}%` with trend `▲/▼ d`, bar filled in the team colour. Empty: "No games played yet. The fans pick their favourites once the cricket starts." (Shared widget; also used on the season overview and the Players page.)

## 7. Innings break panel

See [S05 §5](S05-match-live.md#5-captain-panel-captainpaneljsx). The scoreboard headline shows the target; the Impact panel stays available (AI sides have already used theirs).

## 8. Detail tabs (live and complete)

Tabs: **Scorecard** (default), **Worm**, **Manhattan**, **Win chance**. Scorecard has innings sub-tabs when there is more than one innings (`{team name}` + ` (SO)` for Super Overs); defaults to the current innings.

### Scorecard (`Scorecard.jsx`)

Batting table — caption `{team} batting (· Super Over n)`:

| Batter | Dismissal | R | B | 4s | 6s | SR |
|--------|-----------|---|---|----|----|----|
| name link + ` (c)` captain + ` †` keeper + `IP` tag | `howOut` (e.g. `c Smith b Jones`, `c †Keeper b Jones`, `lbw b X`, `st †K b X`, `run out (Y)`, `c & b X`), else `batting` (at the crease, innings live) or `not out` | runs (+`*` not out, bold) | balls | | | 1 dp or `–` |

Footer rows: `Extras` with `b, lb, wd, nb` and total; `Total` with overs and run rate, `{runs}/{wkts}`.
Notes: `Yet to bat` names · `Fall of wickets` `87-2 (Surname, 9.3)` · `Reviews` `RJR: lbw v Surname, overturned (9.4)`.
Bowling table — caption `{team} bowling`: Bowler (+IP), O, M, R, W (bold), Econ, Dots, Wd, Nb.

### Worm (`Worm.jsx`)

Cumulative runs by over for each (non-Super-Over) innings; series colours; wickets as dots on the line; target line labelled `Target {n}`; powerplay band (scaled for shortened games, label `Powerplay` or `PP`); x ticks every 5 (or 2/1 for short games) ending `{N} ov`; y ticks every 25 or 50; end labels `RJR 178/6` nudged apart; hover crosshair + tooltip "After N overs · RJR 87/2 · MUI 91/3". A collapsible "Over-by-over table".

### Manhattan (`Manhattan.jsx`)

Runs per over as paired bars (rounded tops) per innings; wicket dots stacked above each bar; y ticks every 5 or 10; hover band + tooltip "Over N · RJR 12 runs, 1 wkt".

### Win chance (`WinProbChart`)

The batting-first side's win probability over the match from the `WinProbJob` points: y from `{opp} 100%` (bottom) through 50% to `{team} 100%` (top); area above 50% in series 1, below in series 2; dashed "Innings break" line; x ticks Start, mid, `{half} ov`, mid, End; hover tooltip with the checkpoint label (`RJR 87/2 (9.3 ov)`) and both percentages; collapsible table. Fewer than 2 points: hint "Win probability builds up while you play ball by ball or over by over… Stretches you sim through are skipped."

## 9. C++ notes

- Charts: ImPlot (`PlotLine`, `PlotShaded` with a 0.5 reference, `PlotBars` with offsets, `PlotScatter` for wickets, `PlotInfLines` for target/break) themed to the tokens; or custom `ImDrawList` (they are small). Hover tooltips with ImPlot's `IsPlotHovered` + `GetPlotMousePos`.
- The win-chance store is per match (`matchKey = id|seed|teamA|teamB`) and must survive leaving and re-entering the screen.

## 10. Acceptance checklist

- [ ] Result banner text, tone and POTM line match the web for the same seeded match (tie → Super Over text; DLS wins; no result).
- [ ] Friendly: Rematch returns to Training with the same XI; New fixture returns Home and clears the match.
- [ ] Fixture: Back to season returns to the hub and the result is in the fixtures list.
- [ ] Milestones/achievements panel appears only when something was reached/unlocked.
- [ ] Development report shows both teams with correct labels and ▲/▼ values.
- [ ] Scorecard columns, dismissal texts, extras, FoW and reviews match the web; Super Over innings tabs appear.
- [ ] Worm, Manhattan and Win chance render with tooltips, including a rain-shortened match (axis and powerplay band scale).
- [ ] Career stats, records and popularity update exactly once per match (no double counting after leaving and returning).
