# S11: Season: player stats, team analytics, awards

| | |
|---|---|
| **Step** | S11 |
| **JS source** | `season/SeasonStats.jsx`, `season/TeamAnalytics.jsx` (`PositionChart`, `Records`), `season/Awards.jsx`, `SortableTable.jsx`; engine `season.js` (`seasonStats`, `teamAnalytics`, `positionHistory`, `seasonRecords`, `seasonAwards`, `MVP`) |
| **C++ files** | `ui/screens/season_tabs/{player_stats_tab,team_analytics_tab,awards_tab}.cpp`, `ui/widgets/sortable_table.cpp`, `ui/charts/position_chart.cpp` |
| **Depends on** | S09 |

---

## A. Player stats tab

Header: tabs `Batting | Bowling | Fielding | MVP` + combo `Team: All teams / each team`. Hint: "Top 10 after N matches. Your players are highlighted." Empty: "Season stats appear after the first match."

Every table is a `SortableTable` ([08 §5](../08-ui-design-system.md#sortabletable)); the name column shows avatar + name link + team name. Qualification for rate columns: **60 balls faced** (batting), **10 overs bowled** (bowling).

| Batting column | Notes |
|----------------|-------|
| M, Inns, NO | |
| Runs | default sort desc; tie-break strike rate |
| Balls | |
| HS | sorts by runs + 0.5 if not out; shows `87*` |
| Avg | 2 dp, qualified |
| SR | 2 dp, qualified |
| 50s, 100s, 4s, 6s | |
| Bdry % | share of runs in boundaries, 1 dp, qualified |

| Bowling column | Notes |
|----------------|-------|
| M | |
| Overs | sorts by balls, shows `oversText` |
| Wkts | default sort desc; tie-break economy (lower first) |
| Best | sorts `bestW·1000 − bestR`, shows `4/21` |
| Avg, Econ, SR | low is good; qualified (Avg and SR also need ≥ 1 wicket) |
| Dots, Dot % (qualified), 4w, 5w, Mdns | |
| Wd/Nb | low is good, qualified |

| Fielding column | |
|-----------------|-|
| M, Dis (catches + stumpings + run outs, default sort), Ct, St, RO, Drops (low is good) | |

| MVP column | |
|------------|-|
| M, Points (1 dp, default sort), Wkts, Dots, 4s, 6s, Ct/St | Hint: "MVP points: 3.5 per wicket, 1 per dot ball, 2.5 per four, 3.5 per six, 2.5 per catch or stumping." |

---

## B. Team analytics tab

1. **Table position through the season**: line chart of every team's position after each league game (y 1 at top → 10), the user's team in series 1, the others muted; a band for "Playoff places" (1–4); end labels with team ids; hover tooltip "After game N" + the whole table `pos. TEAM · pts`. Fewer than 2 games: hint.
2. **Team analytics** `SortableTable` (top 10 = all teams), default sort Win % desc, your team highlighted:

| Column | Value |
|--------|-------|
| Team | badge + name |
| P, Win % | |
| Won bat 1st / Won chasing | `W/P` (sorts by ratio) |
| Avg 1st inns | |
| RR, Econ (low good) | |
| PP RR (overs 1–6), Mid RR (7–15), Death RR (16–20) | |
| PP econ, Death econ (low good) | |
| 6s, 4s, Dot % faced (low good), Wkts taken | |
| Highest | `runs/wkts` |

3. **Season records**: four lists of 5: Highest totals (`team runs/wkts v XXX`), Highest scores (`Name runs(*) (balls) v XXX`), Best bowling (`Name w/r v XXX`), Lowest all-out totals.

---

## C. Awards tab

1. Champion banner (after the final): xl badge, "Season N champions", team name, "Runners-up: …".
2. Award cards (provisional until the final: hint "Awards are provisional until the final is played."):

| Award | Rule (`seasonAwards`) | Line |
|-------|----------------------|------|
| Orange Cap · most runs | max runs (+ SR/1000 tie-break) | `412 runs · SR 145.2` |
| Purple Cap · most wickets | max wkts − econ/100 | `21 wickets · econ 7.45` |
| Most Valuable Player | max MVP points | `213.5 points` |
| Emerging Player (23 or under) | max MVP points, age ≤ 23 | `98.0 MVP points` |
| Most sixes / Most fours | | `28 sixes` |
| Best strike rate (100+ balls) | | `SR 168.4` |
| Best economy (20+ overs) | balls ≥ 120 | `6.85 an over` |
| Most dot balls | | `112 dots` |
| Safest hands | catches + stumpings | `14 catches and stumpings` |

Each card: eyebrow, avatar 40 + name link, stat line, team name. Orange and purple cards tinted.

3. **Past seasons** table (from `season.history`, newest first): Season, Champions, Runners-up, Your finish (`3rd in league · champions`), Orange Cap (name + runs), Purple Cap (name + wickets).

---

## C++ notes

- `seasonStats` walks every fixture summary: cache it by `rev.season`. The name column sorts by the **current** (possibly edited) name, so its cache also depends on `rev.world`.
- Position chart: ImPlot with inverted y-axis (1 at the top), or custom lines.

## Acceptance checklist

- [ ] Every column sorts both ways; first click on a low-is-good column sorts ascending; rate columns rank only qualified players and say so in the footer.
- [ ] Team filter works on all four stat tabs; your players highlighted.
- [ ] Top 10 / Show all toggles.
- [ ] Team analytics numbers match the web; position chart tooltip shows the full table at that game.
- [ ] Awards match the web after the same season (including the emerging player age rule); past seasons table lists every archived season.
