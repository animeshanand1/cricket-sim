// A copy of t20-coach/scripts/balance.mjs for parity checks with tools/balance.cpp.
// Differences from the original: `--seed-base S` makes the match draw (teams, pitch, seed)
// deterministic, so the JS and C++ runs play the same matches; and the last line is printed in a
// fixed "Dismissals: k v%, ..." format instead of Node's object inspection.
//
//   node tools/parity/balance_js.mjs 3000 [pitch] [--seed-base 7]
import { generateLeague, pickBestXI, benchOf } from '../../../t20-coach/src/engine/league.js';
import { createMatch, simMatch } from '../../../t20-coach/src/engine/match.js';
import { PITCH_KEYS } from '../../../t20-coach/src/engine/pitch.js';
import { createRng } from '../../../t20-coach/src/engine/rng.js';

const args = process.argv.slice(2);
const sbAt = args.indexOf('--seed-base');
const seedBase = sbAt >= 0 ? Number(args[sbAt + 1]) : null;
const pos = args.filter((_, i) => sbAt < 0 || (i !== sbAt && i !== sbAt + 1));
const N = Number(pos[0] || 2000);
const onlyPitch = pos[1];
const rand = seedBase == null ? Math.random : createRng(seedBase);
const league = generateLeague();

const buildSide = (team, pitch) => {
  const { ids, keeperId } = pickBestXI(team, pitch);
  const byId = Object.fromEntries(team.squad.map((p) => [p.id, p]));
  return { id: team.id, name: team.name, color: team.color, captain: team.captain, keeperId, players: ids.map((id) => byId[id]), bench: benchOf(team, ids).map((id) => byId[id]) };
};

const s = { first: [], second: [], wk1: [], allOut: 0, fours: 0, sixes: 0, extras: 0, dots: 0, legal: 0, chaseWins: 0, ties: 0, inns: 0 };
const bat = { top: { r: 0, b: 0, outs: 0 }, tail: { r: 0, b: 0, outs: 0 } };
const phaseRuns = { pp: 0, mid: 0, death: 0 };
const phaseOvers = { pp: 0, mid: 0, death: 0 };
const wkTypes = {};
let fifties = 0, hundreds = 0;
const bowlEcon = [];
const hauls = { 3: 0, 4: 0, 5: 0 };
const ex = { b: 0, saves: 0, overthrows: 0, reviews: 0, overturned: 0 };

for (let i = 0; i < N; i++) {
  const a = Math.floor(rand() * league.length);
  let b = Math.floor(rand() * (league.length - 1));
  if (b >= a) b++;
  const pitch = onlyPitch || PITCH_KEYS[Math.floor(rand() * PITCH_KEYS.length)];
  const m = createMatch({ seed: (rand() * 2 ** 31) | 0, teams: [buildSide(league[a], pitch), buildSide(league[b], pitch)], userTeam: null, pitch });
  simMatch(m);
  const [i1, i2] = m.innings;
  s.first.push(i1.runs);
  if (!m.weather?.rain) (s.dry ??= []).push(i1.runs); else s.rainy = (s.rainy ?? 0) + 1;
  s.wk1.push(i1.wickets);
  if (i1.wickets === 10) s.allOut++;
  if (m.result.winner === i2.batting) s.chaseWins++;
  if (m.result.superOver) s.ties++;
  const impacts = m.teams.filter((t) => t.impact).length; s.impacts = (s.impacts ?? 0) + impacts;
  for (const inn of [i1, i2]) {
    s.inns++;
    s.extras += inn.extras.wd + inn.extras.nb + inn.extras.lb + inn.extras.b;
    s.legal += inn.legalBalls;
    ex.b += inn.extras.b ?? 0;
    for (const o of inn.overs) for (const b of o.balls) { if (b.saved) ex.saves++; if (b.overthrow) ex.overthrows++; }
    for (const r of inn.reviews ?? []) { ex.reviews++; if (r.overturned) ex.overturned++; }
    for (const [idx, id] of inn.battedOrder.entries()) {
      const st = inn.batters[id];
      const g = idx < 4 ? bat.top : idx >= 8 ? bat.tail : null;
      if (st.runs >= 100) hundreds++; else if (st.runs >= 50) fifties++;
      if (g) { g.r += st.runs; g.b += st.balls; g.outs += st.out ? 1 : 0; }
      s.fours += st.fours; s.sixes += st.sixes;
    }
    for (const id in inn.bowling) {
      const f = inn.bowling[id];
      s.dots += f.dots;
      if (f.balls >= 24) bowlEcon.push((f.runs * 6) / f.balls);
      for (const k of [3, 4, 5]) if (f.wickets >= k) hauls[k]++;
    }
    inn.overs.forEach((o, k) => {
      const ph = k < 6 ? 'pp' : k < 15 ? 'mid' : 'death';
      phaseRuns[ph] += o.runs; phaseOvers[ph]++;
    });
    for (const c of inn.commentary) {
      if (c.kind !== 'wicket') continue;
      const t = c.text.match(/ (b |lbw|c & b|c †|st †|run out|c )/);
      const k = t ? t[1].trim() : '?';
      wkTypes[k] = (wkTypes[k] || 0) + 1;
    }
  }
}

const avg = (a) => a.reduce((x, y) => x + y, 0) / a.length;
const sorted = [...s.first].sort((x, y) => x - y);
const pct = (p) => sorted[Math.floor(p * (sorted.length - 1))];
const f1 = (x) => x.toFixed(1);
console.log(`Matches: ${N}${onlyPitch ? ` on ${onlyPitch}` : ''}`);
console.log(`1st inns avg ${f1(avg(s.first))} (dry games ${f1(avg(s.dry))}, rain affected ${s.rainy ?? 0}) | p10 ${pct(0.1)} p50 ${pct(0.5)} p90 ${pct(0.9)} | min ${sorted[0]} max ${sorted.at(-1)}`);
console.log(`1st inns wickets avg ${f1(avg(s.wk1))} | all out ${f1((100 * s.allOut) / N)}% | 200+ ${f1((100 * s.first.filter((r) => r >= 200).length) / N)}%`);
console.log(`Per innings: 4s ${f1(s.fours / s.inns)} 6s ${f1(s.sixes / s.inns)} extras ${f1(s.extras / s.inns)} dot% ${f1((100 * s.dots) / s.legal)}`);
console.log(`RPO by phase: PP ${f1(phaseRuns.pp / phaseOvers.pp)} mid ${f1(phaseRuns.mid / phaseOvers.mid)} death ${f1(phaseRuns.death / phaseOvers.death)}`);
console.log(`Chasing side wins ${f1((100 * s.chaseWins) / N)}% | super overs ${s.ties}`);
for (const [k, g] of Object.entries(bat)) console.log(`${k.padEnd(4)} avg ${f1(g.r / Math.max(1, g.outs))} SR ${f1((100 * g.r) / g.b)}`);
console.log(`50s per match ${f1(fifties / N)} | 100s per match ${(hundreds / N).toFixed(3)}`);
console.log(`Impact Player used in ${f1((100 * s.impacts) / (2 * N))}% of team-matches`);
console.log(`Bowler econ (4 overs) avg ${f1(avg(bowlEcon))} | per match: 3+ wkt hauls ${(hauls[3] / N).toFixed(2)}, 4+ ${(hauls[4] / N).toFixed(3)}, 5+ ${(hauls[5] / N).toFixed(3)}`);
console.log(`Per innings: byes ${(ex.b / s.inns).toFixed(2)} | boundaries saved ${(ex.saves / s.inns).toFixed(2)} | overthrows ${(ex.overthrows / s.inns).toFixed(2)} | reviews ${(ex.reviews / s.inns).toFixed(2)} (${ex.reviews ? f1((100 * ex.overturned) / ex.reviews) : 0}% overturned)`);
const totalWk = Object.values(wkTypes).reduce((x, y) => x + y, 0);
console.log(`Dismissals: ${Object.entries(wkTypes).map(([k, v]) => `${k} ${f1((100 * v) / totalWk)}%`).join(', ')}`);
