// Golden dumps from the JS engine (the reference). The C++ twin is tools/parity/parity.cpp; both
// must print byte-identical JSON. Nothing in t20-coach/ is changed: this only imports it.
//
//   node tools/parity/dump_js.mjs league            > tests/golden/league.json
//   node tools/parity/dump_js.mjs matches 1 400     > tests/golden/matches.json
//   node tools/parity/dump_js.mjs userflow 1 40     > tests/golden/userflow.json
//   node tools/parity/dump_js.mjs balllog 17        (every ball of one match, to find a divergence)
import { generateLeague, pickBestXI, benchOf } from '../../../t20-coach/src/engine/league.js';
import {
  createMatch, simMatch, simInnings, autoToss, playBall, playOver, callToss, chooseTossDecision, startSecondInnings,
  reviewDecision, setBowler, setNextBatter, setBowlingPlan, setField, setBatterTempo, setBatterTarget, canChangeBowler,
  changeBowler, impactOptions, useImpactPlayer, resolvePending,
} from '../../../t20-coach/src/engine/match.js';
import { bowlingOptions } from '../../../t20-coach/src/engine/rules.js';
import { plansFor } from '../../../t20-coach/src/engine/tactics.js';
import { recordMatchups } from '../../../t20-coach/src/engine/matchups.js';
import { winChance } from '../../../t20-coach/src/engine/winprob.js';
import { PITCH_KEYS } from '../../../t20-coach/src/engine/pitch.js';

const league = generateLeague();

function side(team, pitch, withBench = true) {
  const { ids, keeperId } = pickBestXI(team, pitch);
  const by = Object.fromEntries(team.squad.map((p) => [p.id, p]));
  return {
    id: team.id, name: team.name, color: team.color, captain: team.captain, keeperId,
    players: ids.map((i) => by[i]), bench: withBench ? benchOf(team, ids).map((i) => by[i]) : [],
  };
}

// FNV-1a over the UTF-8 bytes of a string
function fnv(str) {
  let h = 0x811c9dc5 | 0;
  for (const b of new TextEncoder().encode(str)) {
    h ^= b;
    h = Math.imul(h, 0x01000193);
  }
  return h >>> 0;
}

function summary(seed, m, extra = {}) {
  const comm = [];
  const balls = [];
  for (const inn of m.innings) {
    for (const c of inn.commentary) comm.push(`${c.id}|${c.over}|${c.kind}|${c.text}`);
    for (const o of inn.overs) {
      for (const b of o.balls) balls.push(`${o.bowler}:${b.label},${b.kind},${b.runs ?? ''},${b.dir ?? ''},${b.dist ?? ''},${b.pos ?? ''},${b.saved ?? ''}`);
    }
  }
  return {
    seed,
    pitch: m.pitch,
    teams: m.teams.map((t) => t.id),
    toss: m.toss?.winner ?? null,
    dew: m.dew,
    sky: m.weather.sky,
    rain: m.weather.rain?.when ?? null,
    status: m.status,
    winner: m.result?.winner ?? null,
    text: m.result?.text ?? null,
    potm: m.potm ?? null,
    innings: m.innings.map((i) => [i.runs, i.wickets, i.legalBalls, i.extras.wd + i.extras.nb + i.extras.lb + i.extras.b, i.reviews.length, i.drops.length]),
    impact: m.teams.map((t) => (t.impact ? `${t.impact.out}>${t.impact.in}` : null)),
    rng: m.rngState,
    seq: m.commentarySeq,
    commentary: fnv(comm.join('\n')),
    balls: fnv(balls.join('\n')),
    ...extra,
  };
}

function matches(from, to) {
  const book = {};
  const out = [];
  for (let seed = from; seed <= to; seed++) {
    const pitch = PITCH_KEYS[seed % PITCH_KEYS.length];
    const a = league[seed % 10];
    const b = league[(seed + 3) % 10];
    const m = createMatch({
      seed, pitch, userTeam: null, teams: [side(a, pitch), side(b, pitch)], matchups: seed > (from + to) / 2 ? book : null,
    });
    autoToss(m);
    simInnings(m);
    const extra = {};
    if (seed % 25 === 0 && m.status === 'break') extra.wp = winChance(m, 30);
    simMatch(m);
    recordMatchups(book, m);
    out.push(summary(seed, m, extra));
  }
  return out;
}

const PRESET_CYCLE = ['attacking', 'balanced', 'defensive', 'death', 'spin', 'offside', 'auto'];
const TARGET_CYCLE = ['attack', 'block', null];

function userflow(from, to) {
  const out = [];
  for (let seed = from; seed <= to; seed++) {
    const pitch = PITCH_KEYS[seed % PITCH_KEYS.length];
    const m = createMatch({
      seed, pitch, userTeam: 0,
      teams: [side(league[seed % 10], pitch), side(league[(seed + 5) % 10], pitch, seed % 2 === 0)],
      settings: { autoReview: seed % 4 === 0 },
    });
    callToss(m, seed % 3 ? 'heads' : 'tails');
    if (!m.toss.decision) chooseTossDecision(m, seed % 2 ? 'bat' : 'bowl');
    let guard = 0;
    let step = 0;
    const log = [];
    while (m.status !== 'complete' && guard++ < 4000) {
      step++;
      if (m.status === 'break') {
        if (seed % 3 === 0) {
          const opts = impactOptions(m, 0);
          if (opts.allowed) {
            const inn = opts.ins.find((x) => x.ok);
            const outId = inn && opts.outs.find((o) => !m.players[inn.id].overseas || m.players[o].overseas);
            if (inn && outId) log.push(`impact:${useImpactPlayer(m, 0, outId, inn.id)}`);
          }
        }
        startSecondInnings(m);
        continue;
      }
      if (m.awaiting === 'review') {
        log.push(`review:${reviewDecision(m, (seed + step) % 3 === 0)}`);
      } else if (m.awaiting === 'bowler') {
        const ok = bowlingOptions(m).filter((o) => o.ok);
        const pick = ok[(seed + step) % ok.length];
        log.push(`bowler:${pick.id}:${setBowler(m, pick.id)}`);
        const bad = bowlingOptions(m).find((o) => !o.ok);
        if (bad) log.push(`bad:${setBowler(m, bad.id)}`);
        if (canChangeBowler(m) && step % 9 === 0) {
          log.push(`change:${changeBowler(m)}`);
          continue;
        }
        const bowler = m.players[m.innings[m.current].currentBowler];
        if (bowler) {
          const plans = plansFor(bowler);
          const plan = step % 5 === 0 ? 'auto' : plans[step % plans.length].key;
          setBowlingPlan(m, plan);
          setField(m, { preset: PRESET_CYCLE[step % PRESET_CYCLE.length] });
        }
      } else if (m.awaiting === 'batter') {
        const q = m.innings[m.current].yetToBat;
        const pick = q[(seed + step) % q.length];
        log.push(`batter:${pick}:${setNextBatter(m, pick)}`);
      } else {
        const inn = m.innings[m.current];
        if (m.status === 'live' && inn.batting === 0 && inn.striker) {
          if (step % 7 === 0) setBatterTempo(m, inn.striker, step % 11 === 0 ? 'auto' : 1 + (step % 5));
          if (step % 13 === 0 && inn.currentBowler) setBatterTarget(m, inn.currentBowler, TARGET_CYCLE[step % 3]);
        }
        if (seed % 2) playBall(m);
        else playOver(m);
      }
    }
    out.push(summary(seed, m, { steps: step, log: fnv(log.join('\n')) }));
  }
  return out;
}

function balllog(seed) {
  const pitch = PITCH_KEYS[seed % PITCH_KEYS.length];
  const m = createMatch({ seed, pitch, userTeam: null, teams: [side(league[seed % 10], pitch), side(league[(seed + 3) % 10], pitch)] });
  autoToss(m);
  const out = [];
  while (m.status !== 'complete') {
    if (m.status === 'break') startSecondInnings(m);
    resolvePending(m);
    const res = playBall(m);
    const inn = m.innings[m.current];
    out.push([m.current, inn.legalBalls, inn.runs, inn.wickets, res, m.rngState]);
  }
  return out;
}

const [cmd, a1, a2] = process.argv.slice(2);
if (cmd === 'league') process.stdout.write(JSON.stringify(generateLeague()));
else if (cmd === 'matches') process.stdout.write(JSON.stringify(matches(Number(a1 ?? 1), Number(a2 ?? 400))));
else if (cmd === 'userflow') process.stdout.write(JSON.stringify(userflow(Number(a1 ?? 1), Number(a2 ?? 40))));
else if (cmd === 'balllog') process.stdout.write(JSON.stringify(balllog(Number(a1 ?? 1))));
else {
  console.error('usage: node tools/parity/dump_js.mjs league | matches FROM TO | userflow FROM TO | balllog SEED');
  process.exit(1);
}
