# S15: Off-season: youth draft

| | |
|---|---|
| **Step** | S15 |
| **JS source** | `src/components/Draft.jsx`; engine `world.js` (`draftOnTheClock`, `draftAvailable`, `potentialView`, `potentialGrade`, `mainRating`, `roleCounts`, `aiDraftChoice`, `ROLE_TARGET`, `SQUAD_MAX`, `DRAFT_ROUNDS`, `scoutProspect`, `draftPick`, `advanceDraft`, `finishDraft`) |
| **C++ files** | `ui/screens/offseason/draft_stage.cpp` |
| **Depends on** | S13 |

## 1. Purpose

Draft home-grown prospects (17–21) over 2 snake rounds, worst team first. Scout to reveal a prospect's exact potential (limited reports). Then open the auction.

## 2. Wireframe

```
┌──── 2 ROUNDS, SNAKE ORDER, WORST TEAM FIRST ──────────────────────────────────────┐
│ YOUTH DRAFT                                                                        │
│ 26 home-grown prospects aged 17-21 on 3-year rookie deals (₹1.6 crore for the first │
│ pick, falling to ₹30 lakh). Ratings now are visible to everyone; potential is only  │
│ a range until you scout a player (6 scout reports left). Your picks: 4 and 17.      │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── (gold border when it's you) ──────────────────────────────────────────────────┐
│ PICK 4 OF 20   YOU'RE ON THE CLOCK                       [Pick for me] [Auto-finish draft] │
│ (or "PICK 5 · Mumbay Islanders are picking"; or "Draft complete … [Continue to the auction ›]") │
└───────────────────────────────────────────────────────────────────────────────────┘
┌──── PROSPECTS ─────── Role [All ▾] Sort [Potential ▾] ┐ ┌── YOUR SQUAD NEEDS ─────────┐
│ (av) Name  Bowler · 19 · RHB · RF · Swing bowler       │ │ 21 players (18-25 after…)  │
│   Now 58  Bat 22  Bowl 58  Field 61  [B–A+ Pot. 74-88] │ │ Wicketkeeper   2/2          │
│   [◎ Scout] [Draft]                                    │ │ Batter         5/6  (short) │
│ (scouted rows highlighted; potential shows exact grade)│ │ All-rounder    4/5  (short) │
│ …                                                      │ │ Bowler         7/7          │
└────────────────────────────────────────────────────────┘ ├── DRAFT BOARD ──────────────┤
                                                           │ 1 [⛨] Name   Batter         │
                                                           │ 2 [⛨] Name   Bowler         │
                                                           │ 3 [⛨] On the clock          │
                                                           │ …20 picks (yours highlighted)│
                                                           └─────────────────────────────┘
```

## 3. Rules

| Rule | Value |
|------|-------|
| Pool | 26 prospects (`makePlayer` kind prospect), all home-grown (not overseas) |
| Order | 2 rounds, snake (round 2 reversed); order = the draft order from the season (worst first) |
| Rookie deal | 3 years; cost by pick number: `max(30, 160 − (pick − 1)·7)` lakh |
| Scout reports | 6 + the user's academy level |
| Potential view | exact for scouted prospects and for your own players; else a 14-point range (seeded), grades A+ ≥ 88, A ≥ 82, B+ ≥ 76, B ≥ 70, C+ ≥ 64, C ≥ 58, D |
| Full squad | a team with 25 players skips its pick |
| AI picks | `aiDraftChoice`: noisy potential × 0.7 + rating now × 0.3 + 6 per missing role; the AI picks automatically until it's your turn |
| Undrafted | become free agents in the auction pool |

## 4. Widgets and actions

| Widget | Interaction |
|--------|-------------|
| Clock panel | your turn: **Pick for me** → `makeDraftPick(aiDraftChoice(world, me))`; **Auto-finish draft** → `completeDraft()` (the AI makes every remaining pick including yours, then the auction opens). Done: **Continue to the auction** → `completeDraft()` |
| Prospect list | filter by role, sort by potential (mid of range), rating now, youngest, role. Row: avatar, name link, `Role · age · LHB/RHB · bowl type (bowl ≥ 30) · Overseas · bat style / bowl style`; numbers Now, Bat, Bowl, Field, Potential (grade range or exact) |
| Scout | only if not exact; disabled with no reports left → `scout(id)` |
| Draft | enabled on your turn and squad < 25 → `makeDraftPick(id)` (then the AI picks until your next turn) |
| Squad needs | role counts vs targets WK 2, BAT 6, AR 5, BOWL 7 (short rows highlighted) |
| Draft board | 20 slots: number, team badge, the pick (name link + role), "On the clock" for the current one; yours highlighted |

## 5. Acceptance checklist

- [ ] Pool, order and AI picks match the web for the same seeded world.
- [ ] Scouting reveals exact potential and uses a report; disabled at 0.
- [ ] Your picks happen at the right numbers; Pick for me chooses the same player as the web.
- [ ] Auto-finish completes the draft and opens the auction.
- [ ] Draft board and squad needs update after every pick.
