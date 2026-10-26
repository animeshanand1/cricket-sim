#include "ui/screens/match_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/milestones.hpp"
#include "engine/mindset.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "engine/season.hpp"
#include "ui/screens/ui_context.hpp"
#include "ui/anim.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/match/charts.hpp"
#include "ui/match/crease.hpp"
#include "ui/match/result_panels.hpp"
#include "ui/screens/common.hpp"
#include "ui/match/moment_banner.hpp"
#include "ui/match/scoreboard.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

const Player& P(const Match& m, const PlayerId& id) { return m.players.at(id); }

TeamMeta teamMeta(const MatchTeam& t) {
  TeamMeta tm;
  tm.id = t.id;
  tm.name = t.name;
  tm.shortName = t.id;
  tm.color = t.color;
  return tm;
}

std::string fixed(double v, int dp) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", dp, v);
  return buf;
}

BallPipData pipOf(const Ball& b) {
  BallPipData p;
  p.text = b.label;
  p.runs = b.runs;
  p.isWicket = b.kind == BallKind::wicket;
  p.isFour = b.kind == BallKind::b4;
  p.isSix = b.kind == BallKind::b6;
  p.isExtra = b.kind == BallKind::extra;
  p.isSpecial = b.special.has_value();
  return p;
}

// Engine Match -> the scoreboard's view data (S05 §4)
ScoreboardViewData scoreboardData(const Match& m, const std::optional<std::array<double, 2>>& win) {
  ScoreboardViewData d;
  const Innings& inn = inningsOf(m);
  d.battingTeam = teamMeta(m.teams[static_cast<size_t>(inn.batting)]);
  d.bowlingTeam = teamMeta(m.teams[static_cast<size_t>(inn.bowlingSide)]);
  d.inningsLabel = inn.superOver ? "Super Over" : m.current == 0 ? "1st innings" : "2nd innings";
  d.runs = inn.runs;
  d.wickets = inn.wickets;
  d.balls = inn.legalBalls;

  const double crr = runRate(inn.runs, inn.legalBalls);
  d.stats.push_back({"Run rate", fixed(crr, 2)});
  if (inn.target) {
    d.targetRuns = *inn.target;
    d.stats.push_back({m.dls && m.dls->target ? "DLS target" : "Target", std::to_string(*inn.target)});
    const auto req = requiredRate(inn);
    d.stats.push_back({"Req. rate", req && m.status == MatchStatus::live ? fixed(*req, 2) : "–", true});
  } else {
    d.stats.push_back({"Partnership", std::to_string(inn.partnership.runs) + " (" + std::to_string(inn.partnership.balls) + ")"});
  }

  // Headline
  const int ballsLeft = inn.maxBalls - inn.legalBalls;
  if (m.status == MatchStatus::complete && m.result) {
    d.headline = m.result->text;
  } else if (m.status == MatchStatus::break_) {
    const Innings& next = m.innings.size() > 1 ? m.innings[1] : inn;
    d.headline = "Target " + std::to_string(next.target ? *next.target : inn.runs + 1);
  } else if (inn.target) {
    const int need = *inn.target - inn.runs;
    d.headline = "Need " + std::to_string(need) + " from " + std::to_string(ballsLeft) + (ballsLeft == 1 ? " ball" : " balls");
  } else if (inn.legalBalls >= 12) {
    d.headline = "Projected " + std::to_string(static_cast<int>(std::lround(inn.runs + crr * ballsLeft / 6)));
  } else {
    d.headline = "Early days";
  }

  // This over / last over
  const int oi = overIndex(inn);
  const Over* ov = nullptr;
  if (oi < static_cast<int>(inn.overs.size()) && !inn.overs[static_cast<size_t>(oi)].balls.empty()) {
    ov = &inn.overs[static_cast<size_t>(oi)];
    d.overLabel = "This over";
  } else if (oi > 0 && oi - 1 < static_cast<int>(inn.overs.size())) {
    ov = &inn.overs[static_cast<size_t>(oi - 1)];
    d.overLabel = "Last over";
  }
  if (ov) {
    for (const Ball& b : ov->balls) d.currentOverPips.push_back(pipOf(b));
    if (!d.currentOverPips.empty() && d.overLabel == "This over") d.currentOverPips.back().isNewest = true;
    d.overRuns = ov->runs;
  }

  // Tags: phase + conditions notes
  const ConditionsSummary cs = conditionsSummary(m);
  if (m.status != MatchStatus::complete)
    d.tags.emplace_back(inn.superOver ? "Super Over" : std::string(phaseLabel(phaseAt(&inn, std::min(oi, inningsOvers(&inn) - 1)))));
  for (const auto& n : cs.notes) d.tags.push_back(n);
  d.freeHit = inn.freeHit && m.status == MatchStatus::live;

  if (win) d.battingWinPct = static_cast<float>((*win)[static_cast<size_t>(inn.batting)] * 100.0);
  else d.winPending = m.status != MatchStatus::toss;

  // Fans: home side first
  const int home = m.fans.home ? *m.fans.home : 0;
  for (int s = 0; s < 2; ++s) {
    FansSideData f;
    f.team = teamMeta(m.teams[static_cast<size_t>(s)]);
    f.fansCount = static_cast<int>(m.fans.now[static_cast<size_t>(s)]);
    const double delta = m.fans.now[static_cast<size_t>(s)] - m.fans.overStart[static_cast<size_t>(s)];
    f.trend = delta > 0 ? 1 : delta < 0 ? -1 : 0;
    (s == home ? d.fansHome : d.fansAway) = f;
  }
  d.groundCapacity = static_cast<int>(m.fans.capacity);

  d.conditions = std::string(cs.label) + " · pitch wear " + std::to_string(static_cast<int>(cs.wear)) + "%";
  if (m.dew) d.conditions += " · evening game, dew expected in the chase";
  return d;
}

void eyebrow(const std::string& text, const ColorRGBA& c) {
  FontScope fs(FontRole::Eyebrow);
  ImGui::TextColored(vec(c), "%s", upper(text).c_str());
}

}  // namespace

MatchScreen::MatchScreen(UiContext& ctx) : m_session(ctx.session), m_ctx(&ctx) {}

void MatchScreen::onEnter() {
  m_scorecardInnings = -1;
  m_captain = CaptainPanelState{};
  m_impact = ImpactPanelState{};
  m_field = FieldViewState{0, {}, m_field.showFielders, m_field.open, 0};
  m_dev = DevReportState{};
  m_detailTab = 0;
}

std::vector<MatchExit> MatchScreen::exits() {
  if (!m_ctx) return {{"Rematch", [this] { m_session.rematch(); }}, {"New fixture", [this] { newFixture(); }}};
  UiContext* ctx = m_ctx;
  if (ctx->matchIsFixture()) return {{"Back to the season", [ctx] { ctx->leaveMatch(); }}};
  return {{"Rematch", [ctx] { ctx->rematch(); }}, {"New game", [ctx] { ctx->leaveMatch(); }}};
}

void MatchScreen::draw(float contentWidth) {
  // Toss decided but not yet out in the middle (the nav bar's Match link): take it from the Game
  if (m_ctx && !m_session.hasMatch()) m_ctx->startLiveMatch();
  handleKeys();
  consumeEvents();
  const auto& th = currentTheme();
  (void)th;

  if (!m_session.hasMatch()) {
    ImGui::TextUnformatted("No match in progress.");
    return;
  }

  drawHeader();
  ImGui::Dummy(ImVec2(0, 4));
  const Match& match = m_session.match();
  const MatchStatus status = match.status;
  const bool complete = status == MatchStatus::complete;
  const float gap = 18.0f;

  // S06: the result, what it brought, who developed, the league's favourites
  if (complete) {
    drawResultBanner(match, exits());
    ImGui::Dummy(ImVec2(0, 8));
    if (m_ctx) {
      const auto& st = m_ctx->game.state();
      drawMatchMilestones(match, st.career, st.season.history, st.world.manager.history);
      ImGui::Dummy(ImVec2(0, 6));
      drawDevelopmentReport(match, st.career, m_dev);
      ImGui::Dummy(ImVec2(0, 6));
      popularityMeter(*m_ctx, "##matchpop");
      ImGui::Dummy(ImVec2(0, 8));
    }
  }

  // Dock rail on the left while the match is on, the match on the right
  float mainW = contentWidth;
  if (!complete) {
    const float dockW = 168.0f;
    ImGui::BeginChild("##dock", ImVec2(dockW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground);
    drawDock();
    ImGui::EndChild();
    ImGui::SameLine(0, gap);
    mainW = contentWidth - dockW - gap;
  }
  ImGui::BeginChild("##main", ImVec2(mainW, 0), ImGuiChildFlags_AutoResizeY,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawScoreboardArea(mainW);
  ImGui::Dummy(ImVec2(0, 8));

  // Two columns (>= 980px), else one
  const bool twoCol = mainW >= 900.0f;
  const float colW = twoCol ? (mainW - gap) * 0.5f : mainW;
  ImGui::BeginChild("##left", ImVec2(colW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawCaptainPanel(m_session, m_captain, exits());
  ImGui::Dummy(ImVec2(0, 6));
  if (status == MatchStatus::live) {
    drawCrease(match, m_ctx ? &m_ctx->game.state().career : nullptr);
    ImGui::Dummy(ImVec2(0, 6));
  }
  if (status == MatchStatus::live || status == MatchStatus::break_) drawImpactPanel(m_session, m_impact);
  if (complete) {
    beginPanel("##unfolded", "How the match unfolded");
    const float w = ImGui::GetContentRegionAvail().x;
    drawWorm(match, w, 230);
    ImGui::Dummy(ImVec2(0, 6));
    drawWinChart(match, m_session.winPoints(), w, 190);
    endPanel();
  }
  ImGui::EndChild();
  if (twoCol) ImGui::SameLine(0, gap);
  ImGui::BeginChild("##right", ImVec2(colW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawFieldViewPanel(match, m_field);
  ImGui::Dummy(ImVec2(0, 6));
  drawCommentary();
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0, 6));
  drawDetails();
  ImGui::EndChild();
}

void MatchScreen::drawHeader() {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  std::string kind = "Friendly";
  if (m.fixture) {
    kind = "Season " + std::to_string(m.fixture->seasonNo) + " · " +
           (m.fixture->stage == Stage::league ? "Match " + std::to_string(m.fixture->no) : std::string(stageLabel(m.fixture->stage)));
  }
  eyebrow(kind + " · " + (m.fixture && !m.fixture->venue.empty() ? m.fixture->venue : m_session.homeTeam().ground), th.goldText);
  {
    FontScope fs(FontRole::H1);
    ImGui::Text("%s v %s", m.teams[0].name.c_str(), m.teams[1].name.c_str());
  }
  std::string sub(pitchDef(m.pitch).label);
  if (m.toss && m.toss->decision) {
    sub += " · " + m.teams[static_cast<size_t>(m.toss->winner)].name + " won the toss and chose to " +
           std::string(name(*m.toss->decision));
  }
  ImGui::TextColored(vec(th.muted), "%s", sub.c_str());
}

void MatchScreen::drawDock() {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  const Innings& inn = inningsOf(m);
  const bool live = m.status == MatchStatus::live;

  // mini score
  eyebrow(m.teams[static_cast<size_t>(inn.batting)].id, th.goldText);
  {
    FontScope fs(FontRole::Kpi);
    ImGui::Text("%d/%d", inn.runs, inn.wickets);
  }
  ImGui::TextColored(vec(th.muted), "%s ov", oversText(inn.legalBalls).c_str());
  ImGui::Dummy(ImVec2(0, 6));

  auto full = [](const char* label, ButtonKind kind, bool enabled) { return buttonFull(label, kind, enabled); };
  const char* waiting = pendingDecisionLabel(m);

  if (m.status == MatchStatus::break_) {
    if (full("Start the chase", ButtonKind::Primary, true)) m_session.startChase();
  } else if (waiting) {
    // a call to make: jump to it (MatchDock "dock-wait")
    if (full(waiting, ButtonKind::Primary, true)) m_captain.scrollToDecision = true;
  } else {
    if (full("Next ball", ButtonKind::Primary, live)) m_session.nextBall();
    if (full("Next over", ButtonKind::Default, live)) m_session.nextOver();
  }
  const char* watchLabel = m_session.watching ? (m.awaiting != Awaiting::none ? "Waiting" : "Pause") : "Watch";
  if (full(watchLabel, ButtonKind::Default, live)) m_session.watching = !m_session.watching;
  if (m_session.watching) {
    static const double secs[] = {1.4, 0.7, 0.26};
    m_speed = segmented({"Slow", "Med", "Fast"}, m_speed);
    m_session.watchInterval = secs[m_speed];
    switchToggle("Stop at wickets", &m_session.stopOnWicket);
  }
  ImGui::Dummy(ImVec2(0, 4));
  if (!waiting) {
    if (full("Sim innings", ButtonKind::Quiet, live || m.status == MatchStatus::break_)) m_session.simInnings();
    if (full("Sim match", ButtonKind::Quiet, m.status != MatchStatus::complete)) m_session.simMatch();
  }
  if (!m_ctx) {
    if (full("New match", ButtonKind::Quiet, true)) newFixture();
  } else if (m.status == MatchStatus::complete) {
    const auto ex = exits();
    for (const auto& e : ex)
      if (full(e.label.c_str(), ButtonKind::Quiet, true)) e.action();
  }
  ImGui::Dummy(ImVec2(0, 6));
  {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "Keys: N ball · O over\nW watch · R/A review");
  }
}

void MatchScreen::newFixture() {
  ++m_fixture;
  const int n = static_cast<int>(m_session.league().size());
  const int home = m_fixture % n, away = (m_fixture * 3 + 1) % n == home ? (home + 1) % n : (m_fixture * 3 + 1) % n;
  m_session.newMatch(home, away, 1000 + m_fixture * 7919);
  m_scorecardInnings = -1;
  m_captain = CaptainPanelState{};
  m_impact = ImpactPanelState{};
}

void MatchScreen::drawScoreboardArea(float width) {
  const Match& m = m_session.match();
  const ImVec2 o = ImGui::GetCursorScreenPos();
  drawScoreboard(nullptr, scoreboardData(m, m_session.winChance()), width);
  // Moment banner sits over the right end of the board's top row
  drawMomentBanner(ImGui::GetWindowDrawList(), o.x + width - 30, o.y + 22);
}

void MatchScreen::drawCommentary() { drawCommentaryPanel(m_session.match(), m_comment); }

void MatchScreen::drawDetails() {
  const Match& m = m_session.match();
  beginPanel("##details", nullptr);
  m_detailTab = tabs({{"card", "Scorecard", ""}, {"worm", "Worm", ""}, {"manhattan", "Manhattan", ""}, {"win", "Win chance", ""}},
                     m_detailTab);
  const int n = static_cast<int>(m.innings.size());
  int sel = m_scorecardInnings < 0 || m_scorecardInnings >= n ? m.current : m_scorecardInnings;
  if (m_detailTab == 0 && n > 1) {
    ImGui::SameLine(0, 16);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    for (int i = 0; i < n; ++i) {
      const Innings& inn = m.innings[static_cast<size_t>(i)];
      std::string label = m.teams[static_cast<size_t>(inn.batting)].id + (inn.superOver ? " (SO)" : "");
      if (chip((label + "##inn" + std::to_string(i)).c_str(), sel == i)) m_scorecardInnings = sel = (i == m.current ? -1 : i);
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
  }
  ImGui::Dummy(ImVec2(0, 6));
  const float w = ImGui::GetContentRegionAvail().x;
  switch (m_detailTab) {
    case 1: drawWorm(m, w, 300); break;
    case 2: drawManhattan(m, w, 280); break;
    case 3: drawWinChart(m, m_session.winPoints(), w, 260); break;
    default: drawScorecard(sel); break;
  }
  endPanel();
}

void MatchScreen::drawScorecard(int sel) {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  const Innings& inn = m.innings[static_cast<size_t>(sel)];
  const MatchTeam& bat = m.teams[static_cast<size_t>(inn.batting)];
  const MatchTeam& bowl = m.teams[static_cast<size_t>(inn.bowlingSide)];
  auto surnameOf = [&](const PlayerId& id) { return surname(P(m, id).name); };
  auto impactIn = [&](const MatchTeam& t, const PlayerId& id) { return t.impact && t.impact->in == id; };
  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, vec(ColorRGBA(th.surface2.r, th.surface2.g, th.surface2.b, 0.55f)));
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0, 0, 0, 0));

  // Batting
  {
    FontScope fs(FontRole::H3);
    ImGui::Text("%s batting%s", bat.name.c_str(), inn.superOver ? (" \xc2\xb7 Super Over " + std::to_string(inn.superOver)).c_str() : "");
  }
  if (ImGui::BeginTable("##bat", 7, flags)) {
    ImGui::TableSetupColumn("Batter", ImGuiTableColumnFlags_WidthStretch, 3.2f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 3.6f);
    for (const char* h : {"R", "B", "4s", "6s", "SR"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch, 0.8f);
    ImGui::TableHeadersRow();
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings* bp = inn.batters.find(id);
      if (!bp) continue;
      const BatterInnings& b = *bp;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(id.c_str());
      if (playerLink(id, P(m, id).name) && m_ctx) m_ctx->openCard(id);
      ImGui::PopID();
      std::string marks;
      if (bat.captainId == id) marks += " (c)";
      if (bat.keeperId == id) marks += " \xe2\x80\xa0";
      if (!marks.empty()) {
        ImGui::SameLine(0, 2);
        ImGui::TextColored(vec(th.muted), "%s", marks.c_str());
      }
      if (impactIn(bat, id)) {
        ImGui::SameLine(0, 6);
        ipTag();
      }
      ImGui::TableNextColumn();
      const bool live = !inn.complete && m.status == MatchStatus::live;
      ImGui::TextColored(vec(th.muted), "%s", b.out ? (b.howOut ? b.howOut->c_str() : "out") : live ? "batting" : "not out");
      ImGui::TableNextColumn();
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::Text("%d%s", b.runs, b.out ? "" : "*");
      }
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.balls);
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.fours);
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.sixes);
      ImGui::TableNextColumn();
      ImGui::Text("%s", b.balls ? fixed(100.0 * b.runs / b.balls, 1).c_str() : "\xe2\x80\x93");
    }
    // extras and the total
    const Extras& e = inn.extras;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(vec(th.ink2), "Extras");
    ImGui::TableNextColumn();
    ImGui::TextColored(vec(th.muted), "b %d, lb %d, wd %d, nb %d", e.b, e.lb, e.wd, e.nb);
    ImGui::TableNextColumn();
    ImGui::Text("%d", e.b + e.lb + e.wd + e.nb);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    {
      FontScope fs(FontRole::BodyStrong);
      ImGui::TextUnformatted("Total");
    }
    ImGui::TableNextColumn();
    ImGui::TextColored(vec(th.muted), "%s ov \xc2\xb7 RR %s", oversText(inn.legalBalls).c_str(), fixed(runRate(inn.runs, inn.legalBalls), 2).c_str());
    ImGui::TableNextColumn();
    {
      FontScope fs(FontRole::BodyStrong);
      ImGui::Text("%d/%d", inn.runs, inn.wickets);
    }
    ImGui::EndTable();
  }
  // notes: yet to bat, fall of wickets, reviews
  ImGui::PushTextWrapPos(0);
  if (!inn.yetToBat.empty() && !inn.complete) {
    std::string names;
    for (const PlayerId& id : inn.yetToBat) names += (names.empty() ? "" : ", ") + P(m, id).name;
    ImGui::TextColored(vec(th.muted), "Yet to bat: %s", names.c_str());
  }
  if (!inn.fow.empty()) {
    std::string fow;
    for (const FallOfWicket& f : inn.fow)
      fow += (fow.empty() ? "" : ", ") + std::to_string(f.runs) + "-" + std::to_string(f.wicket) + " (" + surnameOf(f.playerId) + ", " + f.over + ")";
    ImGui::TextColored(vec(th.muted), "Fall of wickets: %s", fow.c_str());
  }
  if (!inn.reviews.empty()) {
    std::string rv;
    for (const ReviewRecord& r : inn.reviews)
      rv += (rv.empty() ? "" : ", ") + m.teams[static_cast<size_t>(r.side)].id + ": " + std::string(name(r.type)) + " v " + surnameOf(r.batter) +
            (r.overturned ? ", overturned (" : ", upheld (") + ballTag(r.ball - 1) + ")";
    ImGui::TextColored(vec(th.muted), "Reviews: %s", rv.c_str());
  }
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0, 8));

  // Bowling
  {
    FontScope fs(FontRole::H3);
    ImGui::Text("%s bowling", bowl.name.c_str());
  }
  if (ImGui::BeginTable("##bowl", 9, flags)) {
    ImGui::TableSetupColumn("Bowler", ImGuiTableColumnFlags_WidthStretch, 3.2f);
    for (const char* h : {"O", "M", "R", "W", "Econ", "Dots", "Wd", "Nb"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableHeadersRow();
    for (const auto& [id, f] : inn.bowling) {
      if (f.balls == 0 && f.runs == 0) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(id.c_str());
      if (playerLink(id, P(m, id).name) && m_ctx) m_ctx->openCard(id);
      ImGui::PopID();
      if (impactIn(bowl, id)) {
        ImGui::SameLine(0, 6);
        ipTag();
      }
      ImGui::TableNextColumn();
      ImGui::Text("%s", oversText(f.balls).c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%d", f.maidens);
      ImGui::TableNextColumn();
      ImGui::Text("%d", f.runs);
      ImGui::TableNextColumn();
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::TextColored(vec(f.wickets >= 3 ? th.goldText : th.ink), "%d", f.wickets);
      }
      ImGui::TableNextColumn();
      ImGui::Text("%s", f.balls ? fixed(f.runs * 6.0 / f.balls, 2).c_str() : "\xe2\x80\x93");
      ImGui::TableNextColumn();
      ImGui::Text("%d", f.dots);
      ImGui::TableNextColumn();
      ImGui::Text("%d", f.wides);
      ImGui::TableNextColumn();
      ImGui::Text("%d", f.noballs);
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleColor(2);
}

void MatchScreen::handleKeys() {
  // MatchDock keys: N / Space next ball, O next over, W watch, Esc pause; R / A on a review call
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || io.KeyCtrl || io.KeyAlt) return;
  const Match& m = m_session.match();
  if (m.status != MatchStatus::live) return;
  if (m.awaiting == Awaiting::review) {
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_session.reviewCall(true);
    else if (ImGui::IsKeyPressed(ImGuiKey_A, false)) m_session.reviewCall(false);
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_session.watching = !m_session.watching;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) m_session.watching = false;
  if (m.awaiting != Awaiting::none) return;
  if (ImGui::IsKeyPressed(ImGuiKey_N, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false)) m_session.nextBall();
  else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) m_session.nextOver();
}

void MatchScreen::consumeEvents() {
  const Match& m = m_session.match();
  const std::vector<app::BallEvent> events = m_session.takeEvents();
  if (!events.empty() && !events.back().outcome.empty()) startTween("pip_pop", 0.4f, 1.0f, 0.35, EaseType::EaseOutCubic);

  // A new match: start counting from here (no banner for what was already there)
  const Innings& inn = inningsOf(m);
  if (m.id != m_momentMatch) {
    m_momentMatch = m.id;
    m_momentInnings = m.current;
    m_momentLines = inn.commentary.size();
    m_reached.clear();
    if (m_ctx)
      for (const auto& r : reachedInMatch(m_ctx->game.state().career.players, m)) m_reached.insert(r.key);
    return;
  }
  const size_t lines = inn.commentary.size();
  const bool sameInnings = m.current == m_momentInnings;
  const size_t fresh = sameInnings && lines > m_momentLines ? lines - m_momentLines : 0;
  m_momentInnings = m.current;
  m_momentLines = lines;

  // Career milestones reached with this ball (not once the match is recorded, not in a Super Over)
  std::optional<MomentEvent> career;
  if (m_ctx) {
    const Career& c = m_ctx->game.state().career;
    const bool recorded = std::find(c.recorded.begin(), c.recorded.end(), m.id) != c.recorded.end();
    for (const auto& r : reachedInMatch(c.players, m)) {
      if (m_reached.count(r.key)) continue;
      m_reached.insert(r.key);
      if (!recorded && !inn.superOver && fresh >= 1 && fresh <= 14)
        career = MomentEvent{MomentKind::Milestone, milestoneText(r.type, r.value), "Career milestone \xc2\xb7 " + m.players.at(r.id).name};
    }
  }
  // 1-14 new lines: the biggest thing that happened (a sim brings more and gets no banner)
  if (fresh < 1 || fresh > 14) return;
  auto rank = [](CommentKind k) {
    switch (k) {
      case CommentKind::result: return 5;
      case CommentKind::wicket: return 4;
      case CommentKind::milestone: return 3;
      case CommentKind::six: return 2;
      case CommentKind::four: return 1;
      default: return 0;
    }
  };
  const CommentaryLine* best = nullptr;
  for (size_t i = lines - fresh; i < lines; ++i) {
    const CommentaryLine& c = inn.commentary[i];
    if (rank(c.kind) > (best ? rank(best->kind) : 0)) best = &c;
  }
  // "Bowler to Batter, ..." -> the batter
  auto batterOf = [](const std::string& text) {
    const size_t to = text.find(" to "), comma = text.find(',');
    return to != std::string::npos && comma != std::string::npos && comma > to ? upper(text.substr(to + 4, comma - to - 4)) : std::string();
  };
  if (best && best->kind == CommentKind::result) {
    triggerMomentBanner({MomentKind::Result, "Result", m.result ? m.result->text : best->text});
  } else if (career) {
    triggerMomentBanner(*career);
  } else if (best) {
    switch (best->kind) {
      case CommentKind::wicket: triggerMomentBanner({MomentKind::Wicket, "Wicket", batterOf(best->text)}); break;
      case CommentKind::milestone: triggerMomentBanner({MomentKind::Milestone, best->text, ""}); break;
      case CommentKind::six: triggerMomentBanner({MomentKind::Six, "Six!", batterOf(best->text)}); break;
      case CommentKind::four: triggerMomentBanner({MomentKind::Four, "Four", batterOf(best->text)}); break;
      default: break;
    }
  }
}

}  // namespace t20::ui
