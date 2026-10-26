# S18: Club: board, contracts, medical

| | |
|---|---|
| **Step** | S18 |
| **JS source** | `src/components/ClubView.jsx`, `BoardPanels.jsx` (`BoardPanel` full, `InjuryList`), `offseason/ContractsStage.jsx` (`PurseBar`) |
| **C++ files** | `ui/screens/club_screen.{hpp,cpp}`, `ui/widgets/{board_panel,injury_list,purse_bar}.cpp` |
| **Depends on** | S09 |

## 1. Purpose

The club page for the franchise you manage: what the board thinks, the finances, every contract, and the medical room. It also hosts the four business tabs ([S19](S19-club-business.md)).

## 2. Navigation

Nav **Club** from anywhere. During an off-season the header has **Back to the off-season ›** → `goTo(offseason)`.

Team shown: `world.manager.team`, or `setup.userTeamId` before any season.

## 3. Wireframe

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│[⛨] CLUB                                                [Back to the off-season ›] │
│    RAJASTHAN REGALS                                                               │
│    Sawai Mansing Stadium, Jaipur. Contracts, the purse and what the board         │
│    expects. Rival franchises: CHK (Old heads), MUI (Star chaser), …               │
└──────────────────────────────────────────────────────────────────────────────────┘
[Board and finances] [Accounts and prizes] [Sponsors] [Home ground] [Youth and coaching] [Contracts (22)] [Medical room]
──── Board and finances ────
┌──── THE BOARD · RAJASTHAN REGALS ─────────────────────────────────────────────────┐
│ TARGET: MAKE THE PLAYOFFS                                                          │
│ Your squad was rated 3rd strongest before the season, so that's what they expect…  │
│ ████████████████▏░░░░░░░░░  62 · Happy        (red marker at 15 = sack line)       │
│ Season 3 · RJR   Make the playoffs: finished 2nd   Exceeded expectations (+22)      │
│ … last 6 seasons                                                                   │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── FINANCES ─────────────────────────────────────────────────────────────────────┐
│ PurseBar  ·  "Club account: ₹31.5 cr for the ground, facilities and coaches…"     │
└───────────────────────────────────────────────────────────────────────────────────┘
Your franchise's AI style when you're not in charge: Moneyball.
```

## 4. Tabs

`overview` "Board and finances" (default) · `accounts` · `sponsors` · `ground` · `youth` (S19; each shows the PlanBanner first) · `contracts` "Contracts (n)" · `medical` "Medical room".

### Board and finances
- **BoardPanel (full)**: with no manager team yet, an explainer ("The board sets you a target when you start a season…"). Otherwise: eyebrow `The board · {team}`, title `Target: {label}` or "Between seasons", hint with the strength rank, confidence meter (0–100 track, fill green ≥ 60 / gold ≥ 35 / red, red sack marker at 15) + `{confidence} · {label}`, history of the last 6 season verdicts: `Season N · TEAM`, `{target}: finished {pos}`, `{verdict} ({+delta})` green/red.
- **Finances**: `PurseBar`, then "Club account: **₹x** (or −₹x) for the ground, facilities and coaches (see Accounts and prizes). Purse ₹120 crore. Squads need 18-25 players with at most 8 overseas. Contracts are renewed or released each off-season; every third off-season is a mega auction…"

### Contracts
Table sorted by salary (highest first):

| Column | Value |
|--------|-------|
| Player | avatar, name link, `Role · age PhaseTag · Overseas · Capped/Uncapped` |
| Rating | main rating |
| Salary | `money(contract.cost)` |
| Years | years left |
| Value | `money(marketValue)` |
| How signed | Retained, Auction, RTM, Renewed, Draft pick, Academy, Signed at base, Signed, Trade, Injury cover (+ " (fee capped)" when salary < cost) |
| Fitness | FitnessTag, or "Fresh" when ≥ 90 and not injured |

### Medical room
`InjuryList` full: "Everyone is fit. Fast bowlers and older players tire quickest: rest them in a busy run of games." when clear; injured list (name, red label, "out for N more matches"); tired list (< 70) with FitnessTag; "Elsewhere: …" up to 5 recent injuries at other clubs.

Footer hint: "Your franchise's AI style when you're not in charge: {personality}."

## 5. Acceptance checklist

- [ ] Board panel: target, rank hint, confidence and label, sack marker, history match the web.
- [ ] Purse bar and club account match.
- [ ] Contracts table lists every squad player with the right "How signed".
- [ ] Medical room lists injured and tired players; "Elsewhere" shows other clubs' recent injuries.
- [ ] During an off-season the header button returns to it.
