# S02: Playing XI

| | |
|---|---|
| **Step** | S02 |
| **JS source** | `src/components/PickXI.jsx` |
| **C++ files** | `ui/screens/pick_xi_screen.{hpp,cpp}` |
| **Depends on** | S01 (and the Player card, S07, for name links; until S07 exists, links can be inert) |

## 1. Purpose

Choose 11 of the squad, set the batting order, name the captain (C) and wicketkeeper (WK), and see the 5 bench players who will be the Impact Player substitutes.

## 2. Navigation

| In | Out |
|----|-----|
| Home → Pick your XI (friendly) · Season → Captain this match (`playNextFixture`) · Training → Back to XI | "Next: training ›" → `training` (disabled while there are problems) · "Back" → `home` (friendly) or "Back to season" → `season` (fixture) |

## 3. Wireframe

```
SEASON MATCH 23 · v Mumbay Islanders · Wankade Stadium · Flat deck          ← fixture banner (season only)
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│[⛨] RAJASTHAN REGALS                                               [★ Auto-pick best XI]   │
│    PLAYING XI                                                                            │
│    Drag and drop or tap swap to adjust your batting order and substitute players…        │
└──────────────────────────────────────────────────────────────────────────────────────────┘
┌──────────── SQUAD 22 ─────────────────────────────┐ ┌──── BATTING ORDER 11/11 · overseas 4/4 ─┐
│ ☑ ⠿ (av) Yashasvi J…   Bat Loft Bowl Awr Type     │ │  ⇄ Drag rows or tap ⇄ to swap           │
│        Batter · Anchor · LHB · 24                 │ │ ⠿ 1 (av) Name  OS        (C) (WK) ⇄ ↑ ↓ │
│        [Tired 66]                                 │ │        Bat 82 · RF 34                    │
│ ☑ ⠿ …                       82   78   34   71  RF │ │ ⠿ 2 …                                    │
│ ☐ ⠿ … (injured row, checkbox disabled)            │ │ …                                        │
│ …                                                 │ │ [ 12 · Drop bench player here (1 needed)]│
│                                                   │ │ ✖ Select 1 more player.                  │
│                                                   │ │ ⚠ Only 4 genuine bowling options…        │
│                                                   │ │ BENCH / IMPACT SUBSTITUTES 11            │
│                                                   │ │ ⠿ (av) Name OS  Batter · Bat 70 · RM 40 ⇄│
└───────────────────────────────────────────────────┘ └──────────────────────────────────────────┘
────────────────────────────────────────────────────── [ Back ]  [ Next: training › ]  ← sticky
```

Desktop: the two panes side by side (squad 1.35 : order 1). The web's phone tab switch ("Squad / Batting order") is not needed.

## 4. Sections and widgets

### Fixture banner (season fixtures only)
`{stage == league ? "Season match {no}" : STAGE_LABEL[stage]}` · `v`/`at` (home or away) opponent name · venue · pitch label.

### Page header
Team badge, eyebrow team name, title "Playing XI", subtitle, right: **Auto-pick best XI** → `autoPick()`.

### Squad table (every player in the current squad)

| Column | Content |
|--------|---------|
| (select) | checkbox: in XI. Disabled when the XI already has 11 and he is out, or when he is injured and out |
| Player | drag handle, avatar (team colour), name link, meta line `Role · style (+ / bowl style for AR) · LHB/RHB · age (· Overseas)`, `FitnessTag` |
| Bat | `Rating(bat)` |
| Loft | `Rating(power)` (lofted shots) |
| Bowl | `Rating(bowl)` |
| Awr | `Rating(awareness)` |
| Type | bowling type short (`RF`, `OB`…) with full label as tooltip |

Row classes: in XI (highlight), injured (dim + red), drag source, drop target.

### Batting order list (the XI in order)

Each row: drag handle · number · small avatar · name link + `OS` tag + "⇄ Swap" badge when hovered as a drop target · meta `Bat {bat}` (+ ` · {type} {bowl}` if bowl ≥ 45) · **C** radio · **WK** radio · swap button ⇄ · ↑ · ↓.

- **C**: shows the named captain (`setup.captainId` if in the XI), else the club captain (`team.captainId`) if he is in the XI, else the natural leader (`pickCaptain` of the XI). Tooltip: "Club captain" for the first two, "Captain (chosen for you: pick anyone)" when auto.
- An empty "drop slot" row when fewer than 11: "Drop bench player here to fill XI (N needed)".
- Title shows `{xi.length}/11 · overseas {n}/4` (red when over).
- Swap banner when a tap-swap is in progress: "Swapping **Name** — tap another player to swap" + Cancel.

### Problems (block "Next") and warning

| Problem (red list) | Condition |
|--------------------|-----------|
| "Select N more player(s)." / "Select N fewer…" | XI ≠ 11 |
| "Choose a wicketkeeper from your XI." | no keeper |
| "{names} is/are injured." | an injured player in the XI |
| "{n} overseas players picked; the limit is 4." | > 4 overseas |

Warning (gold, only when no problems): "Only {n} genuine bowling options. Part-timers will have to fill in." when fewer than 5 non-keepers with `bowl ≥ 45`.

### Bench / Impact substitutes
Every squad player not in the XI: drag handle, avatar, name + OS, `Role · Bat N (· type bowl)`, swap button. (Note: the actual 5 named subs for the match are chosen by `benchOf` at match start: the best of the rest, injured excluded.)

## 5. Actions

| User does | Calls |
|-----------|-------|
| Tick / untick | `togglePlayer(id)` |
| Drag a row onto another row (squad, order or bench) | `swapPlayers(source, target)` |
| Drag a non-XI player onto the empty slot | `togglePlayer(source)` |
| Tap ⇄ on A, then ⇄ (or row) on B | `swapPlayers(A, B)`; tap A again cancels |
| ↑ / ↓ | `movePlayer(id, -1 / +1)` |
| C radio | `chooseCaptain(id)`: `setCaptain(id)` and makes him club captain (`clubCaptainSet`) |
| WK radio | `setKeeper(id)` |
| Auto-pick best XI | `autoPick()` (`pickBestXI` includes the club captain when fit; captainId = club captain) |
| Next: training | `goTo(training)` |
| Back / Back to season | `goTo(home)` / `goTo(season)` |

`swapPlayers` semantics: both in XI → swap order positions; one in XI → the other takes exactly his batting slot (and the leaver loses keeper/captain/training).

## 6. View state

`draggedId`, `dropTargetId`, `tapSwapId` (all local, reset on enter).

## 7. C++ notes

- Drag and drop: ImGui `BeginDragDropSource` / `BeginDragDropTarget` with payload type `"T20_PLAYER"` carrying the player id. Highlight the target row while hovering.
- Up/down and swap buttons need `PushID(playerId)`.
- Radios for C/WK: draw as small toggle pills ("C", "WK") gold when on.
- Derived data each frame: `team = derived.team(userTeamId)`, `byId` map, captain resolution, problems list. Cheap; no caching needed.

## 8. Acceptance checklist

- [ ] Squad table shows every player with correct ratings, fitness and injury tags.
- [ ] Can't add a 12th player; can't add an injured player; problems list matches the web for the same state.
- [ ] Drag a bench player onto XI player #3: he becomes #3 and the other goes to the bench; keeper/captain cleared if it was them.
- [ ] Drag within the order swaps positions; ↑/↓ move by one.
- [ ] Tap-swap works and can be cancelled.
- [ ] C shows the auto captain until you pick one; picking persists to the next visit (saved line-up).
- [ ] Auto-pick gives the same XI as the web for the same squad and pitch.
- [ ] Next is disabled with problems; the overseas counter turns red over 4.
- [ ] Season fixture: banner shows, Back goes to the season.
