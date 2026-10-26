#include "engine/achievements.hpp"

#include <algorithm>
#include <functional>
#include <numeric>

#include "core/str.hpp"

namespace t20 {

namespace {

struct Ctx {
  const ManagerRecord& manager;
  const std::vector<SeasonArchive>& history;
  const std::vector<BoardEntry>& board;
};

std::vector<int> titles(const std::vector<SeasonArchive>& h) {
  std::vector<int> out;
  for (const auto& s : h)
    if (s.champion == s.userTeam) out.push_back(s.number);
  return out;
}
std::optional<int> backToBack(const std::vector<SeasonArchive>& h) {
  const auto won = titles(h);
  for (int n : won)
    if (std::find(won.begin(), won.end(), n - 1) != won.end()) return n;
  return std::nullopt;
}
std::optional<int> afterSacking(const Ctx& c) {
  std::vector<int> sacked;
  for (const auto& b : c.board)
    if (b.event == "sacked") sacked.push_back(b.season.value_or(0));
  if (sacked.empty()) return std::nullopt;
  const int first = *std::min_element(sacked.begin(), sacked.end());
  for (int n : titles(c.history))
    if (n > first) return n;
  return std::nullopt;
}

struct Def {
  std::string_view key, group, title, text;
  int goal;  // 0 = a one-game feat
  std::function<int(const Ctx&)> count;
  std::function<std::optional<int>(const Ctx&)> when;
};

const std::vector<Def>& ACHIEVEMENTS() {
  static const std::vector<Def> defs = {
      {"first_win", "match", "Off the mark", "Win your first match.", 1, [](const Ctx& c) { return c.manager.wins; }, {}},
      {"wins_10", "match", "Finding your feet", "Win 10 matches.", 10, [](const Ctx& c) { return c.manager.wins; }, {}},
      {"wins_50", "match", "Winning habit", "Win 50 matches.", 50, [](const Ctx& c) { return c.manager.wins; }, {}},
      {"wins_100", "match", "Century of wins", "Win 100 matches.", 100, [](const Ctx& c) { return c.manager.wins; }, {}},
      {"streak_5", "match", "On a roll", "Win five matches in a row.", 5,
       [](const Ctx& c) { return c.manager.bestStreak; }, {}},
      {"streak_10", "match", "Unstoppable", "Win ten matches in a row.", 10,
       [](const Ctx& c) { return c.manager.bestStreak; }, {}},
      {"heart_stopper", "match", "Heart-stopper",
       "Win by three runs or fewer, or with no more than one ball to spare.", 0, {}, {}},
      {"super_over", "match", "Ice in the veins", "Win a Super Over.", 0, {}, {}},
      {"chase_200", "match", "Mission impossible", "Chase down a target of 200 or more.", 0, {}, {}},
      {"defend_low", "match", "Daylight robbery", "Defend a total under 140 in a full 20-over game.", 0, {}, {}},
      {"thrashing", "match", "No contest", "Win by 80 runs or more, or by ten wickets.", 0, {}, {}},
      {"total_220", "match", "Run riot", "Score 220 or more.", 0, {}, {}},
      {"skittled", "match", "Skittled", "Bowl the opposition out for under 100.", 0, {}, {}},
      {"sixes_15", "match", "Clearing the ropes", "Hit 15 sixes in an innings.", 0, {}, {}},
      {"ton", "match", "Raise the bat", "One of your batters scores a hundred.", 0, {}, {}},
      {"fifer", "match", "Five-star", "One of your bowlers takes five wickets.", 0, {}, {}},
      {"playoffs", "season", "Business end", "Finish a league season in the top four.", 1,
       [](const Ctx& c) {
         return static_cast<int>(std::count_if(c.history.begin(), c.history.end(),
                                               [](const SeasonArchive& h) { return h.userPos && *h.userPos <= 4; }));
       },
       [](const Ctx& c) -> std::optional<int> {
         for (const auto& h : c.history)
           if (h.userPos && *h.userPos <= 4) return h.number;
         return std::nullopt;
       }},
      {"exceeded", "season", "Board room hero", "Exceed the board's expectations in a season.", 1,
       [](const Ctx& c) {
         return static_cast<int>(std::count_if(c.board.begin(), c.board.end(),
                                               [](const BoardEntry& b) { return b.verdict == "Exceeded expectations"; }));
       },
       [](const Ctx& c) -> std::optional<int> {
         for (const auto& b : c.board)
           if (b.verdict == "Exceeded expectations") return b.season;
         return std::nullopt;
       }},
      {"champion", "season", "Champions", "Win the title.", 1,
       [](const Ctx& c) { return static_cast<int>(titles(c.history).size()); },
       [](const Ctx& c) -> std::optional<int> {
         const auto t = titles(c.history);
         return t.empty() ? std::nullopt : std::optional<int>(t[0]);
       }},
      {"double", "season", "Back to back", "Win the title two seasons running.", 1,
       [](const Ctx& c) { return backToBack(c.history) ? 1 : 0; },
       [](const Ctx& c) { return backToBack(c.history); }},
      {"dynasty", "season", "Dynasty", "Win three titles.", 3,
       [](const Ctx& c) { return static_cast<int>(titles(c.history).size()); },
       [](const Ctx& c) -> std::optional<int> {
         const auto t = titles(c.history);
         return t.size() > 2 ? std::optional<int>(t[2]) : std::nullopt;
       }},
      {"phoenix", "season", "Second act", "Win the title after being sacked.", 1,
       [](const Ctx& c) { return afterSacking(c) ? 1 : 0; }, [](const Ctx& c) { return afterSacking(c); }},
      {"seasons_5", "season", "Part of the furniture", "Manage five full seasons.", 5,
       [](const Ctx& c) { return static_cast<int>(c.history.size()); },
       [](const Ctx& c) -> std::optional<int> {
         return c.history.size() > 4 ? std::optional<int>(c.history[4].number) : std::nullopt;
       }},
  };
  return defs;
}

}  // namespace

std::optional<int> managedSide(const Match& match) {
  if (match.userTeam) return match.userTeam;
  if (match.managed) {
    for (int s = 0; s < 2; ++s)
      if (match.teams[static_cast<size_t>(s)].id == *match.managed) return s;
  }
  return std::nullopt;
}

std::vector<std::string> matchFeats(const Match& match, int side) {
  const auto& r = match.result;
  std::vector<const Innings*> main, mine, theirs;
  for (const auto& i : match.innings)
    if (!i.superOver) {
      main.push_back(&i);
      (i.batting == side ? mine : theirs).push_back(&i);
    }
  std::vector<std::string> out;
  auto any = [](const std::vector<const Innings*>& v, auto f) { return std::any_of(v.begin(), v.end(), f); };
  if (any(mine, [](const Innings* i) { return i->runs >= 220; })) out.push_back("total_220");
  if (any(mine, [](const Innings* i) {
        int n = 0;
        for (const auto& id : i->battedOrder) n += i->batters.at(id).sixes;
        return n >= 15;
      }))
    out.push_back("sixes_15");
  if (any(mine, [](const Innings* i) {
        return std::any_of(i->battedOrder.begin(), i->battedOrder.end(),
                           [&](const PlayerId& id) { return i->batters.at(id).runs >= 100; });
      }))
    out.push_back("ton");
  if (any(theirs, [](const Innings* i) {
        return std::any_of(i->bowlingOrder.begin(), i->bowlingOrder.end(),
                           [&](const PlayerId& id) { return i->bowling.at(id).wickets >= 5; });
      }))
    out.push_back("fifer");
  if (any(theirs, [](const Innings* i) { return i->wickets >= 10 && i->runs < 100; })) out.push_back("skittled");
  if (!r || r->winner != side || main.size() < 2) return out;
  const Innings& first = *main[0];
  const Innings& second = *main[1];
  if (r->superOver) {
    out.push_back("super_over");
    return out;
  }
  if (match.dls && match.dls->abandoned) return out;
  const bool battedFirst = first.batting == side;
  const int target = second.target.value_or(0);
  if (battedFirst) {
    const int margin = target - 1 - second.runs;
    if (margin <= 3) out.push_back("heart_stopper");
    if (margin >= 80) out.push_back("thrashing");
    if (first.runs < 140 && !(match.dls && match.dls->target) && first.maxBalls >= 120) out.push_back("defend_low");
  } else {
    if (second.maxBalls - second.legalBalls <= 1) out.push_back("heart_stopper");
    if (second.wickets == 0) out.push_back("thrashing");
    if (target >= 200) out.push_back("chase_200");
  }
  return out;
}

std::vector<std::string> updateManager(Career& career, const Match& match, const std::string& when) {
  const auto side = managedSide(match);
  if (!side) return {};
  ManagerRecord& m = career.manager;
  const auto& r = match.result;
  m.played += 1;
  if (r && r->winner == *side) {
    m.wins += 1;
    m.streak += 1;
    m.bestStreak = std::max(m.bestStreak, m.streak);
  } else if (!(r && r->noResult)) {
    m.streak = 0;
  }
  static const std::vector<SeasonArchive> noHistory;
  static const std::vector<BoardEntry> noBoard;
  const Ctx ctx{m, noHistory, noBoard};
  const auto feats = matchFeats(match, *side);
  std::vector<std::string> fresh;
  for (const auto& a : ACHIEVEMENTS()) {
    if (a.group != "match" || m.unlocked.contains(std::string(a.key))) continue;
    const bool got = a.count ? a.count(ctx) >= a.goal : std::find(feats.begin(), feats.end(), a.key) != feats.end();
    if (got) {
      m.unlocked.set(std::string(a.key), Unlock{when, match.teams[static_cast<size_t>(1 - *side)].id});
      fresh.emplace_back(a.key);
    }
  }
  return fresh;
}

std::vector<AchievementState> achievementList(const ManagerRecord& manager, const std::vector<SeasonArchive>& history,
                                              const std::vector<BoardEntry>& board) {
  const Ctx ctx{manager, history, board};
  std::vector<AchievementState> out;
  for (const auto& a : ACHIEVEMENTS()) {
    AchievementState s;
    s.key = a.key, s.group = a.group, s.title = a.title, s.text = a.text;
    const Unlock* got = a.group == "match" ? manager.unlocked.find(std::string(a.key)) : nullptr;
    const int goal = a.goal ? a.goal : 1;
    const std::optional<int> progress = a.count ? std::optional<int>(std::min(a.count(ctx), goal)) : std::nullopt;
    s.done = a.group == "match" ? got != nullptr : (progress && *progress >= goal);
    const std::optional<int> season = s.done && a.when ? a.when(ctx) : std::nullopt;
    if (got)
      s.when = got->when, s.vs = got->vs;
    else if (season)
      s.when = cat("Season ", *season);
    if (goal > 1) s.progress = progress, s.goal = goal;
    out.push_back(std::move(s));
  }
  return out;
}

}  // namespace t20
