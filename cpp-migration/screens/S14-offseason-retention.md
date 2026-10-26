# S14: Off-season: retentions (mega-auction years)

| | |
|---|---|
| **Step** | S14 |
| **JS source** | `offseason/RetentionStage.jsx`; engine `finance.js` (`retentionCosts`, `retentionValid`, `isCapped`, `marketValue`, `MAX_RETAIN*`, `RETENTION_SLABS`), `world.js` (`setRetention`, `closeRetention`) |
| **C++ files** | `ui/screens/offseason/retention_stage.cpp` |
| **Depends on** | S13 |

## 1. Purpose

Every 3rd off-season (`year % 3 == 0`) all contracts end before a **mega auction**. Pick up to 6 players to keep; the rest go into the auction. Unused retention slots become Right to Match (RTM) cards.

## 2. Wireframe

```
┌──── MEGA AUCTION: RETENTIONS ─────────────────────────────────────────────────────┐
│ Every contract ends before a mega auction. Keep up to 6 (at most 5 capped and 2     │
│ uncapped). Capped retentions cost ₹18, 14, 11, 18 and 14 crore in that order;       │
│ uncapped ₹4 crore. Every retention you don't use becomes a Right to Match card…     │
│   3/6          ₹43 cr           ₹71.4 cr        3                                    │
│   RETAINED     RETENTION COST   AUCTION PURSE   RTM CARDS                            │
└───────────────────────────────────────────────────────────────────────────────────┘
┌───────────────────────────────────────────────────────────────────────────────────┐
│ (av) Name  Batter · 27 [PRIME] · rating 86 · Capped · Overseas                       │
│      Market value ₹16 cr   Retention ₹18 cr                      [✔ Retained]        │
│ (av) Name  … Rookie deal ₹1.2 cr · 2 yr                          (Stays)             │
│ (av) Name  …                                                     [Retain] (disabled when it would break a limit) │
└───────────────────────────────────────────────────────────────────────────────────┘
──────────────────────────────────────── [ Confirm retentions, go to the youth draft › ]
```

## 3. Rules

| Rule | Value |
|------|-------|
| Max retained | 6 |
| Max capped / uncapped | 5 / 2 |
| Capped cost, in order of the kept list | ₹18 cr, ₹14 cr, ₹11 cr, ₹18 cr, ₹14 cr |
| Uncapped cost | ₹4 cr each |
| Order | The kept list is always sorted by market value (best player takes the first slab) |
| Rookies | Draft and academy players with years left keep their rookie deals automatically ("Stays"); they can't be retained or released here |
| Capped | `isCapped(p)`: saved flag, else roster player, age ≥ 24, or main rating ≥ 75 |
| Contract | Retained players get 3-year contracts at their slab price (`via: retained`) |
| RTM cards | `6 − kept` per team |

Stats strip: retained `n/6`, retention cost (sum), auction purse = ₹120 cr − retention cost − rookie contracts, RTM cards.

## 4. Actions

| User does | Calls |
|-----------|-------|
| Retain / un-retain | toggle in a copy of `offseason.retain`, sort by market value, and only if `retentionValid` → `retentionSet(ids)` |
| Confirm | `retentionClosed()`: user keeps his list; AI keeps up to 4 worth ≥ 95% of their slab and aged ≤ 35 (`aiRetention`); everyone else is released (except live rookie deals); RTM cards set; the draft starts |

The squad list is sorted by market value; a "Retain" button is disabled when adding him would break a limit.

## 5. Acceptance checklist

- [ ] Only appears in mega years (off-season 3, 6, 9…).
- [ ] Slab prices apply in market-value order; uncapped cost ₹4 cr; limits enforced (6 total, 5 capped, 2 uncapped).
- [ ] Rookies show "Stays" with their rookie deal.
- [ ] Strip numbers update live; RTM count = 6 − retained.
- [ ] Confirm releases everyone else into the auction and opens the draft; AI teams retain sensibly (compare counts with the web for the same seed).
