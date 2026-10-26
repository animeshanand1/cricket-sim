# 07: Persistence (saves)

## 1. What the web game saves today

| localStorage key | Content | Written | Loaded by |
|------------------|---------|---------|-----------|
| `t20coach.career.v2` | whole `career` slice | when `career` changes | `loadCareer()`: accepts if it has `players` and an array `recorded`; merges onto `emptyCareer()` so new fields get defaults |
| `t20coach.season.v2` | `{ season, history }` | when either changes | `loadSeason()`: accepts if it has `season` and an array `history`; `busy` reset |
| `t20coach.world.v1` | whole `world` | when `world` changes | `loadWorld()`: accepts if it has `players` and `teams`; `upgradeWorld()` to version 3 |
| `t20coach.setup.v2` | `{ userTeamId, oppTeamId, pitch, lineups, matchSettings }` | when `setup` changes | at import time of `setupSlice.js` |
| `t20coach.edits.v2` | legacy edit overlay | never (v0.8 only) | only by `migrateWorld()` when no world save exists |
| `t20coach.ui.<key>` | `'1'`/`'0'` or a string | by widgets | by widgets |

Not saved: the match in progress, the current screen, the open player card, `season.busy`.

Corrupt or missing data → start fresh for that slice, never crash.

## 2. C++ save layout

> **Built in step A5 (24 Oct 2026)**: `t20-coach-cpp/src/app/save_manager.{hpp,cpp}` with the JSON in
> `src/engine/state_json.cpp`. In place: the four save files, `meta.json`, atomic writes with `.bak`,
> fallback to `.bak` then to a fresh start, autosave at most once a second, a save after every match
> and on quit, `logs/t20coach.log`. Verified both ways (a browser save loads in C++; a C++ save loads
> in the browser game and plays on). Not yet: `match.json` (the match in progress) and `settings.json`.

Folder: `SDL_GetPrefPath("T20Coach", "T20Coach")` (Windows: `%APPDATA%\T20Coach\T20Coach\`).

```
saves/
  slot1/
    career.json      ← t20coach.career.v2   (same JSON)
    season.json      ← t20coach.season.v2   (same JSON)
    world.json       ← t20coach.world.v1    (same JSON)
    setup.json       ← t20coach.setup.v2    (same JSON)
    match.json       ← NEW: the match in progress (so quitting mid-match is safe)
    meta.json        ← NEW: { "format": 1, "gameVersion": "3.0.0", "savedAt": "...", "team": "RJR", "season": 4 }
settings.json        ← t20coach.ui.* + window size, UI scale, theme
logs/t20coach.log
```

- One slot to start (`slot1`). The folder structure allows more slots later (a "Load career" screen is an optional extra, not part of the port).
- **Same JSON as the browser.** Keep every field name and shape (including the array rows in season summaries and `matchups`). Two benefits: the C++ save can be diffed against the JS, and a browser career can be imported.

## 3. When to write

The JS writes on every change. A desktop game should not hit the disk on every ball:

| Slice | Write policy |
|-------|--------------|
| `world`, `career`, `season`, `setup` | Debounced: mark dirty on change; write at most once a second from `Game::update()`; always write immediately after a match completes, a season starts or ends, and an off-season stage closes |
| `match` | Write at the end of each over, at the break, on a pending decision, and on quit. Delete `match.json` when the match is cleared |
| `settings` | On change (tiny) |
| All | Flush on quit (`SDL_EVENT_QUIT`) |

**Atomic writes:** write `world.json.tmp`, flush, then rename over `world.json` (`std::filesystem::rename`; on Windows use `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`). Keep the previous file as `world.json.bak`.

Size: a world after many seasons holds a few thousand players with histories; expect a few MB. Write JSON compactly (`dump()` without indent) in release, indented in debug.

## 4. Loading and upgrading (boot, flow F1)

```cpp
GameState SaveManager::loadOrCreate() {
  GameState s;
  s.career = readJson<Career>("career.json").value_or(emptyCareer());   // merge onto defaults
  s.season = readJson<SeasonState>("season.json").value_or(SeasonState{});
  if (auto w = readJson<World>("world.json")) s.world = engine::upgradeWorld(*w);
  else s.world = engine::createWorld(engine::generateLeague());          // no legacy edits on desktop
  s.setup  = setupFrom(readJson<SetupSave>("setup.json"), s.world);      // defaults: LEAGUE[0] v LEAGUE[5], 'random'
  if (auto m = readJson<Match>("match.json")) s.match = *m;              // NEW
  return s;
}
```

Rules carried over from JS:
- Merge loaded career onto `emptyCareer()` so fields added in later versions get defaults (`records`, `popularity`, `matchups`, `milestones`, `manager`…). With nlohmann, give every struct field a default and use `j.value("field", default)` in `from_json`.
- `upgradeWorld`: version < 3 → add `auction`, `clubs`, `manager` if missing; give unsigned squad players contracts; convert a pre-v1.2 draft into an off-season. Port it exactly.
- Missing `world.transfers` → empty (added in v2.2 without a version bump).
- After loading, if `setup.xi` is empty or has anyone not in the user's world squad → `autoPick()`.
- If a loaded `match.json` refers to players or teams the world no longer has (should not happen), discard it.

## 5. Importing a browser career (optional but cheap)

Because the schema is identical, add **Import from browser** to the main menu (or a `--import` flag):

1. In the web game, open the browser dev tools console and run:
   ```js
   copy(JSON.stringify(Object.fromEntries(['career.v2','season.v2','world.v1','setup.v2'].map(k => [k, JSON.parse(localStorage.getItem('t20coach.' + k))]))))
   ```
2. Paste into a file `t20coach-export.json`.
3. The C++ importer reads the four keys and writes the four save files, then runs the normal load (with `upgradeWorld`).

(No change to the JS code is needed for this.)

## 6. Settings file

```json
{
  "commentaryOn": true, "commentary": true, "commentaryKeyOnly": false,
  "fieldView": true, "fielders": true,
  "watchSpeed": "normal", "watchStopWicket": true,
  "fold.field": false, "fold.target": false,
  "window": { "w": 1440, "h": 900, "x": 100, "y": 80, "maximized": false },
  "uiScale": 1.0, "theme": "dark"
}
```

Desktop defaults: commentary and field view **expanded** (the web build collapses them under 720 px wide).

## 7. Reset actions (must clear the right files)

| UI action | Clears |
|-----------|--------|
| Players → Reset career stats | `career` → `emptyCareer()` (squads, ages, retirements stay) |
| Season hub → Abandon season | `season.season = null` (history, career and development stay) |
| Players → Restore all edited players | `undoEdit` on every edited player in `world` |
| *New (desktop):* Main menu → New career | Delete the slot folder, then boot fresh (ask for confirmation) |
