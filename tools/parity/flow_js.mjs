// Golden dump of a whole career step from the JS game (the reference): the path of the JS
// flow test, driven through the real Redux store. Nothing in t20-coach/ is changed: this only
// imports it. The C++ twin is tools/parity/flow.cpp.
//
//   node tools/parity/flow_js.mjs [TEAM] [RANDOM_SEED] > tests/golden/flow.json
//
// Math.random and Date.now only make season seeds; they are stubbed so the run is repeatable.
// Checkpoints (full JSON state, as the browser would save it):
//   start       season 1 created
//   seasonDone  season 1 played to a champion (world, career, season)
//   offseason   board review done, off-season started (world, setup screen)
//   season2     contracts, draft, auction (handed to the AI) and season 2 started (world, career, season)
const [team = 'RJR', rseed = '7'] = process.argv.slice(2);

// Only the game's own draws advance the sequence: Redux also calls Math.random (action type
// strings, listener ids), and those calls get a constant so the C++ Game can replay the run.
let r = Number(rseed) >>> 0 || 7;
Math.random = () => {
  const caller = new Error().stack.split('\n')[2] ?? '';
  if (caller.includes('node_modules')) return 0.5;
  r = (Math.imul(r, 1103515245) + 12345) >>> 0;
  return r / 4294967296;
};
Date.now = () => 1700000000000;

const { makeStore } = await import('../../../t20-coach/src/store/store.js');
const { startSeason, simFixtures, nextSeason, completeDraft, finishOffseason } = await import('../../../t20-coach/src/store/thunks.js');
const { contractsClosed, auctionAutoRest } = await import('../../../t20-coach/src/store/worldSlice.js');

const store = makeStore();
const S = () => store.getState();
const seasonOf = () => ({ season: S().season.season, history: S().season.history });
const setupOf = () => {
  const s = S().setup;
  return { screen: s.screen, userTeamId: s.userTeamId, oppTeamId: s.oppTeamId, xi: s.xi, keeperId: s.keeperId, captainId: s.captainId };
};

const out = { team, rseed: Number(rseed), seeds: [], checkpoints: {} };

store.dispatch(startSeason(team));
out.seeds.push(S().season.season.seed);
out.checkpoints.start = { season: seasonOf(), setup: setupOf() };

for (let i = 0; i < 10 && !S().season.season.champion; i++) await store.dispatch(simFixtures({ includeUser: true }));
out.checkpoints.seasonDone = { world: S().world, career: S().career, season: seasonOf(), setup: setupOf() };

store.dispatch(nextSeason());
out.checkpoints.offseason = { world: S().world, season: seasonOf(), setup: setupOf() };

if (S().setup.screen === 'offseason') {
  store.dispatch(contractsClosed());
  store.dispatch(completeDraft());
  store.dispatch(auctionAutoRest());
  store.dispatch(finishOffseason());
  out.seeds.push(S().season.season?.seed ?? null);
  out.checkpoints.season2 = { world: S().world, career: S().career, season: seasonOf(), setup: setupOf() };
}

process.stdout.write(JSON.stringify(out));
