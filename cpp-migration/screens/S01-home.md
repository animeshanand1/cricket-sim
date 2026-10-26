# S01: Home (friendly setup)

| | |
|---|---|
| **Step** | S01 |
| **JS source** | `src/components/Home.jsx`, `HeroArt.jsx`, `InstallBanner.jsx` (dropped) |
| **C++ files** | `ui/screens/home_screen.{hpp,cpp}`, `ui/widgets/hero_art.cpp`, `ui/widgets/team_card.cpp` |
| **Depends on** | S00 |

## 1. Purpose

The landing screen. Two ways in: continue or start a **season**, or set up a **friendly** (your franchise, an opponent, a pitch) and go to team selection.

## 2. Navigation

| In | Out |
|----|-----|
| App start; nav Home; brand logo; "New fixture" after a friendly | "Play a season / Continue season N" → `season` · "Pick your XI" → `xi` (friendly) |

## 3. Wireframe

```
┌─────────────────────────────────── HERO (stadium art) ───────────────────────────────────┐
│ T20 FRANCHISE CRICKET                                                                    │
│ CAPTAIN IT  BALL BY BALL          (h1; "ball by ball" in gold)                           │
│ Ten franchises. Pick your XI, train your players, win the toss, then make every call...  │
│ ┌──────────────────────────────┐ ┌──────────────────────────────┐                        │
│ │ 🏆 Continue season 3          │ │ ● Friendly match             │  ← mode cards          │
│ │ IPL rules: 14 league games…   │ │ Any two teams, any pitch…    │                        │
│ └──────────────────────────────┘ └──────────────────────────────┘                        │
└──────────────────────────────────────────────────────────────────────────────────────────┘
FRIENDLY: YOUR FRANCHISE          Batting and bowling ratings are the average of each squad's best…
┌──────────────┐┌──────────────┐┌──────────────┐┌──────────────┐┌──────────────┐
│[⛨] Chennai…  ││[⛨] Mumbay…   ││ …            ││              ││              │  ← 10 team cards
│ Chepok · Dus…││ Wankade · Fl…││              ││              ││              │    (radio group)
│ BAT 78 BWL 80││ BAT 81 BWL 79││              ││              ││              │
└──────────────┘└──────────────┘└──────────────┘└──────────────┘└──────────────┘
OPPONENT                                    PITCH
(chip)(chip)(chip)… 9 other teams           (Surprise me)(Good cricket wicket)(Flat deck)(Green top)(Dusty turner)(Slow and low)
                                            "Even bounce and a bit for everyone. Par score around 175."
───────────────────────────────────────────────────────────────────────── [ Pick your XI › ]  ← sticky
```

## 4. Sections and widgets

| Section | Widget | Data | Interaction |
|---------|--------|------|-------------|
| Hero | `HeroArt` (night stadium SVG), eyebrow, h1, lede | static text | — |
| Mode card 1 (primary) | big button, trophy icon | label "Continue season {n}" if `season.season`, else "Play a season" | `goTo(season)` |
| Mode card 2 | big button, ball icon | — | scroll to the Friendly section |
| Install banner | — | — | **dropped** |
| Your franchise | grid of `TeamCard` (radio) | each team from `derived.league()`: badge, name, `ground · PITCHES[pitch].label`, `teamRatings` BAT / BWL | `chooseTeam(id)` |
| Opponent | `ChipRow` (radio) of the 9 other teams with a small badge | `setup.oppTeamId` | `selectOpponent(id)` |
| Pitch | `ChipRow` (radio): "Surprise me" + 5 pitch labels | `setup.pitch` | `setPitch(key)` |
| Pitch report | paragraph | "The groundsman keeps it secret until the toss." for random, else `PITCHES[pitch].report` | — |
| Sticky CTA | primary button "Pick your XI ›" | — | `clearFixture()`, `goTo(xi)` |

`teamRatings(team)` = average of the best 7 batting ratings and best 5 bowling ratings in the current (developed) squad.

## 5. Rules

- Choosing your franchise: if the opponent is the same team, the opponent becomes the first other team.
- `chooseTeam` restores that team's saved line-up when all 11 are still in the squad, else auto-picks.
- The team card selected state: gold border + gold glow; hover lifts the card 2 px and tints the border with the team colour.

## 6. View state

None (all choices are in `setup` and saved in `setup.json`).

## 7. C++ notes

- Team grid: `ImGui::BeginTable` with as many columns as fit (`floor(width / 220)`), or manual wrap.
- Mode cards: custom button with icon disc, bold title, hint text.
- Hero art: rasterise `hero.svg` at the hero panel's size; draw text on top; on resize re-rasterise (debounced).
- "Scroll to Friendly": `ImGui::SetScrollHereY` on a marker before the section, with a short smooth-scroll tween.

## 8. Acceptance checklist

- [ ] All 10 teams show with correct colours, grounds, home pitch labels and BAT/BWL ratings (compare with the web game for the same save).
- [ ] Choosing a team updates the selection, keeps a valid opponent, and restores that team's saved XI.
- [ ] Opponent chips exclude your team.
- [ ] Pitch report text changes with the chip; "Surprise me" shows the secret line.
- [ ] Primary mode card says "Continue season N" when a season exists.
- [ ] "Pick your XI" opens the Playing XI screen as a friendly (no fixture banner).
