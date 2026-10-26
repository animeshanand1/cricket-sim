# S03: Pre-match training

| | |
|---|---|
| **Step** | S03 |
| **JS source** | `src/components/Training.jsx`; engine `development.js` (`DRILLS`, `drillsFor`, `applyTraining`, `aiTrainingPlan`) |
| **C++ files** | `ui/screens/training_screen.{hpp,cpp}` |
| **Depends on** | S02 |

## 1. Purpose

Give up to 3 players in the XI one drill each. A drill boosts the drilled skills **for this match only** (+3 to +7) and adds +4 starting confidence. Then create the match and go to the toss.

## 2. Navigation

| In | Out |
|----|-----|
| Playing XI → Next: training · Match result → Rematch | "Back to XI" → `xi` · "Go to the toss" / "Skip training, go to the toss" → `startFixture()` → `toss` |

## 3. Wireframe

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│[⛨] RAJASTHAN REGALS   PRE-MATCH TRAINING                       [🏋 Suggest a plan] │
│ Up to 3 players, one drill each. The boost lasts for this match only…             │
└──────────────────────────────────────────────────────────────────────────────────┘
┌──── SLOT 1 ──────── (av) ┐ ┌──── SLOT 2 ──────────────┐ ┌──── SLOT 3 ──────────────┐
│ Player [Name · bat 82 ▾] │ │ Player [Rest day ▾]      │ │ Player [disabled ▾]      │
│ Drill  [Range hitting ▾] │ │ Pick a player to see the │ │ Fill the slot above first│
│ Clearing the rope…       │ │ drills that suit him.    │ │                          │
│ Open Yash's card         │ │                          │ │                          │
│ • Lofted shots 78 → 85   │ │                          │ │                          │
│ • Grounded shots 80 → 83 │ │                          │ │                          │
│ Batting 82 → 84 · +4 con…│ │                          │ │                          │
└──────────────────────────┘ └──────────────────────────┘ └──────────────────────────┘
┌──── DRILLS ──────────────────────────────────────────────────────────────────────┐
│ Technique nets (Batting): Tighter defence…  Defence +5, Against pace +4, …        │
│ … 9 drills …                                                                      │
│ The opposition coach trains three of their players too. You'll see who at the toss│
└──────────────────────────────────────────────────────────────────────────────────┘
──────────────────────────────────── [ Back to XI ]  [ 🪙 Go to the toss ]  ← sticky
```

## 4. Widgets

| Widget | Data | Interaction |
|--------|------|-------------|
| Suggest a plan | — | `clearTraining()` then `setTraining` for each of `aiTrainingPlan(XI players, pitch or 'balanced')` |
| Slot card × 3 | `setup.training[i]` | — |
| Player combo | "Rest day (no drill)" + XI players not used in another slot, labelled `Name · bat N (· bowl N if ≥ 35)`; disabled when the slot before is empty | Pick: `setTraining({ slot, playerId, drill })` with the current drill if it suits him, else his first suitable drill. Rest day: `setTraining({ slot, playerId: null })` |
| Drill combo | `drillsFor(player)` grouped Batting / Bowling / Fielding | `setTraining({ slot, playerId, drill })` |
| Help line | `drill.help` + a link "Open {first name}'s card" | opens player card |
| Preview | each boosted skill `{label} {before} → {after}` (keeping only for keepers) and headline changes `Batting a → b · …` + "+4 confidence walking out" | computed with `applyTraining(player, drill)` |
| Drills reference | all 9 drills with area, type (pace/spin), help, boosts | — |
| CTA | label depends on whether any drill is set | `startFixture()` |

`drillsFor(p)`: bowling drills only if he bowls (`bowlType` and `bowl ≥ 35`); pace-only drills for pacers, spin-only for spinners; batting and fielding drills for everyone.

The 9 drills:

| Key | Label | Area | Boosts |
|-----|-------|------|--------|
| technique | Technique nets | Batting | defence +5, vsPace +4, vsSpin +4 |
| range | Range hitting | Batting | lofted +7, grounded +3 |
| rotation | Running and rotation | Batting | rotation +6, running +6 |
| pressure | Pressure scenarios | Batting | awareness +5, temperament +6 |
| newball | New-ball swing and seam | Bowling (pace) | swing +6, seam +6 |
| death | Yorkers and slower balls | Bowling (pace) | death +7, variations +4 |
| spincraft | Turn and flight | Bowling (spin) | turn +6, flight +5 |
| lines | Line and length | Bowling | accuracy +6 |
| fielding | Catching and fielding | Fielding | catching +7, ground +4, throwing +3, keeping +5 |

## 5. What `startFixture()` does

See [02 F3](../02-process-flows.md#f3-friendly-match). Training is consumed by this match (`clearTraining()` afterwards).

## 6. C++ notes

- Three slot cards in a 3-column table (one column below ~960 px).
- `applyTraining` returns a new player copy; never write it back to the world.

## 7. Acceptance checklist

- [ ] Slot 2 is disabled until slot 1 has a player; a player can't be in two slots.
- [ ] Only suitable drills appear (a pure batter gets no bowling drills; a spinner gets "Turn and flight", not "New-ball swing").
- [ ] Preview numbers match the web for the same player and drill.
- [ ] Suggest a plan gives the same 3 players and drills as the web.
- [ ] Going to the toss creates a match and the toss screen shows "Your training: …" with the chosen drills.
