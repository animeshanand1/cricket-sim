# S19: Club: accounts, sponsors, home ground, youth and coaching

| | |
|---|---|
| **Step** | S19 |
| **JS source** | `src/components/club/ClubBusiness.jsx` (`PlanBanner`, `CashStrip`, `AccountsPanel`, `SponsorsPanel`, `FacilityCard`, `GroundPanel`, `AssignCoach`, `YouthPanel`); engine `club.js` |
| **C++ files** | `ui/screens/club/{accounts_tab,sponsors_tab,ground_tab,youth_tab}.cpp`, `ui/widgets/facility_card.cpp` |
| **Depends on** | S18 |

## 1. The one rule

**Every decision here is for next season.** `planYear(world, club)`: during a season (or after it ends, before the off-season) decisions apply to the following season; between seasons, to the season about to start. Upgrades are **paid when ordered** and **open when that season starts** (`activateClub`). Coaching works over a season and shows in the off-season after it.

Every tab starts with the **PlanBanner**: "Changes you make here take effect from **season {n}**, not straight away. Upgrades are paid for now and open when that season starts; coaches and prices switch on then too." (n = current season + 1, or history + 1 between seasons.)

Money is in lakh (₹1 cr = 100 L). "Free to spend" = bank − money already promised to next season's coaches and staff (`available`).

---

## A. Accounts and prizes

```
┌── CLUB ACCOUNT ─────────────────────────────────────────────────────────┐
│  ₹31.5 cr        ₹3 cr              ₹28.5 cr        78                   │
│  CLUB ACCOUNT    PROMISED TO COACHES FREE TO SPEND   POPULARITY           │
│ The club account pays for the ground, facilities and coaches. Player wages │
│ come out of the ₹120 crore purse… Each season brings ₹25 cr of central…   │
└──────────────────────────────────────────────────────────────────────────┘
┌── THE BOOKS ───────────────────────────────────────── Year [Year 4 ▾] ──┐
│ Income            ₹       │ Spending              ₹                       │
│ Central rights    ₹25 cr  │ Running costs         ₹15 cr                   │
│ Sponsors          ₹14 cr  │ Facility upkeep       ₹0.45 cr                 │
│ Ticket sales …            │ Building work …                                │
│ Total             ₹x      │ Total                 ₹y                       │
│ Year | Finished | Prize (₹x to players) | Crowds 92% full | Net (+/-) | Fans 70 → 74 │
└──────────────────────────────────────────────────────────────────────────┘
┌── PRIZE MONEY ──────────────────────────────────────────────────────────┐
│ ₹20 cr Champions · ₹13 cr Runners-up · ₹7 cr Third · ₹6.5 cr Fourth       │
│ (Keep it all) (25% to the players  title: −5% on renewals) (50% …  −10%)  │
│ This season: the club keeps it all. Goodwill from last season's bonus…    │
└──────────────────────────────────────────────────────────────────────────┘
┌── CLUB NEWS ── last 10: "Year 4  Stands level 2 ordered for ₹22 cr. Opens next season." ──┐
```

| Widget | Rule |
|--------|------|
| Cash strip | bank (red with − when negative), committed, free to spend, popularity |
| Books | year combo (last 10, shown as `Year {y+1}`); income keys: central, sponsors, bonuses, tickets, matchday ("Food and merchandise"), hospitality, prize, transfers; spend keys: running, upkeep, staff, coaches, building, playerBonus ("Players' prize bonus"), transfers. Empty: "Nothing in the books yet…" |
| Season history | per year: position, prize (+ share to players), crowds `{fill}% full`, net (green/red), fans before → after |
| Prize share chips | 0 / 25 / 50% → `clubPrizeShareSet({ team, share })`; each shows the renewal discount a title would buy (`goodwillFrom`: 1% per ₹1 cr shared, max 10%) |
| Current policy line | this season's share + last season's goodwill if it applies this off-season |
| Club news | `club.log` (10 newest) |

## B. Sponsors

- **Current deals**: Slot, Sponsor (`brand · deal type`), per season, bonuses (`₹x playoffs · ₹y title`), seasons (`n left, including this one` or `starts next season`). Hint: fees paid at season start, bonuses at the end; offers grow with popularity; the ground partner pays more for a bigger ground.
- **Offers** for each slot free next season (6 slots: Principal shirt ₹10 cr base, Kit ₹5 cr, Sleeve ₹3 cr, Ground partner ₹2.5 cr, Helmet and cap ₹2 cr, Digital ₹2 cr; × popularity pull `0.55 + 0.9·pop/100`): three offers (fixed fee 1–2 seasons; performance = 70% fee + bonuses 35% playoffs / 60% title, 2 seasons; long term = 90% fee, 3 seasons), each with fictional brand, per season, seasons, bonuses → **Sign** → `clubSponsorSigned({ team, slot, index })`. Offers are seeded and stable until the season ends. All filled: "Every slot is filled for next season."

## C. Home ground

| Section | Content |
|---------|---------|
| Ground | `{ground}`: seats now, seats next season, tickets this season, popularity; popularity meter with explanation |
| Ticket prices next season | 4 tier cards (radio): Budget ₹800 / Standard ₹1,500 / Premium ₹2,500 / Elite ₹4,000, each with projected crowd (`{att} fans ({pct}% full)`) and takings per game from `projectGate` (next season's ground, average form and opponent), plus the tier note → `clubTicketsSet({ team, tier })` |
| Home pitch next season | Without a curator: explanation of the usual surface. With one: "{80/90/100}% of your home league games will be on the surface you choose. Playoff games use the ground's usual surface." chips `Usual (…)` + 5 pitches → `clubPitchSet({ team, pitch })` |
| Ground upgrades | `FacilityCard` for stands (+5,000 seats/level), hospitality (₹40 L a home game per level), fan zone (+₹40 per fan, +1 popularity a season per level), curator (3 levels) |
| Home games this season | table: game (`stage · #no`), opponent, crowd `att / cap`, takings |

**FacilityCard**: label, level pips (filled, "building" for an ordered level), help text; status "Level n being built · opens next season" / "Fully upgraded" / button **Build level n+1 · ₹cost** (disabled when cost > free money, with "Not enough free money") → `clubUpgradeOrdered({ team, key })`.

Facility costs (₹ lakh per level): stands 1500/2200/3000/4000/5200; hospitality 800/1200/1700/2300/3000; fan zone 500/800/1200/1600/2100; curator 400/700/1000; academy 700/1100/1600/2200/3000; HPC 1000/1500/2100/2800/3600. Upkeep ₹15 L per level per season.

## D. Youth and coaching

| Section | Content |
|---------|---------|
| Youth facilities | FacilityCards for the academy (+2.5 graduate quality and +1 scout report per level) and the high-performance centre (+1 personal-coach slot, +10% coaching, under-24s grow a little in their own discipline) |
| Support staff next season | three combos (Batting coach, Bowling coach, Fielding coach) with tiers None / Good ₹1 cr / Elite ₹2 cr / World-class ₹3.5 cr; "(now: …)"; tiers you can't afford are disabled → `clubStaffSet({ team, role, tier })`. They coach every player aged ≤ 26 |
| Personal coaches | `{used} of {slots} slots used next season`; explanation; list of planned coaches (player, `Role · age · rating · potential`, `{tier} {area} coach`, cost per season, status "Working now, continues" / "Starts next season", button "End after this season" / "Cancel" → `clubCoachRemoved({ team, playerId })`); finishing-this-season note; **AssignCoach** form when a slot is free: Player combo (`name · age · role · rating (pot. n)`), Coach area combo (`areasFor(p)`: batting, pace or spin, fielding, mental, + keeping for keepers), Level combo (Specialist ₹40 L, Elite ₹1 cr, World-class ₹2 cr), **Hire from next season** (disabled when unaffordable) → `clubCoachAssigned({ team, playerId, area, tier })`. All slots used: "Upgrade the high-performance centre for more." |
| Coaching report | after an off-season with coaching: table Player, Rating `before → after`, Potential (`before → after` if a World-class coach lifted it), Skill points `+n`, Worked with (notes) |

## Acceptance checklist

- [ ] PlanBanner shows the right season number during and between seasons.
- [ ] Ordering an upgrade debits the account now; the level opens only when the next season starts; a second order for the same facility is blocked while building.
- [ ] Ticket tier projections match the web; the chosen tier applies next season (takings per home game in the books).
- [ ] Curator levels give 80/90/100% of home league games on the chosen pitch next season.
- [ ] Prize share pays the players at season end and lowers renewal demands next off-season (Contracts stage shows "was ₹x: prize bonus goodwill").
- [ ] Sponsor offers are stable until the season ends; signing fills the slot from next season; bonuses arrive at the end.
- [ ] Staff and personal coaches are paid at season start (unaffordable ones dropped with a club note) and improve players in the following off-season (coaching report).
- [ ] Ending a season in the red costs 8 board confidence.
