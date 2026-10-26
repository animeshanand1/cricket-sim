# S12: Season: transfer room

| | |
|---|---|
| **Step** | S12 (completes the full-season milestone) |
| **JS source** | `season/TransferRoom.jsx` (`WindowLine`, `SquadPicker`, `Cover`, `TransferRoom`); engine `trades.js`, `finance.js` (`money`, `purseLeft`, `overseasCount`, `PERSONALITIES`), `club.js` (`clubOf`, `available`) |
| **C++ files** | `ui/screens/season_tabs/transfer_room_tab.cpp` |
| **Depends on** | S09 |

## 1. Purpose

Mid-season squad business: sign a free agent to cover a long injury (any time), and trade players with other franchises while the trade window is open (after 25 league games, until 45).

## 2. Wireframe

```
┌──── INJURY COVER ─────────────────────────────────────────────────────────────────────┐
│ A player out for 3 or more matches can be covered by a free agent, signed at his base… │
│ Name  Hamstring  out for 4 more matches · Bowler                                        │
│   (av) Free agent A  Bowler · 27 · rating 64 · (block reason)       [Sign for ₹50 L]   │
│   (av) Free agent B  …                                              [Sign for ₹75 L]   │
│ Name (hamstring, 2 more) is covered by Name.                                            │
└────────────────────────────────────────────────────────────────────────────────────────┘
┌──── TRADE WINDOW ─────────────────────────────────────────────────────────────────────┐
│ Swap up to 2 players each way with another franchise, 2 trades a season…              │
│ ✔ The trade window is open for 12 more league games. Trades left this season: 2.      │
│ ⚠ Finish the match in progress before trading.          (when a match is live)          │
│ ✔/✖ flash after proposing                                                               │
│ ([⛨]CHK) ([⛨]MUI) (RGC) … 9 clubs                                                       │
│ Chennai Supreme Kings: Old heads · purse left ₹8.5 cr · 6/8 overseas · 22 players.      │
│ Yours: purse left ₹12 cr · 5/8 overseas · 21 players.                                    │
│ ┌─ YOU GET (from CHK) ───────────────────┐ ┌─ YOU GIVE ─────────────────────────────┐   │
│ │☐ (av) Name  Batter · 29 · Overseas  82 │ │☑ (av) Name  Bowler · 31        74       │   │
│ │   Rating  Salary  Years                │ │   Rating  Salary  Years                 │   │
│ └────────────────────────────────────────┘ └─────────────────────────────────────────┘   │
│ YOU ADD  [−] ₹1.25 cr [+] [Add ₹75 L]   They want about ₹75 L more…   [⇄ Make the trade]│
└────────────────────────────────────────────────────────────────────────────────────────┘
┌──── THIS SEASON'S MOVES ──────────────────────────────────────────────────────────────┐
│ TRADE · RJR  Name from CHK for Name · plus ₹1.25 cr                                     │
│ INJURY COVER · RJR  Name signed as cover for Name (hamstring)                           │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

## 3. Injury cover

| Element | Rule |
|---------|------|
| Who can be covered | `replaceable(world, team)`: injured, ≥ 3 matches left, not already covered |
| Options | `replacementOptions(world, team, injuredId, 6)`: free agents, same role first, then best rating; each with fee = base price and a block reason (`replacementBlock`): no longer available / squad full (25) / already 8 overseas / not enough purse |
| Sign | `signCover(injuredId, id)` → 1-year contract at base price (`via: replacement`), injury marked `replacedBy` |
| Covered list | "{name} ({injury}, N more) is covered by {name}." |
| Empty | "Every long-term injury is covered." / "Nobody in your squad is out for long enough to need cover." |

AI clubs cover their own injuries of 5+ matches automatically after each game (`aiReplacements`), and the game keeps 4 free agents per role (`topUpFreeAgents`).

## 4. Trade window

| Element | Rule |
|---------|------|
| Window line | before: "The trade window opens after 25 league games: N to go. Injury cover can be signed at any time." (warn) · closed: "…shut after 45 league games…" · open: "open for N more league games. Trades left this season: N." (ok) |
| Live match warning | when a match is in progress (not complete) |
| Club chips | the 9 other franchises (selecting one clears "you get") |
| Club line | `{name}: {personality label} · purse left · {os}/8 overseas · N players.` + yours |
| Squad pickers | two tables (theirs: "You get (from XXX)"; yours: "You give"), sorted by main rating: checkbox, avatar, name, `Role · age · Overseas` + FitnessTag if injured; Rating, Salary (`money(contract.cost)`), Years. Max 2 ticked per side; injured players can't be ticked; all disabled when trading isn't possible |
| Cash | `You add` / `They add` (negative); − / + in steps of ₹25 L; − stops at −₹10 cr (`MAX_CASH_BACK`); + stops at your available club money; quick button "Add {shortBy}" when that would make them accept and you can afford it |
| Verdict (live) | `evaluateTrade(world, me, other, offer)` every change: nothing ticked → "Tick the players you want and the players you would give up." · rules broken → first error (red) · accepted → "✔ {club} would accept this." · else "They want about **{shortBy}** more in players or money." |
| Make the trade | enabled when the window is open, trades left, no live match, something ticked, and the verdict accepts → `proposeTrade(other, offer)`; on success clear the offer and flash "{club} said yes. The deal is done."; on failure flash the first error or "{club} turned it down." |

Trade rules (`evaluateTrade`): ≤ 2 trades per season; ≤ 2 players each way; no injured players; after the trade each squad has 15–25 players, ≤ 8 overseas, payroll ≤ ₹120 cr purse, at least one wicketkeeper; cash ≤ your available club money; the other club pays at most ₹10 cr. Acceptance: what they receive (players valued by `tradeValue` as joining + cash, cash they pay counts 1.25×) ≥ what they give × 1.08. Each player starts at market value × the club's personality factor. They value their top 3 players ×1.35, the rest of their XI ×1.1, squad players ×0.8; joining players ×1.1 if he'd make their XI else ×0.75; ± a quarter of the contract's distance from market value.

After a trade, if any of your XI left, the XI is auto-picked again.

## 5. This season's moves

`world.transfers` for this year (newest first): kind (`Trade` / `Injury cover`), team, text, cash note (`plus ₹x` / `and ₹x received`). Empty: "No trades or signings yet this season."

## 6. View state

`otherId` (default first other club), `give[]`, `get[]`, `cash`, `flash`. Drop ticked players who are no longer in the squad (after a trade).

## 7. Acceptance checklist

- [ ] Window states at 0, 25 and 45 league games.
- [ ] Verdict text matches the web for the same offer; "Add ₹x" makes it acceptable.
- [ ] Each rule blocks with its message (3rd trade, injured player, no keeper left, squad > 25 or < 15, overseas > 8, purse, cash).
- [ ] A completed trade moves the players with their contracts, moves the money (club account → Accounts shows "Transfer fees"), logs the move, and re-picks your XI if needed.
- [ ] Injury cover lists only 3+ match injuries, shows block reasons, signs once per injury.
- [ ] Trading is blocked during a live match.
