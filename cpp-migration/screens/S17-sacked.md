# S17: Sacked

| | |
|---|---|
| **Step** | S17 |
| **JS source** | `src/components/SackedView.jsx`; engine `board.js` (`jobOffers`, `SACK_AT`), `finance.js` (`PERSONALITIES`), `league.js` (`teamRatings`) |
| **C++ files** | `ui/screens/sacked_screen.{hpp,cpp}` |
| **Depends on** | S09 |

## 1. Purpose

When the board's confidence ends a season at 15 or below, you are sacked. Three of the weakest other franchises offer you a job; taking one continues the off-season with your new club.

## 2. Navigation

| In | Out |
|----|-----|
| Season → Start season N+1 when `boardSeasonReview` sets `world.manager.sacked` | **Take the job** → `acceptJob(teamId)` → Off-season ([S13](S13-offseason-contracts.md)) with the new team |

If somehow opened with no sacking: header "Club / All is well / The board is behind you."

## 3. Wireframe

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│[⛨] END OF SEASON 4                                                                │
│    YOU'VE BEEN SACKED                                                             │
│    Rajasthan Regals's board has lost confidence after a 9th-place finish          │
│    (target: make the playoffs). Three franchises want to talk.                    │
└──────────────────────────────────────────────────────────────────────────────────┘
┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐
│ [⛨ lg]                │ │ [⛨ lg]                │ │ [⛨ lg]                │
│ LAKHNOW SUPER GIANTS  │ │ PUNJABI KINGS         │ │ DILLI CAPITALS        │
│ Ekaana Stadium, Luck… │ │ …                     │ │ …                     │
│ squad batting 71,     │ │                       │ │                       │
│ bowling 74 · style    │ │                       │ │                       │
│ youth builder. Their  │ │                       │ │                       │
│ board will expect…    │ │                       │ │                       │
│ [Take the job ›]      │ │ [Take the job ›]      │ │ [Take the job ›]      │
└──────────────────────┘ └──────────────────────┘ └──────────────────────┘
```

## 4. Data and rules

- `world.manager.sacked = { season, team, offers, order }`; the last board entry with a verdict gives the position and target for the subtitle.
- Offers: the 3 weakest squads by `batting + bowling` rating, excluding your old club.
- Each offer card: badge lg, team name, `{ground}, {city} · squad batting {b}, bowling {w} · style {personality label lowercase}. Their board will expect less, and you start with fresh confidence.`
- `acceptJob`: history `{ event: 'sacked', from, to }`, new team, confidence 55, then the off-season runs for the new team with the saved draft order. At the next season start the board sets a new target (`seasonTargetSet`).

## 5. Acceptance checklist

- [ ] Reaching confidence ≤ 15 at the season review shows this screen (test by losing a season badly, or by editing confidence in a debug build).
- [ ] The three offers match the web for the same world.
- [ ] Taking a job switches your club everywhere (nav, club page, off-season header) and continues to contracts/retentions.
- [ ] The "Second act" achievement (title after being sacked) can unlock later.
