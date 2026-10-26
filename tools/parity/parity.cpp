#include "parity.hpp"

#include <functional>
#include <unordered_map>

#include "core/json_writer.hpp"
#include "core/str.hpp"
#include "engine/league.hpp"
#include "engine/match.hpp"
#include "engine/matchups.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "engine/skills.hpp"
#include "engine/tactics.hpp"
#include "engine/winprob.hpp"

namespace t20::parity {

namespace {
const std::vector<Team>& LEAGUE() {
  static const std::vector<Team> league = generateLeague();
  return league;
}

template <class K>
void writeSkills(JsonWriter& j, const std::vector<SkillDef<K>>& defs, const SkillSet<K>& s) {
  j.beginObject();
  for (const auto& d : defs) {
    if (!s.contains(d.key)) continue;
    j.key(name(d.key)).value(s[d.key]);
  }
  j.endObject();
}

void writePlayer(JsonWriter& j, const Player& p) {
  j.beginObject();
  j.key("id").value(p.id);
  j.key("name").value(p.name);
  j.key("role").value(name(p.role));
  j.key("hand").value(name(p.hand));
  j.key("age").value(p.age);
  j.key("bat").value(p.bat);
  j.key("power").value(p.power);
  j.key("bowl").value(p.bowl);
  j.key("bowlType");
  if (p.bowlType)
    j.value(name(*p.bowlType));
  else
    j.null();
  j.key("field").value(p.field);
  j.key("awareness").value(p.awareness);
  j.key("skills").beginObject();
  j.key("bat");
  writeSkills(j, BAT_SKILLS, p.skills.bat);
  j.key("bowl");
  writeSkills(j, isPaceType(p.bowlType) ? PACE_SKILLS : SPIN_SKILLS, p.skills.bowl);
  j.key("field");
  writeSkills(j, FIELD_SKILLS, p.skills.field);
  j.endObject();
  j.key("batStyle").value(p.batStyle);
  j.key("bowlStyle");
  if (p.bowlStyle)
    j.value(*p.bowlStyle);
  else
    j.null();
  j.key("overseas").value(p.overseas);
  j.endObject();
}

template <class T>
void optInt(JsonWriter& j, const std::optional<T>& v) {
  if (v)
    j.value(static_cast<double>(*v));
  else
    j.null();
}

std::string ballLine(const Over& o, const Ball& b) {
  const bool isWide = b.kind == BallKind::extra && !b.bat;
  const bool isNoBall = b.kind == BallKind::extra && b.bat;
  const bool isRun = b.kind == BallKind::run || b.kind == BallKind::b4 || b.kind == BallKind::b6;
  std::string line = cat(o.bowler, ":", b.label, ",", name(b.kind), ",");
  if (!isWide) line += std::to_string(b.runs);
  line += ",";
  if (b.dir) line += js_num(*b.dir);
  line += ",";
  if (b.dist) line += js_num(*b.dist);
  line += ",";
  if (!isNoBall && b.pos) line += std::string(name(*b.pos));
  line += ",";
  if (isRun) line += std::to_string(b.saved);
  return line;
}

void writeSummary(JsonWriter& j, int seed, const Match& m, const std::function<void(JsonWriter&)>& extra) {
  std::string comm, balls;
  bool firstC = true, firstB = true;
  for (const Innings& inn : m.innings) {
    for (const auto& c : inn.commentary) {
      if (!firstC) comm += '\n';
      firstC = false;
      comm += cat(c.id, "|", c.over, "|", name(c.kind), "|", c.text);
    }
    for (const Over& o : inn.overs)
      for (const Ball& b : o.balls) {
        if (!firstB) balls += '\n';
        firstB = false;
        balls += ballLine(o, b);
      }
  }
  j.beginObject();
  j.key("seed").value(seed);
  j.key("pitch").value(name(m.pitch));
  j.key("teams").beginArray().value(m.teams[0].id).value(m.teams[1].id).endArray();
  j.key("toss");
  if (m.toss)
    j.value(m.toss->winner);
  else
    j.null();
  j.key("dew").value(m.dew);
  j.key("sky").value(name(m.weather.sky));
  j.key("rain");
  if (m.weather.rain)
    j.value(name(m.weather.rain->when));
  else
    j.null();
  j.key("status").value(name(m.status));
  j.key("winner");
  if (m.result && m.result->winner)
    j.value(*m.result->winner);
  else
    j.null();
  j.key("text");
  if (m.result)
    j.value(m.result->text);
  else
    j.null();
  j.key("potm");
  if (m.potm)
    j.value(*m.potm);
  else
    j.null();
  j.key("innings").beginArray();
  for (const Innings& i : m.innings) {
    j.beginArray()
        .value(i.runs)
        .value(i.wickets)
        .value(i.legalBalls)
        .value(i.extras.wd + i.extras.nb + i.extras.lb + i.extras.b)
        .value(static_cast<int>(i.reviews.size()))
        .value(static_cast<int>(i.drops.size()))
        .endArray();
  }
  j.endArray();
  j.key("impact").beginArray();
  for (const auto& t : m.teams) {
    if (t.impact)
      j.value(cat(t.impact->out, ">", t.impact->in));
    else
      j.null();
  }
  j.endArray();
  j.key("rng").value(static_cast<double>(m.rngState));
  j.key("seq").value(m.commentarySeq);
  j.key("commentary").value(static_cast<double>(fnv(comm)));
  j.key("balls").value(static_cast<double>(fnv(balls)));
  if (extra) extra(j);
  j.endObject();
}

std::string bool_s(bool b) { return b ? "true" : "false"; }
}  // namespace

uint32_t fnv(const std::string& s) {
  uint32_t h = 0x811c9dc5u;
  for (unsigned char b : s) {
    h ^= b;
    h *= 0x01000193u;
  }
  return h;
}

SideInput side(const Team& team, Pitch pitch, bool withBench) {
  const BestXI xi = pickBestXI(team, pitch);
  std::unordered_map<PlayerId, const Player*> by;
  for (const auto& p : team.squad) by[p.id] = &p;
  SideInput s;
  s.id = team.id;
  s.name = team.name;
  s.color = team.color;
  s.captain = team.captain;
  s.keeperId = xi.keeperId;
  for (const auto& id : xi.ids) s.players.push_back(*by.at(id));
  if (withBench)
    for (const auto& id : benchOf(team, xi.ids)) s.bench.push_back(*by.at(id));
  return s;
}

std::string dumpLeague() {
  JsonWriter j;
  j.beginArray();
  for (const Team& t : LEAGUE()) {
    j.beginObject();
    j.key("id").value(t.id);
    j.key("captain").value(name(t.captain));
    j.key("personality").value(t.personality);
    j.key("name").value(t.name);
    j.key("city").value(t.city);
    j.key("color").value(t.color);
    j.key("ground").value(t.ground);
    j.key("pitch").value(name(t.pitch));
    j.key("squad").beginArray();
    for (const Player& p : t.squad) writePlayer(j, p);
    j.endArray();
    j.endObject();
  }
  j.endArray();
  return j.str();
}

std::string dumpMatches(int from, int to) {
  const auto& league = LEAGUE();
  MatchupBook book;
  JsonWriter j;
  j.beginArray();
  for (int seed = from; seed <= to; ++seed) {
    const Pitch pitch = PITCH_KEYS[static_cast<size_t>(seed) % PITCH_KEYS.size()];
    MatchSetup setup;
    setup.seed = seed;
    setup.pitch = pitch;
    setup.userTeam.reset();
    setup.teams = {side(league[static_cast<size_t>(seed % 10)], pitch), side(league[static_cast<size_t>((seed + 3) % 10)], pitch)};
    if (seed > (from + to) / 2.0) setup.matchups = &book;
    Match m = createMatch(setup);
    autoToss(m);
    simInnings(m);
    std::optional<std::array<double, 2>> wp;
    if (seed % 25 == 0 && m.status == MatchStatus::break_) wp = winChance(m, 30);
    simMatch(m);
    recordMatchups(book, m);
    writeSummary(j, seed, m, [&](JsonWriter& w) {
      if (wp) w.key("wp").beginArray().value((*wp)[0]).value((*wp)[1]).endArray();
    });
  }
  j.endArray();
  return j.str();
}

std::string dumpUserflow(int from, int to) {
  static const FieldPreset PRESET_CYCLE[] = {FieldPreset::attacking, FieldPreset::balanced, FieldPreset::defensive,
                                             FieldPreset::death,     FieldPreset::spin,     FieldPreset::offside,
                                             FieldPreset::auto_};
  static const std::optional<TargetMode> TARGET_CYCLE[] = {TargetMode::attack, TargetMode::block, std::nullopt};
  const auto& league = LEAGUE();
  JsonWriter j;
  j.beginArray();
  for (int seed = from; seed <= to; ++seed) {
    const Pitch pitch = PITCH_KEYS[static_cast<size_t>(seed) % PITCH_KEYS.size()];
    MatchSetup setup;
    setup.seed = seed;
    setup.pitch = pitch;
    setup.userTeam = 0;
    setup.teams = {side(league[static_cast<size_t>(seed % 10)], pitch),
                   side(league[static_cast<size_t>((seed + 5) % 10)], pitch, seed % 2 == 0)};
    setup.settings.autoReview = seed % 4 == 0;
    Match m = createMatch(setup);
    callToss(m, seed % 3 ? "heads" : "tails");
    if (!m.toss->decision) chooseTossDecision(m, seed % 2 ? TossDecision::bat : TossDecision::bowl);
    int guard = 0;
    int step = 0;
    std::vector<std::string> log;
    while (m.status != MatchStatus::complete && guard++ < 4000) {
      ++step;
      if (m.status == MatchStatus::break_) {
        if (seed % 3 == 0) {
          const ImpactOptions opts = impactOptions(m, 0);
          if (opts.allowed) {
            const ImpactIn* in = nullptr;
            for (const auto& x : opts.ins)
              if (x.ok) {
                in = &x;
                break;
              }
            const PlayerId* outId = nullptr;
            if (in)
              for (const auto& o : opts.outs)
                if (!m.players.at(in->id).overseas || m.players.at(o).overseas) {
                  outId = &o;
                  break;
                }
            if (in && outId) log.push_back(cat("impact:", bool_s(useImpactPlayer(m, 0, *outId, in->id))));
          }
        }
        startSecondInnings(m);
        continue;
      }
      if (m.awaiting == Awaiting::review) {
        log.push_back(cat("review:", bool_s(reviewDecision(m, (seed + step) % 3 == 0))));
      } else if (m.awaiting == Awaiting::bowler) {
        std::vector<BowlOption> ok;
        for (const auto& o : bowlingOptions(m))
          if (o.ok) ok.push_back(o);
        const PlayerId pick = ok[static_cast<size_t>((seed + step) % static_cast<int>(ok.size()))].id;
        log.push_back(cat("bowler:", pick, ":", bool_s(setBowler(m, pick))));
        for (const auto& o : bowlingOptions(m))
          if (!o.ok) {
            log.push_back(cat("bad:", bool_s(setBowler(m, o.id))));
            break;
          }
        if (canChangeBowler(m) && step % 9 == 0) {
          log.push_back(cat("change:", bool_s(changeBowler(m))));
          continue;
        }
        const Innings& inn = m.innings[static_cast<size_t>(m.current)];
        if (inn.currentBowler) {
          const Player& bowler = m.players.at(*inn.currentBowler);
          const auto& plans = plansFor(bowler);
          const std::optional<Plan> plan =
              step % 5 == 0 ? std::nullopt : std::optional<Plan>(plans[static_cast<size_t>(step) % plans.size()].key);
          setBowlingPlan(m, plan);
          setField(m, PRESET_CYCLE[step % 7]);
        }
      } else if (m.awaiting == Awaiting::batter) {
        const auto& q = m.innings[static_cast<size_t>(m.current)].yetToBat;
        const PlayerId pick = q[static_cast<size_t>((seed + step) % static_cast<int>(q.size()))];
        log.push_back(cat("batter:", pick, ":", bool_s(setNextBatter(m, pick))));
      } else {
        const Innings& inn = m.innings[static_cast<size_t>(m.current)];
        if (m.status == MatchStatus::live && inn.batting == 0 && inn.striker) {
          if (step % 7 == 0)
            setBatterTempo(m, *inn.striker, step % 11 == 0 ? Tempo{} : Tempo{1.0 + step % 5});
          if (step % 13 == 0 && inn.currentBowler) setBatterTarget(m, *inn.currentBowler, TARGET_CYCLE[step % 3]);
        }
        if (seed % 2)
          playBall(m);
        else
          playOver(m);
      }
    }
    std::string joined;
    for (size_t i = 0; i < log.size(); ++i) joined += (i ? "\n" : "") + log[i];
    writeSummary(j, seed, m, [&](JsonWriter& w) {
      w.key("steps").value(step);
      w.key("log").value(static_cast<double>(fnv(joined)));
    });
  }
  j.endArray();
  return j.str();
}

std::string dumpBallLog(int seed) {
  const auto& league = LEAGUE();
  const Pitch pitch = PITCH_KEYS[static_cast<size_t>(seed) % PITCH_KEYS.size()];
  MatchSetup setup;
  setup.seed = seed;
  setup.pitch = pitch;
  setup.userTeam.reset();
  setup.teams = {side(league[static_cast<size_t>(seed % 10)], pitch), side(league[static_cast<size_t>((seed + 3) % 10)], pitch)};
  Match m = createMatch(setup);
  autoToss(m);
  JsonWriter j;
  j.beginArray();
  while (m.status != MatchStatus::complete) {
    if (m.status == MatchStatus::break_) startSecondInnings(m);
    resolvePending(m);
    const std::string res = playBall(m);
    const Innings& inn = m.innings[static_cast<size_t>(m.current)];
    j.beginArray().value(m.current).value(inn.legalBalls).value(inn.runs).value(inn.wickets);
    if (res.empty())
      j.null();
    else
      j.value(res);
    j.value(static_cast<double>(m.rngState)).endArray();
  }
  j.endArray();
  return j.str();
}

}  // namespace t20::parity
