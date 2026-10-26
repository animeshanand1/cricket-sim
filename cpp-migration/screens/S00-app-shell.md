# S00: App shell and navigation

| | |
|---|---|
| **Step** | S00 (after A1–A6) |
| **JS source** | `src/main.jsx`, `src/App.jsx` (`NavBar`, `App`), `src/components/ui.jsx` (`Stepper`), `src/components/PlayerCard.jsx` (modal host), `src/components/InstallBanner.jsx` (`UpdateToast`) |
| **C++ files** | `src/main.cpp`, `ui/app.{hpp,cpp}`, `ui/widgets/nav_bar.cpp`, `ui/widgets/stepper.cpp`, `ui/screens/screen.hpp` |
| **Depends on** | A1 (window), A5 (`Game`), A6 (theme and widgets) |

## 1. Purpose

The frame every screen sits in: the window, the top navigation bar, the router that picks the screen from `setup.screen`, the setup stepper, the player-card modal, toasts, and the main loop.

## 2. Wireframe

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ [◆] T20 COACH        Season 3 · 2nd · 12 pts      [⌂ Home] [🏆 Season] [⛨ Club] [● Match•] [👥 Players] │  ← NavBar (sticky)
├──────────────────────────────────────────────────────────────────────────────────────┤
│  (1 Teams) ─ (2 Playing XI) ─ (3 Training) ─ (4 Toss) ─ (5 Match)    ← Stepper: only on xi, training, toss │
│                                                                                      │
│                         <current screen, max 1240 px wide, centred>                  │
│                                                                                      │
└──────────────────────────────────────────────────────────────────────────────────────┘
            ┌─────────────── Player card modal (when ui.cardPlayerId) ───────────────┐
            └────────────────────────────────────────────────────────────────────────┘
```

## 3. NavBar

| Element | Shows | Click |
|---------|-------|-------|
| Brand mark + "T20 **Coach**" | Always | `goTo(home)` |
| Season label | Only when a season exists: `Season {n} · {pos}{ordinal} · {pts} pts` from `pointsTable(season)` for the user team | — |
| Home | icon `home` | `goTo(home)`; current when view ∈ {home, xi, training} |
| Season | icon `trophy` | `goTo(season)`; current when view ∈ {season, offseason, sacked} |
| Club | icon `shield` | `goTo(club)`; current when view = club |
| Match | icon `ball`; red live dot when a match exists and is not complete | `goTo(toss)` if `match.status == toss`, else `goTo(match)`; disabled with no match; current when view ∈ {match, toss} |
| Players | icon `users` | `openPlayers()` (remembers `returnTo`); current when view = players |

Current link: `surface-3` background with a 2-px gold underline.

## 4. Router (`setup.screen` → view)

```cpp
View resolveView(const GameState& s) {
  switch (s.setup.screen) {
    case Screen::draft: case Screen::offseason: return View::offseason;
    case Screen::club:    return View::club;
    case Screen::sacked:  return View::sacked;
    case Screen::season:  return View::season;
    case Screen::players: return View::players;
    case Screen::match:   return s.match ? View::match : View::home;
    case Screen::toss:    return s.match ? View::toss  : View::home;
    case Screen::xi:      return View::xi;
    case Screen::training:return View::training;
    default:              return View::home;
  }
}
```

Each view is a class implementing `Screen`:

```cpp
struct Screen {
  virtual ~Screen() = default;
  virtual void onEnter(Game&) {}           // reset scroll and local state (see 6)
  virtual void draw(Game&, UiCtx&) = 0;    // called every frame
};
```

Screens are created once and kept (their local state such as the open tab survives, like React components that stay mounted). `onEnter` runs when the resolved view changes.

## 5. Modal host and toasts

- After the screen: if `ui.cardPlayerId` → draw the Player card modal ([S07](S07-player-card.md)).
- `UpdateToast` (web: "A new version is ready") is **dropped**. Keep a generic toast queue in `App` for messages such as "Saved", "Trade done", "Couldn't save: disk full".

## 6. Behaviours to keep

| Behaviour | JS | C++ |
|-----------|----|-----|
| New screen starts at the top | `scrollTo(0)` on view or screen change | `ImGui::SetScrollY(0)` on the main content child window in `onEnter` |
| Keyboard shortcuts | each screen adds its own `keydown` | `App` routes key events to the current screen's `onKey()`; ignore when a text field or the modal has focus |
| Window title | `T20 Coach` | `T20 Coach` (+ ` — Season 3` optional) |
| Quit | browser tab | Window close: flush saves, then exit. If a match is live it is already saved (see [07](../07-persistence.md)) |
| Command line `--screen season|club|players` | `?screen=` | Optional |
| About box | — | New: version, licences, save folder path, "Open save folder" button |

## 7. Main loop

See [03 §5](../03-target-cpp-architecture.md#5-main-loop). The content area is one ImGui window filling the viewport under the nav bar, with an inner child window (`ImGuiChildFlags_None`, vertical scroll) of max width 1240 px centred.

## 8. Acceptance checklist

- [ ] Exe starts in under 1 s and shows Home with the nav bar.
- [ ] Every nav link goes to the right screen and highlights correctly (test each view in the table above).
- [ ] Match link is disabled with no match; shows the live dot during a match; goes to the toss screen before the toss.
- [ ] Players → Back returns to the screen you came from (try from Season, Club, Match).
- [ ] The stepper appears only on Playing XI, Training and Toss, with done/current states right.
- [ ] Changing screens scrolls to the top.
- [ ] Closing the window and relaunching restores the career and the current season.
- [ ] With a season running, the nav shows `Season N · position · points`.
