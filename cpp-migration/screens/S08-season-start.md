# S08: Season: start

| | |
|---|---|
| **Step** | S08 |
| **JS source** | `src/components/season/SeasonHub.jsx` (`StartSeason`, `RULES`) |
| **C++ files** | `ui/screens/season_start_screen.{hpp,cpp}` (or a sub-view of the season screen) |
| **Depends on** | S06 (so a season fixture can be played) |

## 1. Purpose

Shown on the Season screen when no season is running: explains the IPL rules and lets the user choose a franchise and start season 1 (or the next season after an abandoned one).

## 2. Navigation

| In | Out |
|----|-----|
| Nav Season / Home "Play a season" when `season.season == null` | **Start the season** → `startSeason(team)` → Season hub overview ([S09](S09-season-hub-overview.md)) |

(After season 1, new seasons start automatically at the end of the off-season, F12 → F8, so this screen is mainly for the first season and after abandoning one.)

## 3. Wireframe

```
┌─────────────────────────── HERO (stadium art) ───────────────────────────┐
│ SEASON MODE                                                               │
│ THE  T20 LEAGUE                                                           │
│ Ten franchises under IPL rules. Two groups of five; 14 league games each… │
└───────────────────────────────────────────────────────────────────────────┘
┌──── PLAYING CONDITIONS ───────────────────────────────────────────────────┐
│ 🏆 Points. 2 for a win. Ties are settled by a Super Over…                  │
│ ☰ Table order. Points, then wins, then net run rate, then wickets per ball.│
│ ⇄ Impact Player. Name 5 substitutes; bring one in at…                      │
│ 👥 Overseas. No more than 4 overseas players on the field.                  │
│ 💧 Two-ball rule. In evening games with dew, a second ball comes in after… │
│ 🔔 Strategic timeouts. Fielding side after the 8th over, batting side…     │
└───────────────────────────────────────────────────────────────────────────┘
CHOOSE YOUR FRANCHISE
(10 team cards, same as Home)
[ 🏆 Start the season ]
```

## 4. Widgets

| Widget | Data | Interaction |
|--------|------|-------------|
| Hero | static | — |
| Rules list | 6 rows (icon, bold title, text) from `RULES` in `SeasonHub.jsx` | — |
| Team cards | `derived.league()`: badge, name, ground · pitch label, BAT/BWL | local selection (`team`, default `setup.userTeamId`) |
| Start the season | — | `startSeason(team)` |

## 5. What `startSeason` does

See [02 F8](../02-process-flows.md#f8-season-start) and [06 §5](../06-state-actions-controller.md#season). In short: club season starts (money in and out, plans active), fixtures generated, home pitch applied, line-up restored or auto-picked, the board sets a target, go to the hub.

## 6. Acceptance checklist

- [ ] Shown only when no season exists.
- [ ] The selection defaults to your current team and can be changed.
- [ ] Starting creates 70 league fixtures (14 per team) in two groups of five, sets a board target matching your squad rank, and opens the hub.
- [ ] The club account gets central rights and pays running costs at the start (Club → Accounts shows it).
