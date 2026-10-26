#include "ui/screens/match_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/mindset.hpp"
#include "engine/pitch.hpp"
#include "engine/rules.hpp"
#include "ui/anim.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/match/crease.hpp"
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

ColorRGBA commentaryColor(CommentKind k) {
  const auto& th = currentTheme();
  switch (k) {
    case CommentKind::wicket: return th.red;
    case CommentKind::four: return th.blue;
    case CommentKind::six: return th.gold;
    case CommentKind::milestone: return th.purple;
    case CommentKind::result: return th.green;
    case CommentKind::extra: return th.orange;
    case CommentKind::drop: return th.orange;
    case CommentKind::review: return th.purple;
    case CommentKind::over: return th.lineStrong;
    default: return th.line;
  }
}

void eyebrow(const std::string& text, const ColorRGBA& c) {
  FontScope fs(FontRole::Eyebrow);
  ImGui::TextColored(vec(c), "%s", upper(text).c_str());
}

}  // namespace

void MatchScreen::draw(float contentWidth) {
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

  // Dock rail on the left, the match on the right
  const float dockW = 168.0f, gap = 18.0f;
  ImGui::BeginChild("##dock", ImVec2(dockW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground);
  drawDock();
  ImGui::EndChild();
  ImGui::SameLine(0, gap);
  const float mainW = contentWidth - dockW - gap;
  ImGui::BeginChild("##main", ImVec2(mainW, 0), ImGuiChildFlags_AutoResizeY,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawScoreboardArea(mainW);
  ImGui::Dummy(ImVec2(0, 8));

  // Two columns (>= 980px), else one
  const bool twoCol = mainW >= 900.0f;
  const float colW = twoCol ? (mainW - gap) * 0.5f : mainW;
  ImGui::BeginChild("##left", ImVec2(colW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawCaptainPanel(m_session, m_captain, [this] { newFixture(); });
  ImGui::Dummy(ImVec2(0, 6));
  const MatchStatus status = m_session.match().status;
  if (status == MatchStatus::live) {
    drawCrease(m_session.match());
    ImGui::Dummy(ImVec2(0, 6));
  }
  if (status == MatchStatus::live || status == MatchStatus::break_) drawImpactPanel(m_session, m_impact);
  ImGui::EndChild();
  if (twoCol) ImGui::SameLine(0, gap);
  ImGui::BeginChild("##right", ImVec2(colW, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
  drawCommentary();
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0, 6));
  drawScorecard();
  ImGui::EndChild();
}

void MatchScreen::drawHeader() {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  eyebrow("Friendly · " + m_session.homeTeam().ground, th.goldText);
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
    static const double secs[] = {1.6, 0.9, 0.35};
    m_speed = segmented({"Slow", "Med", "Fast"}, m_speed);
    m_session.watchInterval = secs[m_speed];
    switchToggle("Stop at wickets", &m_session.stopOnWicket);
  }
  ImGui::Dummy(ImVec2(0, 4));
  if (!waiting) {
    if (full("Sim innings", ButtonKind::Quiet, live || m.status == MatchStatus::break_)) m_session.simInnings();
    if (full("Sim match", ButtonKind::Quiet, m.status != MatchStatus::complete)) m_session.simMatch();
  }
  if (full("New match", ButtonKind::Quiet, true)) newFixture();
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

void MatchScreen::drawCommentary() {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  beginPanel("##commentary", "Commentary");
  // newest first, across both innings
  int shown = 0;
  for (int i = static_cast<int>(m.innings.size()) - 1; i >= 0 && shown < 16; --i) {
    const auto& lines = m.innings[static_cast<size_t>(i)].commentary;
    for (auto it = lines.rbegin(); it != lines.rend() && shown < 16; ++it, ++shown) {
      const CommentaryLine& c = *it;
      ImDrawList* dl = ImGui::GetWindowDrawList();
      const ImVec2 p = ImGui::GetCursorScreenPos();
      ImGui::SetCursorScreenPos(ImVec2(p.x + 12, p.y));
      ImGui::BeginGroup();
      if (!c.over.empty()) {
        FontScope fs(FontRole::Eyebrow);
        ImGui::TextColored(vec(th.muted), "%s", c.over.c_str());
        ImGui::SameLine();
      }
      ImGui::PushTextWrapPos(0);
      const bool strong = c.kind == CommentKind::wicket || c.kind == CommentKind::six || c.kind == CommentKind::result ||
                          c.kind == CommentKind::milestone;
      {
        FontScope fs(strong ? FontRole::BodyStrong : FontRole::Body);
        ImGui::TextColored(vec(c.kind == CommentKind::over || c.kind == CommentKind::info ? th.ink2 : th.ink), "%s", c.text.c_str());
      }
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();
      const float h = ImGui::GetItemRectSize().y;
      dl->AddRectFilled(ImVec2(p.x, p.y + 1), ImVec2(p.x + 3, p.y + h - 1), col(commentaryColor(c.kind)), 2.0f);
      ImGui::Dummy(ImVec2(0, 1));
    }
  }
  if (shown == 0) ImGui::TextColored(vec(th.muted), "Play a ball to start the commentary.");
  endPanel();
}

void MatchScreen::drawScorecard() {
  const auto& th = currentTheme();
  const Match& m = m_session.match();
  beginPanel("##scorecard", "Scorecard");
  const int n = static_cast<int>(m.innings.size());
  int sel = m_scorecardInnings < 0 || m_scorecardInnings >= n ? m.current : m_scorecardInnings;
  for (int i = 0; i < n; ++i) {
    const auto& t = m.teams[static_cast<size_t>(m.innings[static_cast<size_t>(i)].batting)];
    if (chip((t.id + "##inn" + std::to_string(i)).c_str(), sel == i)) m_scorecardInnings = sel = i;
    ImGui::SameLine(0, 6);
  }
  ImGui::NewLine();
  const Innings& inn = m.innings[static_cast<size_t>(sel)];

  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
  const float half = (ImGui::GetContentRegionAvail().x - 24) * 0.58f;
  ImGui::BeginGroup();
  if (ImGui::BeginTable("##bat", 7, flags, ImVec2(half, 0))) {
    ImGui::TableSetupColumn("Batter", ImGuiTableColumnFlags_WidthStretch, 3.2f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 3.4f);
    for (const char* h : {"R", "B", "4s", "6s", "SR"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableHeadersRow();
    for (const PlayerId& id : inn.battedOrder) {
      const BatterInnings* bp = inn.batters.find(id);
      if (!bp) continue;
      const BatterInnings& b = *bp;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      const bool atCrease = (inn.striker && *inn.striker == id) || (inn.nonStriker && *inn.nonStriker == id);
      ImGui::TextColored(vec(atCrease && !b.out ? th.goldText : th.ink), "%s%s", P(m, id).name.c_str(), atCrease && !b.out ? " *" : "");
      ImGui::TableNextColumn();
      ImGui::TextColored(vec(th.muted), "%s", b.out ? (b.howOut ? b.howOut->c_str() : "out") : "not out");
      ImGui::TableNextColumn();
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::Text("%d", b.runs);
      }
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.balls);
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.fours);
      ImGui::TableNextColumn();
      ImGui::Text("%d", b.sixes);
      ImGui::TableNextColumn();
      ImGui::Text("%s", b.balls ? fixed(100.0 * b.runs / b.balls, 1).c_str() : "–");
    }
    ImGui::EndTable();
  }
  if (!inn.yetToBat.empty() && !inn.complete) {
    std::string names;
    for (const PlayerId& id : inn.yetToBat) names += (names.empty() ? "" : ", ") + P(m, id).name;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + half);
    ImGui::TextColored(vec(th.muted), "Yet to bat: %s", names.c_str());
    ImGui::PopTextWrapPos();
  }
  const Extras& e = inn.extras;
  ImGui::TextColored(vec(th.muted), "Extras %d (wd %d, nb %d, lb %d, b %d)   Total %d/%d (%s ov)", e.wd + e.nb + e.lb + e.b, e.wd, e.nb,
                     e.lb, e.b, inn.runs, inn.wickets, oversText(inn.legalBalls).c_str());
  ImGui::EndGroup();
  ImGui::SameLine(0, 24);
  if (ImGui::BeginTable("##bowl", 6, flags, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    ImGui::TableSetupColumn("Bowler", ImGuiTableColumnFlags_WidthStretch, 3.0f);
    for (const char* h : {"O", "M", "R", "W", "Econ"}) ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();
    for (const auto& [id, f] : inn.bowling) {
      if (f.balls == 0 && f.runs == 0) continue;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%s", P(m, id).name.c_str());
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
      ImGui::Text("%s", f.balls ? fixed(f.runs * 6.0 / f.balls, 2).c_str() : "–");
    }
    ImGui::EndTable();
  }
  endPanel();
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
  // After a batch (Next over, sims) only the last ball gets a moment
  if (!events.empty()) {
    const app::BallEvent& ev = events.back();
    if (!ev.outcome.empty()) startTween("pip_pop", 0.4f, 1.0f, 0.35, EaseType::EaseOutCubic);
    if (ev.matchOver && m.result) {
      triggerMomentBanner({MomentKind::Result, m.result->winner ? "Winners!" : "Match over", m.result->text});
    } else if (ev.outcome == "W") {
      triggerMomentBanner({MomentKind::Wicket, "Wicket!", upper(ev.batter)});
    } else if (ev.outcome == "6") {
      triggerMomentBanner({MomentKind::Six, "Six!", upper(ev.batter)});
    } else if (ev.outcome == "4") {
      triggerMomentBanner({MomentKind::Four, "Four!", upper(ev.batter)});
    }
  }
}

}  // namespace t20::ui
