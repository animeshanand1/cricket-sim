# S20: Players: squads, records book, hall of fame, achievements

| | |
|---|---|
| **Step** | S20 |
| **JS source** | `src/components/Players.jsx` (`Leaders`, `Players`), `RecordsBook.jsx` (`RecordsBook`, `HallOfFame`), `Achievements.jsx`, `OffSeasonReport.jsx`, `PopularityMeter.jsx`; engine `career.js` (`summarize`, `fmt`), `records.js` (`hallOfFame`), `achievements.js` (`achievementList`), `milestones.js` (`upcoming`, `milestoneText`) |
| **C++ files** | `ui/screens/players_screen.{hpp,cpp}`, `ui/screens/players/{squads_tab,records_tab,hall_of_fame_tab,achievements_tab}.cpp` |
| **Depends on** | S07 |

## 1. Purpose

Everything about players across the league and across time: every squad with career numbers, free agents and retired players, the all-time records book, the hall of fame, and the manager's achievements. Also hosts three maintenance actions: advance one year, restore all edits, reset career stats.

## 2. Navigation

| In | Out |
|----|-----|
| Nav Players (`openPlayers`, remembers where you came from) · after an off-season started here | **Back** → `closePlayers()` (returns to the previous screen) · **Advance one year** → off-season |

## 3. Layout

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ CAREER RECORDS   PLAYERS                                                  [‹ Back] │
│ Career numbers from every completed match in this browser: 412 matches so far…    │
└──────────────────────────────────────────────────────────────────────────────────┘
[👥 Squads] [📈 Records book] [★ Hall of fame] [🏆 Achievements]
```

---

## A. Squads tab (default)

```
[OFF-SEASON REPORT (collapsed)]
[POPULARITY METER]                                   (once a match has been played)
┌─ MOST RUNS ───────────────────────┐ ┌─ MOST WICKETS ─────────────────────┐
│ 1 (av) Name  RJR · avg 42.1 · SR 141  1,840 │ │ 1 (av) Name  CHK · best 5/19 · econ 7.1  88 │
└───────────────────────────────────┘ └───────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────────────────────┐
│ ([⛨] Chennai…) ([⛨] Mumbay…) … (Free agents (31)) (Retired players (54))           │
│ [+ New player]  (disabled: "The squad is full (25 players)." / "Finish the off-season first.") │
│ ✔ Name has joined Rajasthan Regals as club captain. [Open his card]   ← after creating │
│ RAJASTHAN REGALS SQUAD · 22 PLAYERS                                                │
│ Player                                         M  Runs  HS  Avg  SR   Wkts Best Econ Ct │
│ (av) Name [C] ✎   Created · Edited · Batter · 24 [PRIME] · Anchor / …   52 1840 112* 42.1 141.2 3 1/12 9.8 21 │
│ …                                                                                   │
└──────────────────────────────────────────────────────────────────────────────────┘
[Advance one year (2 passed)]  [Restore all edited players (3)]  [Reset career stats]   ← each with an inline confirm
```

| Widget | Rule |
|--------|------|
| Leaders | top 5 by career runs (`avg · SR` detail) and by wickets (tie: economy; `best · econ` detail); team name or "Free agent" |
| Team chips | 10 teams + **Free agents** (unattached, not retired, by rating) + **Retired players** (by retirement year, then rating); default your team |
| New player | 10 teams only (not Free agents / Retired). Opens a modal (team-coloured header: badge, `{team} · {n} of 25 players`, **New player**, ✕; Esc or backdrop closes) holding the editor in create mode (S21 §7). On success: close, show the notice with **Open his card** (`openCard(id)`) |
| Squad table | Player cell: avatar, name link + gold `C` tag (title "Club captain") for `clubCaptainOf(world, team)`, meta (`Created ·` for `origin == custom`, `Edited ·` in gold, role, age + PhaseTag or `retired season N, aged A`, styles), pencil button (not for retired) → `openEditor(id)`; columns M, Runs, HS, Avg, SR, Wkts, Best, Econ, Ct (catches + stumpings), `–` when none |
| Advance one year | confirm text: "Runs an off-season now: every player turns a year older, some veterans retire, academies bring in prospects and you pick in the youth draft…" → `startOffseason()` (then = players) |
| Restore all edited players | disabled with no edits; confirm → `allEditsRestored()` |
| Reset career stats | disabled with no matches; confirm: "This clears every player's career record, development, the records book, and your match achievements and milestones in this browser. Squads, ages and retirements stay. It can't be undone." → `resetCareer()` |

## B. Records book tab

1. **Franchise honours** (after a season has been archived): Team, Titles, Finals, Won in (`Season 2, Season 5`); sorted by titles then finals.
2. **Match records** (`career.records`, top 10 each, all matches incl. friendlies, not Super Overs): Highest scores (`runs* (balls, 4s, 6s) v XXX · when`), Best bowling (`w/r (overs)`), Fastest fifties / hundreds (`N balls (made R)`), Most sixes in an innings, Highest team totals, Lowest completed totals, Biggest wins by runs, Biggest wins by wickets (`(N balls left)`).
3. **All-time leaders** (top 10, from career stats): Most runs (avg, SR), Most wickets (best, econ), Most sixes, Best average (10+ innings), Best strike rate (200+ balls), Best economy (30+ overs, ascending), Most catches and stumpings, Player of the match awards.

## C. Hall of fame tab

Intro: inducted for a career here (1,200 runs, 50 wickets, 700 runs + 30 wickets, or 8 POTM awards) or as a legend (peak rating ≥ 85). Cards (`hallOfFame(world, career.players, summarize)`, quiet retirements excluded), sorted by number of reasons then peak: avatar 56, eyebrow "Legend of the game" / "Inducted" · `retired season N, aged A`, name link, `Role · last with {team} · peak rating P`, stats line (`M matches · R runs (avg) · W wickets` or "Retired before playing in this league") + reasons. Empty: "Nobody has been inducted yet. Players join when they retire."

## D. Achievements tab

1. **Your record** KPIs: Achievements `done/23`, Matches, Wins, Win rate, Best streak (`· now N`).
2. **On the field** (16 match achievements) and **Across the seasons** (7): card per achievement with trophy (done) or target (locked) icon, title, text, and either "Unlocked · {when} v {team}" or a progress bar `progress/goal` (for counted ones). From `achievementList({ manager, history, board })`.
3. **Career milestones**: "Your squad's next milestones" (`upcoming(players, squad, { reach: 3, perPlayer: 1 })`, 8) and "Latest milestones in the league" (`career.milestones`, 12: badge, name, milestone, `· when`).

## 4. Acceptance checklist

- [ ] Back returns to the screen you came from.
- [ ] Squad tables, free agents and retired lists match the web; pencil opens the editor.
- [ ] Leaders and all-time lists match; qualification thresholds respected.
- [ ] Records book lists match after the same matches; franchise honours after archived seasons.
- [ ] Hall of fame inductees and reasons match.
- [ ] Achievements: unlocked ones show when/v; counted ones show progress; season achievements read from history.
- [ ] Advance one year starts an off-season and returns to Players when it ends; restore-all and reset need confirmation and do exactly what the text says.
