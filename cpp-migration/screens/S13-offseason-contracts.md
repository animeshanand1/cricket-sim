# S13: Off-season shell and contracts

| | |
|---|---|
| **Step** | S13 |
| **JS source** | `offseason/OffseasonView.jsx`, `offseason/ContractsStage.jsx` (`PurseBar`, `ExpiringRow`), `OffSeasonReport.jsx`; engine `world.js` (`runOffseason`, `expiring`, `renewPlayer`, `releasePlayer`, `closeContracts`), `finance.js` (`renewalDemand`, `renewalYears`, `marketValue`, `payroll`, `purseLeft`), `club.js` (`renewalPrice`) |
| **C++ files** | `ui/screens/offseason_screen.{hpp,cpp}`, `ui/screens/offseason/contracts_stage.cpp`, `ui/widgets/{purse_bar,offseason_report}.cpp` |
| **Depends on** | S09 |

## 1. Purpose

The off-season container (header, 3-step stepper, report) and its first stage in non-mega years: **Contracts**, where you renew or release players whose contracts have ended (and can release anyone else).

## 2. Navigation

| In | Out |
|----|-----|
| Season → Start season N+1 (`nextSeason`) · Sacked → Take the job · Players → Advance one year · nav Season while an off-season runs (`setup.screen == offseason`) | Continue to the youth draft → `contractsClosed()` → Draft stage ([S15](S15-youth-draft.md)) |

Mega years (every 3rd off-season) show Retentions instead ([S14](S14-offseason-retention.md)).

## 3. Off-season shell

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│[⛨] OFF-SEASON 3 · MEGA AUCTION YEAR                                               │
│    OFF-SEASON                                                                     │
│    Every player is a year older and some have retired. Sort out contracts…        │
└──────────────────────────────────────────────────────────────────────────────────┘
(1 Contracts | Retentions) ─ (2 Youth draft) ─ (3 Auction | Mega auction)    ← stepper
[OFF-SEASON REPORT — open on the contracts/retention stage, collapsed later]
<stage view>
```

No off-season running → header "Off-season" + "No off-season is running. It starts when you begin a new season, or from Players → Advance one year." + the last report.

Stage view by `world.offseason.stage`: `contracts` → this doc · `retention` → S14 · `draft` → S15 · `auction` → S16.

## 4. Contracts stage

```
┌──── YOUR PURSE ───────────────────────────────────────────────────────────────────┐
│  ₹14.5 cr        ₹105.5 cr       21             6/8                                 │
│  PURSE LEFT      COMMITTED       SQUAD (18-25)  OVERSEAS                            │
│  ████████████████████████████████████████████░░░░░                                 │
│ Renewals and releases change what you have to spend in the auction…                │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── CONTRACTS ENDING (3) ─────────────────────────────────────────────────────────┐
│ (av) Name   Batter · 33 [VETERAN] · rating 78 · Overseas                            │
│      Was paid ₹8 cr   Market value ₹6.5 cr   Asks for ₹6.2 cr / season (was ₹6.8 cr: prize bonus goodwill) │
│      [2 years ▾] [Renew] [Release]                                                  │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── UNDER CONTRACT ───────────────────────────────────────────────────────────────┐
│ (av) Name  Bowler · 25 · rating 81   Salary ₹9 cr  Years left 2  Value ₹11 cr  [Release] │
│                                                    (confirm: [Release] [Keep])      │
└───────────────────────────────────────────────────────────────────────────────────┘
──────── 3 unresolved: they'll be released into the auction.  [ Continue to the youth draft › ]
```

| Widget | Rule |
|--------|------|
| `PurseBar(team)` | Purse left = ₹120 cr − payroll; Committed = payroll; Squad size (18–25); Overseas n/8; bar = payroll/purse |
| Expiring rows | `expiring(world, me)` = contract years ≤ 0. Show was-paid, market value, asking price `renewalPrice(world, team, p, years)` per season (with the original demand struck in a note when goodwill lowers it). Years combo from `renewalYears(p)`: age ≥ 34 → [1]; ≥ 31 → [1, 2]; else [1, 2, 3]; default the longest. Asking price changes with length (1 yr ×1.08, 3 yrs ×0.95) |
| Renew | `contractRenewed({ id, years })` → new contract (`via: renewed`) |
| Release (expiring) | `contractReleased(id)` → becomes a free agent in the auction pool (`formerTeam` set) |
| Under-contract rows | sorted by rating: salary, years left, market value; Release with inline confirm |
| Continue | `contractsClosed()`: any still-expiring user players are released; AI clubs renew core players they can afford, drop up to 2 overpaid veterans (31+, cost > 1.6× value), trim to 22; then the draft starts. Hint shows how many are unresolved |

## 5. What already happened when this screen opens (`runOffseason`)

Year +1; fitness reset, long injuries reduced; ageing (with coaching from last season); retirements (max 4 per squad); quiet retirement of long-unsold free agents; contracts tick down; one academy prospect per club. The report below shows it all.

## 6. Off-season report (`OffSeasonReport.jsx`)

Shared panel (also on the season hub and the Players page). Shows `world.lastOffseason`.

| Part | Content |
|------|---------|
| Header | eyebrow `Off-season {year}`, title "A year older", summary: `{n} retired ({k} from your squad) · {n} academy graduates · youth draft in progress / {n} draft picks · {n} free-agent signings · {n} released`; buttons Show/Hide, ✕ dismiss (`offseasonReportDismissed`) |
| Tab "Your squad" | hint about the age curve; your players sorted by main change: avatar, name, `Now {age} {PhaseTag}`, changes `Bat ▲+1.5`, `Bowl ▼-2` (bowlers and all-rounders), `Field` |
| Tab "Risers and declines" | two lists of 6: biggest risers and biggest declines across the league (with team) |
| Tab "Retired (n)" | your squad first, then by rating: `{team} (your squad) · role · retired at {age}` + `Rating {main}` |
| Tab "New players (n)" | Academy graduates (yours first), Youth draft picks (`Pick n · team · role · age`), Free-agent signings; each with `Now {rating}` |

## 7. Acceptance checklist

- [ ] Off-season header, mega-year label and stepper reflect `world.offseason`.
- [ ] Report numbers and lists match the web for the same seeded world (ages, retirements, academy).
- [ ] Asking prices match the web, including the 1/2/3-year multipliers and prize-share goodwill.
- [ ] Renew and release work; releasing a contracted player needs confirmation.
- [ ] Continue releases unresolved expiring players and opens the youth draft; AI squads end at ≤ 22.
