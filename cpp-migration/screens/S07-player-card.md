# S07: Player card (modal)

| | |
|---|---|
| **Step** | S07 |
| **JS source** | `src/components/PlayerCard.jsx` (`PlayerCard`, `PlayerLink`, `PlayerAvatar`, `currentMatchLine`), `SkillBreakdown.jsx`; engine `career.js` (`summarize`, `formOf`), `world.js` (`potentialView`, `potentialGrade`, `ratingTrend`, `growthRoom`), `milestones.js` (`nextMilestone`), `ageing.js` (`agePhase`), `fitness.js`, `special.js` |
| **C++ files** | `ui/screens/player_card_modal.{hpp,cpp}`, `ui/widgets/skill_breakdown.cpp`, `ui/widgets/player_link.cpp` |
| **Depends on** | S00 (can be built any time after; every screen links to it) |

## 1. Purpose

A modal stats card for any player (squad players, free agents, retired players, auction lots, draft prospects). Opened from any player name (`PlayerLink`) anywhere in the game. Two tabs: Skills and Career stats. **Edit** switches the card into the editor ([S21](S21-player-editor.md)).

## 2. Opening and closing

| Open | Close |
|------|-------|
| Click any `PlayerLink` → `openCard(id)` · Players page pencil → `openEditor(id)` (card opens in edit mode) | ✕ button, click on the backdrop, Esc (in the editor, Esc cancels the edit instead) → `closeCard()` |

Focus moves to the close button on open and back to the opener on close.

## 3. Wireframe

```
┌────────────────────────────────────────────────────────────────────────────────┐
│ ▰▰ (team-colour gradient header with diagonal stripes)                          │
│ (avatar 72)  [⛨] RAJASTHAN REGALS · Overseas · Retired   [★ Make captain] [✎ Edit] [✕] │
│              YASHASVI JAISWALL                                                  │
│              Batter · Left-hand bat · Off-spin · Age 24 [PRIME]                 │
├────────────────────────────────────────────────────────────────────────────────┤
│   82          18          74       (Anchor) (Off-spin 104 kph) (Special: Arm ball)│
│ BATTING    BOWLING    FIELDING     (Popularity 64%) (Next: 14 runs…) (Club captain) (Created) (Edited)│
├────────────────────────────────────────────────────────────────────────────────┤
│ ✔ Changes saved. They apply from his next match.        ← flash after edit/restore│
│ THIS MATCH  45* (31) · 1/22 (2 ov) · 1 ct · trained: Range hitting · at the crease, confident │
│ AGE 24  Prime: at his peak… · since season 1: Batting 78 → 82                    │
│ POTENTIAL  A (84) · room to grow · draft pick #3, season 2                       │
│ FITNESS  82 · Fit · Injured: Hamstring, out for 2 more matches · tired players…  │
│ FORM  In form · last 3 innings 45, 12, 67 · form sets how confident he feels…    │
│ [Skills] [Career stats]                                                          │
│ … tab content …                                                                  │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 4. Content

| Section | Content (data) |
|---------|----------------|
| Header | Avatar 72 (team colour; free agents grey `#56657a`), eyebrow: small badge + team name (`teamOf(id)`, or "Free agent") + `· Overseas` + `· Retired`; name (h2, condensed italic uppercase); meta `Role · Left/Right-hand bat · {bowling type label if bowl ≥ 25} · Age {age} {PhaseTag}`. Buttons: **Make captain** (in a squad, not retired, not already club captain, not while editing) → `makeClubCaptain(id)` + flash, **Edit** (not for retired players, not while editing), **✕** |
| Headline | Big numbers Batting / Bowling / Fielding; chips: bat style, bowl style, `{kph} kph` (bowl ≥ 35), `Special: {label}` (bowl ≥ 35), `Popularity {v}%` (if any), `Next: {nextMilestone text}` (has career, not retired), `Club captain` (gold outline, if `clubCaptainOf(world, team) == id`), `Created` (gold, `origin == custom`), `Edited` (gold) |
| Flash | after save: "Changes saved. They apply from his next match." (+ the captaincy sentence if the box changed) · after restore: "Original name, age and skills restored." · after Make captain: "He is club captain now, and in your XI for the next match." / "…and leads the side once he is fit again." (injured) / "He is club captain now." (another club) |
| This match / Last match | Only if he is in the match on screen: per innings bat line `45* (31)`, bowl line `1/22 (2 ov)`, catches `1 ct`, `trained: {drill}`, `at the crease, {confidence label}` (live) |
| Age | Retired: "Retired · After season {y}, aged {a}". Else `Age {age}` + phase label + note + trend since the first history point: `since season {y+1}: Batting 78 → 82, …` (only changed ratings; bowling only if ≥ 35) |
| Potential | not retired: exact `A (84)` or range `B to A (76-90, not scouted)` + `room to grow` (growth room > 0.2, age ≤ 29) / `close to his ceiling` / `past the growing years` + `academy graduate` / `draft pick #n, season s` |
| Fitness | in a squad: `{fitness} · {label}` + injury `Injured: {label}, out for N more matches` |
| Form | `formOf(career)` label (Red hot / In form / Steady / Out of form / No recent innings) + `last N innings a, b, c` |
| Tabs | Skills (default) / Career stats |

### Skills tab (`SkillBreakdown.jsx`)

Three blocks with the overall and its development delta (▲/▼ vs `base`):
- **Batting** (sub: hand · bat style): groups Technique / Scoring / Mind, each skill bar with weight %, value, ▲/▼ development delta, help text.
- **Bowling** (sub: type label · kph · style · "part-time" if bowl < 35, or "Does not bowl"): pace or spin skills with weights; pace label shows kph; muted when part-time.
- **Fielding** (sub for keepers: "glovework counts for more than half"): catching, ground, throwing, (+ keeping for keepers).
- Footer note explaining arrows and weights.

Bar colour tiers: ≥ 80 high (green), ≥ 60 mid, else low.

### Career stats tab

- Header: `{m} matches · {w} won · {potm}× player of the match` or "No matches yet".
- Hero row: Runs, Bat avg, Strike rate, Wickets, Economy.
- Batting table: Inns, NO, Runs, HS, Avg, SR, 50s, 100s, 4s, 6s, Ducks (or "Yet to bat").
- Bowling table: Inns, Overs, Wkts, Best, Avg, Econ, SR, Dots, Mdns, 4w+ (or "Yet to bowl").
- Fielding line: catches, stumpings, run outs, dropped.
- Recent form: last 5 matches: result letter (W/L/T coloured), `v {team}`, bat line or "did not bat", bowl line.
- Empty: "Career numbers build up from every match {first name} plays in, whether you captain it ball by ball or sim it."

## 5. Data read

`ui.cardPlayerId`, `ui.cardEdit`, `career.players[id]`, `career.popularity[id]`, `match` (for the live line), `derived.player(id)` (developed), `derived.teamOf(id)`, `world` (potential view, trend), `setup.userTeamId` (scouting: own players exact).

## 6. C++ notes

- `ImGui::OpenPopup` + `BeginPopupModal` with a custom backdrop colour (`ImGuiCol_ModalWindowDimBg = rgba(3,7,12,.72)`), width `min(720, viewport − 24)`, scrollable body.
- Header gradient: `AddRectFilledMultiColor` from 80% team colour to `#07101a`, plus faint diagonal stripes (thin lines at 115°).
- Use the developed player (`derived.player`) so ratings include match development; `p.base` holds the pre-development values for the ▲/▼.

## 7. Acceptance checklist

- [ ] Opens from every kind of player name (squad table, scorecard, commentary-free lists, auction lot, draft prospect, hall of fame); closes by ✕, backdrop and Esc.
- [ ] Free agents show "Free agent" and grey colours; retired players show the retired line and no Edit.
- [ ] Ratings, chips, potential view (range vs exact for your own players), fitness, form match the web.
- [ ] Skills tab ▲/▼ appear after a few matches of development.
- [ ] Career tab numbers match the web after the same matches.
- [ ] Live line updates during a match.
