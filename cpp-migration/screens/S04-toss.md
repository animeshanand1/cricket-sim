# S04: Toss

| | |
|---|---|
| **Step** | S04 |
| **JS source** | `src/components/Toss.jsx`, `Milestones.jsx` (`MilestonesInSight`); engine `match.js` (`callToss`, `chooseTossDecision`), `weather.js` (`SKIES`), `milestones.js` (`upcoming`) |
| **C++ files** | `ui/screens/toss_screen.{hpp,cpp}`, `ui/widgets/milestones.cpp` |
| **Depends on** | S03 |

## 1. Purpose

Show the match conditions (pitch, dew, weather), both sides' training, milestones in sight, and let the user call the toss and choose to bat or bowl.

## 2. Navigation

| In | Out |
|----|-----|
| Training → Go to the toss (`startFixture`) · nav Match while `status == toss` | After the decision → `match` |

## 3. Wireframe

```
            [⛨ xl]                    v                    [⛨ xl]
       Rajasthan Regals                               Mumbay Islanders
┌─ ▤ PITCH REPORT · Wankade Stadium ──────────────────────────────────────────────┐
│ FLAT DECK                                                                         │
│ Hard, true surface with short boundaries. Expect 190-plus.                        │
│ Evening game: dew is expected in the second innings…  (or "Day game: no dew…")    │
│ 💧 Overcast. Heavy cloud: the new ball will swing for longer. Chance of rain: 22%. │
│    (+ "Rain has delayed the start: 12 overs a side." when it did)                 │
└──────────────────────────────────────────────────────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────────────────────┐
│ YOUR TRAINING  Yash Jaiswal (Range hitting) · … / Rest day                         │
│ MUMBAY ISLANDERS TRAINING  …                                                       │
└──────────────────────────────────────────────────────────────────────────────────┘
┌─ ◎ MILESTONES IN SIGHT ──────────────────────────────────────────────────────────┐
│ [⛨] Name  14 runs to 1,000 career runs     (up to 4 yours, 2 theirs)              │
└──────────────────────────────────────────────────────────────────────────────────┘
┌──────────────────────────────── TOSS CARD ───────────────────────────────────────┐
│                                   (🪙)                                            │
│ State A: CALL THE TOSS                        [ Heads ] [ Tails ]                 │
│ State B: It's heads. YOU WON THE TOSS. WHAT WILL YOU DO?  [ Bat first ] [ Bowl first ] │
│ State C: It's tails. You called heads.                                             │
│          MUMBAY ISLANDERS WON THE TOSS AND CHOSE TO BAT FIRST.   [ Take the field ] │
└──────────────────────────────────────────────────────────────────────────────────┘
```

## 4. Widgets and states

| Widget | Data |
|--------|------|
| Fixture | user team (`teams[userTeam]`) v opponent, xl badges |
| Pitch card | `PITCHES[pitch].label/report`; venue from `match.fixture.venue` if a season game; dew line from `match.dew`; weather line `SKIES[sky].label/note`, `round(rainChance·100)%`, pre-match rain note when `weather.rain.when == 'pre' && rainDone` (`match.oversPerSide`) |
| Training summary | for each side, players with `training`: `Name (Drill label)` joined by ` · `, else "Rest day" |
| Milestones in sight | `upcoming(career.players, side XI, { perPlayer: 1 })`, first 4 for the user's side and 2 for the opponent's; hidden when empty |
| Toss card | three states below |

| Toss state | Condition | Buttons → calls |
|-----------|-----------|-----------------|
| A: call | `match.toss == null` | Heads / Tails → `callToss('heads'/'tails')`. If the AI wins, it decides immediately (`aiTossDecision`) and the match starts |
| B: you won | `toss.winner == userTeam && !toss.decision` | Bat first → `chooseToss('bat')`, `goTo(match)` · Bowl first → `chooseToss('bowl')`, `goTo(match)` |
| C: they won | `toss.winner != userTeam` | "Take the field" (they bat) / "Pad up" (they bowl) → `goTo(match)` |

AI toss choice (`aiTossDecision`): bowl if dew, overcast, or rain likely (> 10%); green or flat → bowl; dusty or slow → bat; balanced → 50/50 (one `rand`).

## 5. Animation

Coin icon flips (rotateY 1800° with a 24-px hop, 0.9 s) when the toss result appears.

## 6. C++ notes

The coin: draw the `coin` icon in a 64-px disc; animate horizontal scale `|cos(θ)|` for the flip plus a vertical offset arc.

## 7. Acceptance checklist

- [ ] Pitch, dew and weather lines match the web for the same seed (seeded match: start a season fixture from the same save).
- [ ] A pre-match rain delay shows the shortened overs.
- [ ] Both training lines show the right players and drills (the AI's 3 drills too).
- [ ] Milestones in sight appear when a player is close (e.g. a batter on 970 career runs).
- [ ] Winning the toss shows Bat/Bowl; losing shows the AI's choice and the right button label.
- [ ] Leaving and coming back via nav Match keeps the toss state.
