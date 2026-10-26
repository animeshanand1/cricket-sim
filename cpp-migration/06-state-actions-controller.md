# 06: State, actions and the `Game` controller

Redux is replaced by one C++ class, `Game` (`state/game.{hpp,cpp}`), that owns `GameState` (see [04 §10](04-data-model.md#10-setup-ui-and-settings)). Every Redux action and every thunk becomes a `Game` method with the same name. Screens call these methods; they never edit state themselves.

After any method that changes a slice: bump that slice's revision, mark it dirty for saving (see [07](07-persistence.md)). After any method that can finish a match, run `afterMatchMutation()` (the listener).

---

## 1. `setup` actions (`setupSlice.js`)

| Action | Payload | What it does |
|--------|---------|--------------|
| `goTo` | screen | `setup.screen = screen` |
| `openPlayers` | — | Remember `returnTo = screen` (unless already on players), go to `players` |
| `closePlayers` | — | `screen = returnTo` (or `home`) |
| `selectUserTeam` | team | Set user team; if opponent is the same team, opponent = first other team; clear training; restore that team's saved line-up (if 11) or `pickBestXI` on the **generated** league and save it |
| `selectOpponent` | team | Set opponent (ignored if it is the user team) |
| `setPitch` | key or `'random'` | Set pitch |
| `autoPickXI` | — | Best XI on the generated league (unused by the UI; `autoPick` thunk is used instead) |
| `setXI` | `{ xi, keeperId, captainId }` | Set XI and keeper; captain = given if in XI, else current if still in XI, else none; clear training; save line-up |
| `setTraining` | `{ slot, playerId, drill }` | Put or remove a drill in slot 0–2; compact; max 3 |
| `clearTraining` | — | No drills |
| `prepareFixture` | `{ userTeamId, oppTeamId, pitch, fixture }` | Set up a season fixture; clear training |
| `clearFixture` | — | `fixture = null` |
| `togglePlayer` | id | Remove from XI (and from training, keeper, captain), or add if XI < 11; save line-up |
| `movePlayer` | `{ id, dir }` | Swap with the neighbour above (−1) or below (+1); save |
| `swapPlayers` | `{ id1, id2 }` | Both in XI: swap batting positions. One in XI: the other takes his slot (clears his training, keeper, captain); save |
| `reorderXI` | `{ fromIndex, toIndex }` | Move a player in the order; save |
| `setCaptain` | id | If in XI; save |
| `setKeeper` | id | If in XI; save |
| `updateMatchSettings` | `{ aggression?, autoBowl? }` | Merge into `matchSettings` |
| *(mirror)* | `match/setAggression`, `match/setAutoBowl` | Also copied into `setup.matchSettings` |

"Save line-up" = `lineups[userTeamId] = { xi, keeperId, captainId }`.

## 2. `match` actions (`matchSlice.js`)

Each one is a no-op when there is no match. All forward to `engine::match` on `state.match`.

| Action | Engine call |
|--------|-------------|
| `newMatch(payload)` | `createMatch(payload)`, then `match.fixture = payload.fixture` |
| `clearMatch` | `match = null` |
| `callToss(call)` | `callToss` |
| `chooseToss(decision)` | `chooseTossDecision` |
| `nextBall` | `playBall` |
| `nextOver` | `playOver` |
| `simInnings` | `simInnings` |
| `simMatch` | `simMatch` |
| `startChase` | `startSecondInnings` |
| `pickBowler(id)` | `setBowler` |
| `changeBowler` | `changeBowler` |
| `setBowlingPlan(plan)` | `setBowlingPlan` |
| `setField({ preset, positions })` | `setField` |
| `setBatterTarget({ bowlerId, mode })` | `setBatterTarget` |
| `pickBatter(id)` | `setNextBatter` |
| `setAggression(level)` | `settings.aggression = level; refreshMindset` (+ mirror to setup) |
| `setBatterTempo({ batterId, level })` | `setBatterTempo` |
| `impactSub({ side, out, in })` | `useImpactPlayer` |
| `setAutoBowl(on)` | `settings.autoBowl = on`; if on and waiting for a bowler, `resolvePending` (+ mirror to setup) |
| `reviewCall(review)` | `reviewDecision` |
| `setAutoReview(on)` | `settings.autoReview = on`; if on and waiting for a review, `resolvePending` |

`nextBall`, `nextOver`, `simInnings`, `simMatch`, `pickBatter`, `pickBowler`, `reviewCall`, `setAutoBowl`, `setAutoReview`, `impactSub` can complete the match → call `afterMatchMutation()`.

## 3. `career`, `season`, `ui` actions

| Slice | Action | What it does |
|-------|--------|--------------|
| career | `matchCompleted(match)` | `recordMatch(career, match, baseOfMatch)`; `baseOfMatch(id) = match.players[id].devBase ?? roster skills` |
| career | `progressCleared(id)` | Delete `progress[id]` |
| career | `resetCareer` | `emptyCareer()` |
| season | `seasonStarted(season)` | Set current season |
| season | `fixtureCompleted({ no, summary })` | `recordResult` (then `advancePlayoffs`) |
| season | `archiveSeason(entry)` | Push to history, season = null |
| season | `abandonSeason` | Season = null (career and development kept) |
| season | `setBusy(x)` | Progress label while simulating (not saved) |
| ui | `openCard(id)` | Show player card |
| ui | `openEditor(id)` | Show player card in editor mode |
| ui | `closeCard` | Hide |

## 4. `world` actions (`worldSlice.js`)

| Action | Guard | Engine call |
|--------|-------|-------------|
| `worldReplaced(world)` | — | Replace |
| `offseasonStarted({ order, userTeam, then })` | — | `runOffseason` |
| `contractRenewed({ id, years })` | off-season running | `renewPlayer` (price = `renewalPrice` with goodwill) |
| `contractReleased(id)` | stage contracts or retention | `releasePlayer` |
| `contractsClosed` | stage contracts | `closeContracts` (→ starts the draft) |
| `retentionSet(ids)` | stage retention | `setRetention` (validated) |
| `retentionClosed` | stage retention | `closeRetention` (→ starts the draft) |
| `auctionRun` | auction exists | `runAuction` |
| `auctionBid` | | `userBid` then `runAuction` |
| `auctionPass` | | `userPass` then `runAuction` |
| `auctionLimit(max)` | | `userLimit` then `runAuction` |
| `auctionAutoRest` | | `userAutoRest` then `runAuction` |
| `auctionRtm({ step, value })` | | `rtmInvoke` / `rtmRaise` / `rtmMatch`, then `runAuction` |
| `auctionClosed` | stage auction | `runAuction`; if all lots done, `closeAuction` |
| `matchFitness(workload)` | — | `applyMatchFitness`, `topUpFreeAgents`, `aiReplacements` (if a manager team) |
| `tradeMade({ team, other, offer })` | — | `makeTrade` |
| `replacementSigned({ team, injuredId, id })` | — | `signReplacement` |
| `seasonTargetSet({ team, target, rank, season })` | — | New club → history `moved`, confidence 60; set target; clear `sacked` |
| `boardMatchResult({ won, tied })` | — | `applyConfidence(matchNudge)` |
| `boardSeasonReview({ season, position, offers, order })` | — | `closeClubSeason`; verdict; red-ink penalty; history; clear target; sacked if ≤ 15 |
| `jobAccepted(team)` | — | History `sacked`; team; confidence 55; clear sacked |
| `clubSeasonStarted(team)` | — | `activateClub` |
| `clubGateTaken({ team, fixture, form })` | — | `recordGate` |
| `clubUpgradeOrdered({ team, key })` | — | `orderUpgrade` |
| `clubTicketsSet({ team, tier })` | — | `setTicketTier` |
| `clubPitchSet({ team, pitch })` | — | `setHomePitch` |
| `clubPrizeShareSet({ team, share })` | — | `setPrizeShare` |
| `clubStaffSet({ team, role, tier })` | — | `setStaff` |
| `clubCoachAssigned({ team, playerId, area, tier })` | — | `assignCoach` |
| `clubCoachRemoved({ team, playerId })` | — | `removeCoach` |
| `clubSponsorSigned({ team, slot, index })` | — | `signSponsor` |
| `prospectScouted(id)` | — | `scoutProspect` |
| `draftPicked({ team, id })` | — | `draftPick`, then `advanceDraft` (AI picks until the user's turn) |
| `draftFinished` | — | `finishDraft` (→ `beginAuction` when in an off-season) |
| `offseasonReportDismissed` | — | `lastOffseason = null` |
| `playerEdited({ id, edit })` | — | `writeEdit`, `recompute` |
| `playerRestored(id)` | — | `undoEdit`, `recompute` |
| `allEditsRestored` | — | Every edited player |

---

## 5. Thunks → `Game` methods (step by step)

### Friendly and match flow

**`chooseTeam(teamId)`**
1. `selectUserTeam(teamId)`.
2. If the saved line-up for that team has 11 players who are all in the world squad → `setXI(saved)`; else `autoPick()`.

**`autoPick()`** → `pickBestXI(current team in effectiveLeague, pitch or 'balanced')` → `setXI({ xi, keeperId })`.

**`startFixture()`**: see [02 F3](02-process-flows.md#f3-friendly-match). Uses `effectiveLeague`, `pickBestXI`, `aiTrainingPlan`, `buildSide` ×2, `createMatch`; then `clearTraining`, `goTo(toss)`.

**`rematch()`** → `goTo(training)`.

**`newGame()`** → copy `aggression`, `autoBowl` from the match to `matchSettings`; `clearMatch`; `clearFixture`; `goTo(home)`.

**`backToSeason()`** → same as `newGame` but `goTo(season)`.

### Season

**`startSeason(teamId)`**
1. `clubSeasonStarted(teamId)`.
2. `createSeason({ seed: random, number: history.length + 1, userTeam, league: effectiveLeague })`.
3. `seasonStarted(applyHomePitch(season, teamId, club))`.
4. `selectUserTeam(teamId)`; restore saved line-up if all in squad, else `autoPick()`.
5. `rank = strengthRank(league, teamId)`; `seasonTargetSet({ team, target: boardTarget(rank), rank, season: number })`.
6. `goTo(season)`.

**`takeGate(no)`** → if fixture `no` exists and its home team is the manager's team: `clubGateTaken({ team, fixture: { no, away, stage }, form: seasonForm(season, team) })`.

**`simFixtures({ includeUser = false, stopAfter = null, label = 'Simulating' })`**: becomes a `SimFixturesJob` started by this method and stepped in `Game::update()`.
- Refuse if a job is running or there is no season.
- Each step: `fx = nextFixture(season)`; stop if none, if it is the user's and `!includeUser`, or `fx.no > stopAfter`.
- `m = simFixture(fx, effectiveLeague, career.players, { matchups })`; `m.fixture = {...}`; `m.managed = userTeam` if it is the user's game.
- `matchCompleted(m)`; `matchFitness(matchWorkload(m))`; `summary = summarizeMatch(m)`; `fixtureCompleted({ no, summary })`; `takeGate(no)`; if user's game, `boardMatchResult`.
- Every 2 games update `busy`; at the end `busy = null`.
- Note: `effectiveLeague` must be recomputed every step (fitness and development change after each game).

**`playNextFixture()`** → see [02 F9](02-process-flows.md#playnextfixture).

**`nextSeason()`**
1. Only if the season has a champion.
2. `aw = seasonAwards(season, teamOf, ageOf)`; `pos` = user's league position.
3. `archiveSeason({ number, champion, runnerUp, userTeam, userPos: pos, orangeCap: { id, runs }, purpleCap: { id, wkts }, mvp: { id, pts } })`.
4. `ranking` = all teams sorted by `finalPosition`; `order` = reversed (worst first).
5. `boardSeasonReview({ season: number, position: finalPosition(user), offers: jobOffers(league, userTeam), order })`.
6. If sacked → `goTo(sacked)`; else `startOffseason({ order, then: 'season', userTeam })`.

**`acceptJob(teamId)`** → `jobAccepted(teamId)`, `selectUserTeam(teamId)`, `startOffseason({ order: sacked.order, then: 'season', userTeam: teamId })`.

### Off-season

**`startOffseason({ order = null, then = 'players', userTeam = null })`**
1. `me = userTeam ?? setup.userTeamId`.
2. `draftOrder = order ?? league sorted by (batting + bowling) ascending` (weakest first).
3. `offseasonStarted({ order: draftOrder, userTeam: me, then })`; `goTo(offseason)`.

**`scout(id)`** → `prospectScouted(id)`. **`makeDraftPick(id)`** → `draftPicked({ team: draft.userTeam, id })`.

**`completeDraft()`** → `draftFinished()`; if an auction now exists → `auctionRun()`; else `finishOffseason()`.

**`finishOffseason()`**
1. `then = offseason.then ?? 'players'`, `team = offseason.userTeam ?? setup.userTeamId`.
2. If an auction exists → `auctionClosed()`; if it still exists (not finished) → stop.
3. `autoPick()`.
4. `then == 'season'` → `startSeason(team)`; else `goTo(players)`.

### Editor

**`savePlayerEdit(id, draft)`** → `validateEdit(world player, draft)`; if errors return them (the editor shows them); else `playerEdited({ id, edit })`, `progressCleared(id)`, return ok.

**`restorePlayer(id)`** → `playerRestored(id)`.

### Transfers

**`proposeTrade(other, offer)`** → returns the verdict.
1. Refuse with a message if no season or the window is shut: "The trade window is shut."
2. Refuse if a match is in progress: "Finish the match in progress first."
3. `verdict = evaluateTrade(world, team, other, offer)`; if not accepted → return it.
4. `tradeMade(...)`; if a player in the XI has left → `autoPick()`; return `{ ...verdict, done: true }`.

**`signCover(injuredId, id)`** → `replacementSigned({ team, injuredId, id })`; return whether he joined.

---

## 6. The listener: `afterMatchMutation()`

```cpp
void Game::afterMatchMutation() {
  if (!s_.match || s_.match->status != MatchStatus::complete) return;
  if (lastRecordedMatchId_ == s_.match->id) return;   // fire once (JS: previous status != complete)
  lastRecordedMatchId_ = s_.match->id;
  Match& m = *s_.match;
  matchCompleted(m);                                    // career
  matchFitness(engine::matchWorkload(m));               // world
  if (m.fixture && s_.season.season && s_.season.season->id == m.fixture->seasonId) {
    auto summary = engine::summarizeMatch(m);
    fixtureCompleted(m.fixture->no, summary);
    takeGate(m.fixture->no);
    const TeamId me = m.teams[m.userTeam.value_or(0)].id;
    // JS guards with `summary.winner !== undefined`, which is always true (winner is a team id or null)
    boardMatchResult(/*won*/ summary.winner == me, /*tied*/ !summary.winner.has_value());
  }
}
```

`career.recordMatch` also refuses a match id it has already recorded, so a double call is harmless.

---

## 7. Derived selectors used by screens

| JS | C++ (`state/derived`) |
|----|------------------------|
| `useLeague()` = `effectiveLeague(world, career.progress)` | `derived.league()` |
| `useTeam(id)` | `derived.team(id)` |
| `usePlayerLookup()` / `usePlayer(id)` = every player, developed | `derived.player(id)` |
| `useTeamOf()` = id → team meta (current, else retired team, else id prefix) | `derived.teamOf(id)` |
| `useTeamIdOf()` = id → team id (current, retired, drafted, prefix) | `derived.teamIdOf(id)` |
| `teamById(id)` (static) | `engine::teamMeta(id)` |
| `pointsTable(season)` etc. | call the engine function; cache by `rev.season` |
