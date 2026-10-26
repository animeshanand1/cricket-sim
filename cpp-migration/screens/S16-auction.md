# S16: Off-season: auction

| | |
|---|---|
| **Step** | S16 (third milestone: a full off-season into season 2) |
| **JS source** | `offseason/AuctionStage.jsx` (`Lot`, `AuctionStage`); engine `auction.js` (`awaitingUser`, `userCeiling`, `runAuction`, `userBid`, `userPass`, `userLimit`, `userAutoRest`, `rtmInvoke`, `rtmRaise`, `rtmMatch`), `finance.js` (`money`, `nextBid`, `purseLeft`, `overseasCount`, `marketValue`, `isCapped`, `basePrice`, `PERSONALITIES`), `world.js` (`closeAuction`) |
| **C++ files** | `ui/screens/offseason/auction_stage.cpp` |
| **Depends on** | S15 |

## 1. Purpose

The player auction (mini every year, mega every 3rd). Players come up one lot at a time; the AI franchises bid up to their valuations; you are asked whenever the bidding settles and you could still bid. In a mega auction a player's former team can use a Right to Match card.

## 2. Flow

See [02 F13](../02-process-flows.md#f13-auction). The auction advances only through `runAuction`, which runs AI steps until **the user has a decision** (`'bid'`, `'rtm-invoke'`, `'rtm-raise'`, `'rtm-match'`) or it is **done**. Every user action calls the matching engine function and then `runAuction` again.

## 3. Wireframe

```
┌──── MEGA AUCTION · 170 PLAYERS ───────────────────────────────────────────────────┐
│ AUCTION IN PROGRESS / THE HAMMER HAS FALLEN ON THE LAST LOT                        │
│ Increments: ₹5 lakh to ₹1 crore, ₹10 lakh to 2 crore, ₹20 lakh to 5 crore, then   │
│ ₹25 lakh. Former teams can use Right to Match cards. You've bought 3 for ₹21 cr.   │
│                                        [Let the AI finish my auction] / [Start the new season ›] │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── LOT (team colour of the leader) ─────────────────────┐ ┌── FRANCHISES ───────────────┐
│ MARQUEE · LOT 3 OF 170                        BASE ₹2 cr  │ │ Team  Purse   Squad OS RTM  │
│ (avatar 72)  NAME                                          │ │ CHK   ₹62 cr   9    3   2   │
│   Batter · 29 [PRIME] · Right-hand bat · Stroke-maker      │ │ …  (yours highlighted;      │
│   (Capped) (Overseas) (Last with MUI · RTM possible)       │ │  hover: auction style)      │
│   (44 M · 1,320 runs · 3 wkts · SR 142)                    │ ├── COMING UP ────────────────┤
│   Rating 86 Bat 86 Bowl 12 Field 78 Potential A           │ │ Name  Bowler · 27 · OS ₹1.5 cr│
│ ───────────────────────────────────────────────────────── │ │ … next 8 lots               │
│ CURRENT BID  ₹9.4 cr     [⛨] Chennai Supreme Kings lead    │ └─────────────────────────────┘
│ bid log: CHK ₹9.4 cr · MUI ₹9.2 cr · …                     │
│ [⚡ Bid ₹9.6 cr] [Pass]  Bid up to ₹ [ 12.00 ] cr [Set limit]│
│ You can spend up to ₹38 cr on him (keeping enough to fill 18 places). Market value about ₹11 cr. │
└────────────────────────────────────────────────────────────┘
┌──── SOLD (last 14) ──────────────────────────────────────────┐
│ [⛨] Name  Batter · RTM  ₹9.6 cr  (yours highlighted)          │
│ 12 unsold so far.                                            │
└──────────────────────────────────────────────────────────────┘
```

When done: "Your buys" list (name, role, `· RTM`, price) or "You didn't buy anyone. Squads under 18 sign unsold players at their base price when you continue."

## 4. Lot panel

| Part | Content |
|------|---------|
| Top | `{set} · lot {idx+1} of {n}` · `Base {money}` |
| Player | avatar 72 (former team colour), name link, `Role · age PhaseTag · hand · bowling type (bowl ≥ 35) · styles`; chips Capped/Uncapped, Overseas, `Last with {team}` (+ `· RTM possible` in a mega auction if that team has cards), career line `{M} M · {runs} runs · {wkts} wkts · SR`; numbers Rating, Bat, Bowl, Field, Potential |
| Bid | `No bids yet` / `Current bid` + price (base when no bids); leader badge + "You lead" / "{team} lead" / "Waiting for an opening bid"; last 6 bids |

### Decision states (`awaitingUser(world)`)

| State | UI | Calls |
|-------|----|-------|
| `bid` | **Bid {next}** · **Pass** · "Bid up to ₹ [x] cr" + **Set limit** (min next bid, max your ceiling, placeholder `min(ceiling, max(next, market value))`) · hint with your ceiling and market value | `auctionBid()` · `auctionPass()` · `auctionLimit(round(x·100))` (only if ≥ next) |
| `rtm-invoke` (you are the former team) | "{player} played for you. Use a Right to Match card ({n} left) against {leader}'s {price}? They then get one final raise." | **Use RTM** → `auctionRtm({ step: 'invoke', value: true })` · **Let him go** → `false` |
| `rtm-raise` (you lead; the former team invoked RTM) | "{former} want to match with their RTM card. You get one final raise: they must match it to keep him." Final bid input (min current, max ceiling) | **Confirm final bid** → `auctionRtm({ step: 'raise', value })` |
| `rtm-match` (you are the former team) | "{leader} raised their final bid to {price}. Match it to keep {player}?" | **Match {price}** (disabled above your ceiling) / **Let him go** |
| none | (AI is settling; normally never visible because `runAuction` only stops on a decision or the end) | — |

Your ceiling (`userCeiling`): 0 if your squad is full (25) or he is overseas and you have 8; else `min(₹27 cr, purse left − ₹30 L × slots still needed to reach 18)`.

## 5. Header actions

| Button | When | Calls |
|--------|------|-------|
| Let the AI finish my auction | not done; asks to confirm ("Yes, AI bids for me" / Cancel) | `auctionAutoRest()`: from now on the AI bids for you with a balanced valuation |
| Start the new season / Finish the off-season | done (label from `offseason.then`) | `finishOffseason()`: `auctionClosed` (squads under 18 sign unsold players at base price, or journeymen; contracts; capped flags), `autoPick`, then `startSeason(team)` or Players |

## 6. Side panels

- **Franchises** table: team, purse left, squad size, overseas count, RTM cards (mega only); yours highlighted; tooltip = personality note.
- **Coming up**: next 8 lots: name link, `role · age · OS`, base price.

## 7. Engine facts worth testing

- Lot order: up to 8 marquee capped players (highest market value), then Batters, All-rounders, Wicketkeepers, Fast bowlers, Spinners (capped), then Uncapped.
- Pool: free agents aged ≤ 38, top 170 (mega) / 60 (mini) by market value; +24 (mega) / 16 (mini) new entrants are created first.
- Contract length: 3 years in a mega auction, else years until the next mega. Mini auction: an overseas player's salary is capped at ₹18 cr (the team still pays the full bid).
- AI valuation (`teamValuation`): market value × personality × XI-fit (×1.15 / ×0.9 / ×0.55) × role need (+12% per missing player of that role; ×0.7 when overstocked by more than 2) × noise 0.85–1.15 (seeded by team, player and year), capped by ₹27 cr and by what keeps 18 players affordable. 0 when the squad is full or the overseas limit is reached.

## 8. Acceptance checklist

- [ ] Lots come in the same order as the web for the same seeded world; AI winners and prices match with auto-rest on.
- [ ] Bid increments follow the IPL steps; the ceiling and its hint are right; Pass removes you from the lot.
- [ ] Set limit auto-bids up to the limit, then hands back.
- [ ] RTM sequence works both ways (you as former team, you as leader) in a mega auction.
- [ ] Let the AI finish completes the auction; then squads are ≥ 18 and the new season starts with a board target.
