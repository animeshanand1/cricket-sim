# S21: Player editor

| | |
|---|---|
| **Step** | S21 (last screen) |
| **JS source** | `src/components/PlayerEditor.jsx` (`draftFrom`, `Delta`, `SkillRow`, `PlayerEditor` with `mode` edit/create); engine `editor.js` (`editableGroups`, `overallsFor`, `convertBowling`, `validateEdit`, `writeEdit`, `undoEdit`, `newPlayerTemplate`, `validateNewPlayer`, limits), `world.js` (`createPlayer`, `clubCaptainOf`, `setClubCaptain`), `league.js` (`withCaptain`), thunks `createNewPlayer`, `makeClubCaptain`, `removeClubCaptain`, `special.js` (`SPECIALS`, `specialsFor`, `specialOf`), `league.js` (`BOWL_TYPES`, `ROLE_LABEL`) |
| **C++ files** | `ui/screens/player_editor.{hpp,cpp}` (drawn inside the Player card modal) |
| **Depends on** | S07 |

## 1. Purpose

Edit any active player's name, age, role, batting hand, bowling type, special delivery and every sub-skill, and make him club captain. Saved into the world; applies from the next match; development restarts from the new numbers; career stats stay with the player.

The same form in **create mode** designs a brand-new player for a squad (opened from Players → New player, S20); see §7.

## 2. Navigation

Opened from the Player card **Edit** button or the Players page pencil (`openEditor`). **Save changes** / **Cancel** / **Restore** return to the card view (with a flash message on save/restore). Esc cancels.

## 3. Wireframe (inside the card modal, under the header)

```
EDIT PLAYER
Saved in this browser. Changes apply from the next match (the one in progress keeps his
old numbers), and his development restarts from the new numbers. Career stats stay with him.
Name [Yashasvi Jaiswall            ]   Age [24]
Role [Batter ▾]  Batting hand [Left-hand bat (LHB) ▾]  Bowling type [Off-spin (OB) ▾]  Special delivery [Auto: Arm ball ▾]
The role decides where the team picks and bats him. Switching between pace and spin swaps…
[☐ Overseas player] (create mode only)  [☑ Club captain] (players in a squad)
The club captain plays whenever he is fit, leads the side and gains extra experience every match.
   84 ▲2         18           74            ← live headline ratings (tiers coloured), delta vs saved
 BATTING      BOWLING      FIELDING
Overall ratings are the weighted mix of the skills below and update as you edit.
┌ BATTING ─────────────────────────────────────────────── [−5 all] [+5 all] ┐
│ Defence 17%        ━━━━━━━━━━━━━●━━━━━   [ 82 ] ▲3                          │
│ Against pace 10%   …                                                        │
│ … 9 skills                                                                  │
└────────────────────────────────────────────────────────────────────────────┘
┌ BOWLING (SPIN) ── 4 skills ─┐   ┌ FIELDING ── catching, ground, throwing, wicketkeeping (keepers only) ─┐
⚠ Some values need fixing before you can save.
[✔ Save changes] [Cancel] [Undo changes]                         [↶ Restore original]
                                          (confirm: "Go back to his original name, age, role, types and skills?" [Restore] [Keep])
```

## 4. Fields and rules

| Field | Control | Rule (`validateEdit`) |
|-------|---------|-----------------------|
| Name | text | trimmed, inner spaces collapsed; 1–24 chars ("Enter a name." / "Keep it to 24 characters.") |
| Age | number | 16–50 ("Age must be 16 to 50.") |
| Role | combo: Batter, Wicketkeeper, All-rounder, Bowler | must be valid |
| Batting hand | combo RHB / LHB | |
| Bowling type | combo grouped Pace (RF, RFM, RM, LF, LFM, LM) / Spin (OB, LB, SLA, LWS) / "Does not bowl" | Switching pace ↔ spin converts the bowling skills at his current level (`convertBowling`: accuracy and variations carry over, the rest start at his overall) and resets the special if the new type can't bowl it |
| Special delivery | combo: "Auto: {best-skill special}" + the specials his type can bowl; disabled when none | invalid for the type → cleared |
| Skills | per group (Batting 9, Bowling 6 pace / 4 spin, Fielding 4): label + weight %, slider 1–99 (coloured by tier), number box, ▲/▼ vs the value before this edit | each 1–99 ("1 to 99") |
| −5 all / +5 all | per group | shifts every skill in the group, clamped 1–99 |
| Club captain | checkbox, shown when he has a team; ticked if `clubCaptainOf(world, team) == id` | saved separately from the skills (see §5) |

Live headline ratings (`overallsFor`) update as you type (invalid numbers fall back to the stored value). Fielding "Wicketkeeping" shows "keepers only" for non-keepers.

## 5. Actions

| Button | Behaviour |
|--------|-----------|
| Save changes (enabled when changed) | Only if anything other than the captain box changed: `savePlayerEdit(id, draft)`: on errors, show them per field and focus the first; on success `playerEdited` (store `preEdit` the first time, write name/age/role/hand/type/style/special/skills, recompute) + `progressCleared(id)`; back to the card with "Changes saved. They apply from his next match." Then, if the captain box changed: ticked → `makeClubCaptain(id)`, unticked → `removeClubCaptain(team)` (ticking the box alone must not mark him Edited or clear his development) |
| Cancel | back to the card |
| Undo changes | reset the form to how it opened |
| Restore original (only for edited players) | inline confirm → `restorePlayer(id)` (`undoEdit`: original name, types, skills; age = original + years since the edit) → "Original name, age and skills restored." |

## 6. C++ notes

- Keep the draft as strings for the numeric inputs (so a half-typed value is allowed) and validate on save, as the JS does.
- `ImGui::SliderInt` + `InputInt` side by side per skill; colour the slider grab by tier.
- A match in progress holds its own copies of players, so editing during a match is safe; say so in the hint (`inLiveMatch`).

## 7. Create mode and the club captain

**Create mode** (`mode = create`, `teamId`): the form opens on `newPlayerTemplate("BAT")` with an empty name. Heading "Create a player"; hint "He joins the squad straight away on a two-year contract and can play from the next match. Pick a role first: the skills start at a typical level for it…". No ▲/▼ deltas, no Undo or Restore; the button reads **Create player** and is always enabled.

- Changing the role re-seeds the skills and bowling type from `newPlayerTemplate(role)` until the user touches a skill, the bowling type or a ±5 button.
- Starting levels (`STARTER`, fixed seed so the form always opens the same): BAT 62 bat / 20 bowl RM; WK 58 / 5 / no bowling; AR 56 / 58 RFM; BOWL 22 / 64 RF.
- `createNewPlayer(teamId, draft)`: form-level errors (`errors.form`, shown in the red notice): unknown team, off-season running, squad full (25). Field errors from `validateNewPlayer` = `validateEdit` rules plus "Another player already has this name." (case and spaces ignored; retired players don't count).
- `createPlayer(world, team, edit)`: id `P{nextId++}`; skills from the edit; batting style from `deriveSkills` on his final headline numbers (seed `2014 + year`); bowling style `bowlStyleFor`; `origin = custom`, `joined = year`, `overseas` from the box, `potential = potentialFor`, one history point, contract `contract(basePrice(p), 2, "custom", year, overseas)`. Not `edited`: editing him later works as for anyone, and Restore goes back to how he was created.
- Ticked **Club captain** → `makeClubCaptain(newId)` after creating.

**Club captain** (`WorldTeam::captainId`):

- `clubCaptainOf(world, team)` returns the id only while he is still in that squad, not retired. A traded, released or retired captain lapses with no clean-up step.
- `worldTeams` puts `captainId` on the team (only when set). `pickBestXI` brings a fit captain in via `withCaptain` (he replaces the weakest player of his own role; never the keeper unless he is a keeper, who then takes the gloves; an overseas captain replaces an overseas player when the XI already has 4), then re-sorts the batting order. `buildSide`: named captain if playing → club captain if playing → `pickCaptain`. This applies to every club, simmed or played.
- `makeClubCaptain(id)` for the user's club: if he is in the XI only `setCaptain`; else (fit) `withCaptain` on the current XI + `setXI`; injured → captain named, XI untouched. Returns `{ ok, inXI, injured }`.
- Parity: with no captain set nothing changes, so the golden files are unaffected (checked 2026-10-24).

## 8. Acceptance checklist

- [ ] Every validation message appears on the right field; Save stays disabled until something changes.
- [ ] Headline ratings update live and match `overallsFor`.
- [ ] Switching a fast bowler to off-spin converts his bowling at about the same overall; special list changes to spin specials.
- [ ] After saving: the card shows the new numbers and "Edited"; development arrows are cleared; the next match uses the new skills; career stats unchanged.
- [ ] Restore original brings everything back (age moved on by seasons passed).
- [ ] Players → Restore all edited players restores every edited player.
- [ ] Create mode: role change re-seeds the skills until one is touched; duplicate name, bad age and full squad are refused with the right message; the new player appears in the squad table, his card, Pick XI and the next match.
- [ ] Club captain box: ticking makes him captain and puts him in the XI (unless injured); unticking clears it; ticking alone doesn't mark him Edited.
- [ ] The club captain stays in auto-picked XIs, leads played and simmed matches, and lapses when he leaves the club.
