# 08: UI design system

The web game has a "night-broadcast" look: a dark floodlit navy background, raised panels, gold for the one thing to press, team colours for identity, and a TV-style scoreboard that stays dark in both themes. Source: `src/styles.css` (1,500 lines), `src/components/ui.jsx`, `common.jsx`, `HeroArt.jsx`.

The C++ version reproduces this with Dear ImGui styling plus custom `ImDrawList` drawing. Everything below lives in `ui/theme.cpp` and `ui/widgets/`.

---

## 1. Colour tokens

Dark is the default (and the only theme you need for v1 of the port). Light is listed so it can be added later.

| Token | Dark | Light | Used for |
|-------|------|-------|----------|
| `bg` | `#060b12` | `#eef2f7` | Window background, with two soft radial glows: blue `rgba(74,140,255,.13)` top-left, gold `rgba(245,184,61,.07)` top-right |
| `surface` | `#0c1520` | `#ffffff` | Panels |
| `surface-2` | `#111d2b` | `#f4f7fa` | Buttons, tab strips, inset rows |
| `surface-3` | `#182839` | `#e8eef4` | Meter tracks, hover, selected tab |
| `line` | `#1c2d40` | `#dde5ee` | Borders |
| `line-strong` | `#2b4260` | `#c3d0dd` | Button borders |
| `ink` | `#e8eef5` | `#0c1724` | Main text |
| `ink-2` | `#b9c7d6` | `#33475b` | Secondary text |
| `muted` | `#8195aa` | `#5b6f84` | Hints, labels |
| `gold` | `#f5b83d` | `#f0ad2a` | Primary buttons, selection, current |
| `gold-ink` | `#1a1203` | `#1a1203` | Text on gold |
| `gold-text` | `#f5c563` | `#8f5d00` | Gold-coloured text |
| `gold-soft` | `rgba(245,184,61,.14)` | `rgba(240,173,42,.16)` | Selected chip background |
| `blue` | `#52abff` | `#1c6fd0` | Fours, overseas tag |
| `green` | `#37d39a` | `#0c9563` | Good, wins, done |
| `red` | `#ff5d62` | `#d6373d` | Wickets, errors, injuries, live dot |
| `purple` | `#a88bfa` | `#6a4bd4` | Milestones, Purple Cap |
| `orange` | `#ff9f43` | `#c86a0c` | Orange Cap, veteran |
| `series-1` | `#3987e5` | `#2a78d6` | Chart: side batting first |
| `series-2` | `#d95926` | `#eb6834` | Chart: chasing side |
| `board-bg` / `board-bg-2` | `#0a1622` / `#0f2031` | same | Scoreboard (always dark) |
| `board-ink` / `board-muted` / `board-line` | `#f2f6fa` / `#93a7bb` / `#203650` | same | Scoreboard text and lines |

Shadows: `0 12px 32px rgba(0,0,0,.35)` on panels, `0 24px 60px rgba(0,0,0,.5)` on the scoreboard and modal. In ImGui draw a soft shadow by stacking 3–4 translucent rounded rects under the panel (or skip shadows; borders carry most of the look).

Team colours come from `TEAMS[].color`. `shade(hex, k)` lightens (k > 0) or darkens (k < 0); `inkOn(hex)` picks `#0b1118` or white text by luminance. Port both (`ui.jsx`).

## 2. Typography

| Role | Font | Size (px at 100%) | Style |
|------|------|-------------------|-------|
| Body | Barlow 400/500/600/700 | 15, line height 1.5 | |
| Hint | Barlow 400 | 13.5 | `muted` |
| Eyebrow | Barlow 700 | 11.5 | UPPERCASE, letter-spacing 0.14em, `muted` |
| Panel title | Barlow Condensed 800 italic | 20 | UPPERCASE |
| h1 (page title) | Barlow Condensed 800 italic | 32–48 | UPPERCASE |
| Hero h1 (home, season start) | Barlow Condensed 800 italic | 40–67 | UPPERCASE, `em` part in gold |
| h2 / h3 | Barlow Condensed 700 | 23 / 18 | |
| Score (scoreboard) | Barlow Condensed 800 italic | 48–70 | tabular numbers |
| Board stat value | Barlow Condensed 800 | 28 | |
| KPI / hero number | Barlow Condensed 800 italic | 32 | |
| Moment banner | Barlow Condensed 800 italic | 36–58 | UPPERCASE, skewed −8° |

ImGui setup: load each weight/style you need as a separate `ImFont` at the sizes above (× DPI scale), with the `freetype` builder for sharp small text. Fonts are embedded in the exe (see [09](09-build-and-release-exe.md#assets)). ImGui has no letter-spacing: for eyebrows, draw glyph by glyph with an extra advance (helper `TextSpaced()`), or accept normal spacing.

`font-variant-numeric: tabular-nums` (Barlow has tabular figures) matters for scores and tables: right-align numbers.

## 3. Spacing and shape

| Token | Value |
|-------|-------|
| Panel radius | 14 px (`--r`), padding 18 px |
| Small radius | 9 px |
| Button | min height 42 (small 34), padding 0 18, radius 10, bold |
| Chip | pill, min height 38 (small 30) |
| Gaps | stack 12 px, large stack 26 px |
| Content width | max 1240 px, centred; side padding 28 px on desktop |
| Scoreboard radius | 18 px |
| Modal | max width 720, radius 18, backdrop `rgba(3,7,12,.72)` |

Layout breakpoints that matter on desktop: two-column match grid ≥ 980 px; Playing XI two columns ≥ 1000 px; scoreboard 3-column top row ≥ 820 px.

## 4. Icons

34 stroke icons (24-px grid, 2-px stroke, round caps) defined as SVG path strings in `ui.jsx` `PATHS`:

`home trophy users chart bat ball stumps play pause eye forward skip swap grip dumbbell coin calendar list star x minus edit check undo chevron back droplet target shield pitch flame bolt field whistle`

C++: at start-up wrap each path in `<svg viewBox="0 0 24 24"><path d="…" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>`, rasterise with nanosvg at 16, 20 and 34 px (× DPI) into one texture atlas, and draw tinted with the current text colour (`ImDrawList::AddImage` with a colour). API: `ui::Icon("trophy", size, color)`.

---

## 5. Widget catalog

Each widget is a free function in `ui/widgets/`. "JS" names the source.

### Identity

| Widget | JS | Look | C++ notes |
|--------|----|------|-----------|
| `TeamBadge(team, size)` | `ui.jsx TeamBadge` | Shield (path `M20 1.5 37 7v12.5C37 30.5 29.5 38.5 20 42.5 10.5 38.5 3 30.5 3 19.5V7z`, viewBox 40×44), diagonal gradient `shade(c,.25) → c → shade(c,-.45)`, white 35% outline, a highlight chevron, team id in Barlow Condensed 800 italic in `inkOn(c)`. Sizes xs 18, sm 24, md 40, lg 56, xl 84 (height ×1.1); no text below 22 px | Build the SVG string per team and size, rasterise with nanosvg once, cache the texture |
| `Avatar(player, color, size, showRole)` | `ui.jsx Avatar` | Circle with gradient `shade(color,.15) → shade(color,-.55)`, initials (first letter of first and last name) at 38% of size, a small role icon disc bottom-right (BAT bat, WK shield, AR star, BOWL ball) when size ≥ 30 | `AddCircleFilled` with a vertical gradient (two half-circles or `AddRectFilledMultiColor` clipped), text centred |
| `PlayerLink(id)` | `PlayerCard.jsx` | Player name as a link-styled button; opens the player card | `ImGui::TextLink`-style: underline on hover, click → `game.openCard(id)` |
| `Crest` | `common.jsx` | Alias of TeamBadge | — |

### Values and status

| Widget | JS | Look |
|--------|----|------|
| `Rating(value)` | `common.jsx` | Right-aligned bold number; ≥ 80 green, 60–79 ink, < 60 muted |
| `Meter(value, label, compact)` | `ui.jsx` | 7-px track (`surface-3`), fill gradient: ≥ 70 green, ≥ 45 gold, else red; label after |
| `FitnessTag(player)` | `ui.jsx` | Injured: red "Injured · Hamstring (3)". Fitness < 90: 40×5 bar + "Fit 82" / "Tired 66" (gold) / "Exhausted 52" (orange). ≥ 90: nothing |
| `PhaseTag(age)` | `.phase-*` | Uppercase pill: Rising green, Prime blue, Experienced gold, Veteran orange, Twilight red |
| `OsTag`, `QTag`, `IpTag` | `.os-tag`, `.q-tag` | "OS" blue outline; "Q" green fill; "IP" (Impact Player) |
| `StyleChip(text)` | `.style-chip` | Small grey pill (batting style, kph, special, popularity, "Edited" in gold) |
| `Tag(text, tone)` | `.tag` | Uppercase pill; tones: default (board), hot (gold), cond, weak (red tint), strong (green tint) |
| `FormPips(list)` | `.fp` | 20-px squares: W green, L grey, N dashed |
| `KpiTile(label, value, tone)` | `.kpi` | Eyebrow + big italic number; good green / bad red |
| `BallPip(ball)` | `Scoreboard.jsx BallChip` | 32-px pill on the dark board: dot `•` muted; runs plain; 4 blue outline + tint; 6 gold gradient + glow; W red gradient + glow; extras dashed; special delivery = gold outline. The newest pip pops in (scale 0.4 → 1, 0.35 s) |

### Structure

| Widget | JS | Look |
|--------|----|------|
| `Panel(title)` … `EndPanel()` | `.panel` | Surface, 1-px line border, radius 14, padding 18; optional title (panel-title style) and right-side header controls |
| `PageHeader(team?, eyebrow, title, subtitle, right)` | `ui.jsx` | Panel with the team badge (lg), eyebrow, h1, subtitle; with a team: diagonal gradient from 34% team colour to surface plus faint diagonal stripes |
| `Stepper(current)` | `ui.jsx` | Pills: number disc + label. Done: green disc. Current: gold border, gold-soft background, gold disc. Steps: Teams, Playing XI, Training, Toss, Match (off-season uses its own 3 steps) |
| `Tabs(items, current)` | `.tabs` | Inset strip (`surface-2`, radius 11, 4-px padding); current tab raised (`surface`, shadow); optional icon per tab; small variant |
| `Chip(label, selected)` / `ChipRow` | `.chip` | Pill radio buttons; selected = gold border + gold-soft |
| `Segmented(options, current)` | `.segmented` | Grid of equal cells in an inset strip; selected cell gold fill, gold-ink text |
| `Switch(label, value)` | `.switch` | Checkbox with gold accent + label |
| `Button(label, kind)` | `.btn` | default (surface-2, line-strong border, gold border on hover), primary (gold gradient `#ffd06a → gold`, gold-ink text, glow), quiet (transparent), small; disabled opacity 0.42 |
| `IconButton(icon)` | `.icon-btn` | Square icon button (minimise, close, swap, ↑ ↓, edit) |
| `Notice(text, kind)` | `.notice` | ok green / warn gold-soft / error red tint, radius 10 |
| `CollapsedPanel(title, summary, onExpand)` + `MinimizeButton` | `ui.jsx` | One-line panel with "Show"; used by field view and commentary |
| `Fold(id, title, summary)` | `CaptainPanel.jsx Fold` | Collapsible row with chevron, title and one-line summary; open state remembered in settings (`fold.<id>`) |
| `ConfirmInline(text, confirm, cancel)` | `.reset-confirm` | Warning notice with two buttons (used for Abandon season, Reset stats, Release, Restore all) |
| `StickyActions` | `.sticky-cta` | Bottom action bar for the page's main button (Back / Next). In ImGui, draw it in a fixed footer child window |
| `SortableTable` | `SortableTable.jsx` | See below |
| `Modal` | `.modal` | Centred window over a dimmed backdrop; Esc closes; focus returns to where it was |

### SortableTable

ImGui tables do this natively: `ImGuiTableFlags_Sortable | ScrollY | RowBg | BordersInnerH`, `TableGetSortSpecs()`. Behaviour to keep:
- Columns: key, label, tooltip title, getter, formatter, numeric (right-aligned), `lowIsGood` (first click sorts ascending), `qualify` (only qualifying rows are ranked when sorted by that column, e.g. average needs 60 balls).
- First column is `#` rank. The sorted column's values are bold.
- Shows the top 10 with "Show all N" / "Top 10 only".
- Highlight the user's rows (`is-mine`: gold-tinted background).
- Footer hint: "Ranked by Avg: minimum 60 balls faced. Select a column heading to re-sort."
- Stable sort, nulls last, tie-break function per column.

---

## 6. Scoreboard and match graphics (custom drawing)

These are drawn with `ImDrawList`, not standard widgets. Details live in [S05](screens/S05-match-live.md).

| Graphic | Notes |
|---------|-------|
| Broadcast scoreboard | Dark gradient box, radius 18, team-colour slanted block behind the badge, giant score, stat columns separated by thin lines, a strip with the headline (gold italic uppercase), this over's pips, tags |
| Moment banner | Top-right of the scoreboard, skewed −8°, clipped parallelogram, gradient by kind: four `#1f6fd1→#52abff`, six `#f5b83d→#ffdf8a` (dark text), wicket `#b3242a→#ff5d62`, milestone `#6a4bd4→#a88bfa`, result `#0c9563→#37d39a`. Animation 1.9 s: slide in from +40 px (12%), hold, slide out to −20 px while fading |
| Win bar | Two flex segments in series colours with % on each side |
| Fans meter | Badge + count + trend arrow per side, a two-colour bar, totals line |
| Field view | 420×420 virtual canvas scaled to the panel: outfield radial gradient, mowing stripes, rope circle r 168, dashed inner circle r 86, pitch rectangle, OFF/LEG labels (mirrored for left-handers), shot lines from the striker's end, sixes as quadratic arcs with glow, fielders as gold-ringed dots, wickets as red crosses, saved shots dashed with a green ring. New shot "draws itself" (stroke dash animation) |
| Charts | Worm, Manhattan, win chance, table position: see [S06](screens/S06-match-result.md) and [S11](screens/S11-season-stats-analytics-awards.md). ImPlot or custom `ImDrawList` |
| Hero art | Night stadium SVG (`HeroArt.jsx`): rasterise once with nanosvg at the panel size; re-rasterise on resize |

## 7. Animations

All motion is short and decorative. Provide one global "reduce motion" setting that skips them.

| Animation | Duration | Where |
|-----------|----------|-------|
| Moment banner | 1.9 s | Scoreboard, when a ball is played one at a time (not when ≥ 15 lines arrive at once, i.e. simming) |
| Pip pop-in | 0.35 s | Newest ball pip |
| Coin flip | 0.9 s, rotateY 1800° with a hop | Toss |
| Modal in | 0.22 s, from 12 px lower, scale 0.98 | Player card |
| Meter width | 0.35 s ease | Confidence, rhythm, fitness |
| Shot draw | stroke dash to 0 | Field view newest shot |
| Live dot pulse | — | Nav "Match" link |
| Spinner | rotate | "Simulating…" busy line |
| Swap pulse | gold ring pulse | Playing XI swap source |

`ui/anim.cpp`: a `Tween { start, duration, ease }` keyed by an id, sampled each frame; the main loop keeps redrawing while any tween is active.

## 8. Formatting helpers (`core/jsmath.hpp`, `ui/format.cpp`)

| Helper | Output |
|--------|--------|
| `money(lakh)` | `₹1.6 cr`, `₹18 cr`, `₹75 L`, `–` for none (crore = lakh/100, two decimals, trailing zeros trimmed) |
| `oversText(balls)` | `12`, `12.3` |
| `ordinal(n)` | `1st 2nd 3rd 4th … 10th`. JS uses `['th','st','nd','rd'][n] ?? 'th'`, so only 1–3 get st/nd/rd; table positions never go past 10, so that is enough |
| `nrrText(v)` | `+0.432` / `-1.205` |
| `fmt(v, d)` | fixed decimals, `–` for null |
| `fmt_indian(n)` | `1,00,000` |
| `fmt_thousands(n)` | `10,000` |
| `surname(name)` | everything after the first word, or the whole name |
| `tempoName(t)` | Defend < 1.6, Careful < 2.5, Balanced < 3.35, Attack < 4.3, else All-out |
| `confidenceLabel` | ≥ 80 In the zone, ≥ 65 Confident, ≥ 45 Settled, ≥ 30 Tentative, else Nervy |
| `rhythmLabel` | ≥ 70 On fire, ≥ 55 In rhythm, ≥ 40 Steady, else Rattled |
| `fitnessLabel` | ≥ 90 Fresh, ≥ 75 Fit, ≥ 60 Tired, else Exhausted |
| board `confidenceLabel` | ≥ 80 Delighted, ≥ 60 Happy, ≥ 40 Watching closely, ≥ 25 Concerned, else Your job is at risk |
| `fansMoodLabel` | ≥ 78 Roaring, ≥ 60 In full voice, ≥ 42 Watching closely, ≥ 25 Gone quiet, else Heading home |
| `potentialGrade` | ≥ 88 A+, ≥ 82 A, ≥ 76 B+, ≥ 70 B, ≥ 64 C+, ≥ 58 C, else D |
| `agePhase` | ≤ 23 Rising, ≤ 29 Prime, ≤ 32 Experienced, ≤ 35 Veteran, else Twilight |

All UI text is English, as in the web game. Keep strings in one place (`ui/strings.cpp`) if you plan to translate later.

## 9. Keyboard

| Key | Where | Action |
|-----|-------|--------|
| N or Space | Match, live, nothing awaited | Next ball |
| O | Match, live, nothing awaited | Next over |
| W | Match, live | Watch / pause |
| Esc | Match | Pause watching · Player card: close (in the editor: cancel the edit) |
| R / A | Match, review awaited | Review / Accept |

Ignore shortcuts while a text field has focus or a modal is open (JS checks for `[aria-modal]` and inputs).

## 10. Accessibility carried over

- Every icon-only button has a tooltip (the JS has `aria-label`/`title`).
- Keyboard navigation: enable `ImGuiConfigFlags_NavEnableKeyboard`.
- Colour is never the only signal: pips carry labels (4, 6, W), form pips letters, tags text.
- UI scale setting (90–150%) instead of browser zoom.
