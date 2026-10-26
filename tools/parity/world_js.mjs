// Golden dump of N off-seasons with the AI making every decision (world.test.js fullOffseason),
// from the JS engine. Covers the mega-auction years: retention, Right to Match. The C++ twin is
// runOffseasons in tools/parity/flow.cpp. Nothing in t20-coach/ is changed.
//
//   node tools/parity/world_js.mjs [YEARS] > tests/golden/offseasons.json
import { LEAGUE } from '../../../t20-coach/src/data.js';
import { createWorld, runOffseason, closeContracts, closeRetention, finishDraft, closeAuction } from '../../../t20-coach/src/engine/world.js';
import { runAuction } from '../../../t20-coach/src/engine/auction.js';

const years = Number(process.argv[2] ?? 4);
const order = LEAGUE.map((t) => t.id);
const w = createWorld(LEAGUE);
const results = [];
for (let y = 0; y < years; y++) {
  runOffseason(w, { order, userTeam: null });
  if (w.offseason.stage === 'retention') closeRetention(w);
  else closeContracts(w);
  finishDraft(w);
  runAuction(w);
  results.push(closeAuction(w));
}
process.stdout.write(JSON.stringify({ years, results, world: w }));
