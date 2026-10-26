#include "ui/screens/gallery_screen.hpp"

#include <imgui.h>

#include "engine/rules.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include "ui/match/charts.hpp"
#include "ui/match/field_view.hpp"
#include "ui/match/moment_banner.hpp"
#include "ui/match/scoreboard.hpp"
#include "ui/match/win_and_fans.hpp"
#include "ui/theme.hpp"
#include "ui/view_data.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/forms.hpp"
#include "ui/widgets/meter_and_rating.hpp"
#include "ui/widgets/modal.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/stepper.hpp"
#include "ui/widgets/tags.hpp"
#include "ui/widgets/team_badge.hpp"

namespace t20::ui {

namespace {

const TeamMeta kTeams[] = {
    {"CHK", "Chennai Supreme Kings", "CHK", "#f5b83d", "#0081e8"},
    {"MUI", "Mumbay Islanders", "MUI", "#2f6fd1", "#d1ab3e"},
    {"KNR", "Kolkota Night Riders", "KNR", "#6a4bd4", "#f3b429"},
    {"RJR", "Rajasthan Regals", "RJR", "#e0479e", "#254aa5"},
};

PlayerMeta samplePlayer(const char* name, Role role, float fitness, bool injured = false) {
  PlayerMeta p;
  p.id = name;
  p.name = name;
  p.role = role;
  p.fitness = fitness;
  if (injured) p.injury = PlayerMeta::Injury{"Hamstring", 3};
  return p;
}

void caption(const char* text) {
  ImGui::Dummy(ImVec2(0, 6));
  eyebrowText(text);
}

}  // namespace

void GalleryScreen::onEnter() { m_activeTab = 0; }

void GalleryScreen::draw(float contentWidth) {
  const TeamMeta& demo = kTeams[1];
  pageHeader("Widget gallery", "Every widget in the design system: the night-broadcast theme, shared widgets and the match graphics.",
             "A6 · Design system", &demo);
  ImGui::Dummy(ImVec2(0, 6));
  m_activeTab = tabs({{"identity", "Identity", ""},
                      {"values", "Values and status", ""},
                      {"controls", "Controls and forms", ""},
                      {"tables", "Tables and panels", ""},
                      {"match", "Match graphics", ""}},
                     m_activeTab);
  ImGui::Dummy(ImVec2(0, 8));
  switch (m_activeTab) {
    case 0: drawIdentity(contentWidth); break;
    case 1: drawValues(contentWidth); break;
    case 2: drawControls(contentWidth); break;
    case 3: drawTables(contentWidth); break;
    default: drawMatch(contentWidth); break;
  }
}

void GalleryScreen::drawIdentity(float width) {
  const auto& th = currentTheme();
  beginPanel("##badges", "Team badges");
  caption("Sizes xs 18 · sm 24 · md 40 · lg 56 · xl 84");
  for (float size : {18.0f, 24.0f, 40.0f, 56.0f, 84.0f}) {
    teamBadge(kTeams[0], size);
    ImGui::SameLine(0, 14);
  }
  ImGui::NewLine();
  caption("Every franchise colour");
  for (const auto& t : kTeams) {
    teamBadge(t, 40);
    ImGui::SameLine(0, 10);
  }
  ImGui::NewLine();
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));

  beginPanel("##avatars", "Player avatars");
  caption("Initials on the team colour, role disc bottom right (sizes 30 · 36 · 48 · 64)");
  const Role roles[] = {Role::WK, Role::BAT, Role::AR, Role::BOWL};
  const char* names[] = {"Quinten de Kok", "Sachin Tendulker", "Hardik Pandia", "Jasprit Bumra"};
  for (int i = 0; i < 4; ++i) {
    avatar(samplePlayer(names[i], roles[i], 100), kTeams[i].color, 30.0f + i * 10 + (i == 3 ? 4 : 0), true);
    ImGui::SameLine(0, 14);
  }
  ImGui::NewLine();
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));

  beginPanel("##teamcards", "Team tiles");
  if (beginGrid("##tiles", columnsFor(width - 36, 240, 4), 10)) {
    for (int i = 0; i < 4; ++i) {
      gridNext();
      teamCard(kTeams[i], "Batting 80 · Bowling 84", i == 1, ImGui::GetContentRegionAvail().x, i != 3);
    }
    endGrid();
  }
  hint("Selected = gold border and tick; the last tile is disabled.");
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));

  beginPanel("##icons", "Icons");
  static const char* icons[] = {"home", "trophy", "users", "chart", "bat", "ball", "stumps", "play", "pause", "eye",
                                "forward", "skip", "swap", "grip", "dumbbell", "coin", "calendar", "list", "star", "x",
                                "minus", "plus", "edit", "check", "undo", "chevron", "back", "up", "down", "droplet",
                                "target", "shield", "pitch", "flame", "bolt", "field", "whistle"};
  if (beginGrid("##icongrid", columnsFor(width - 36, 110, 10), 6)) {
    for (const char* name : icons) {
      gridNext();
      const ImVec2 p = ImGui::GetCursorScreenPos();
      drawIcon(ImGui::GetWindowDrawList(), name, p.x, p.y, 22, th.ink);
      ImGui::Dummy(ImVec2(24, 24));
      ImGui::SameLine(0, 8);
      ImGui::TextColored(vec(th.muted), "%s", name);
    }
    endGrid();
  }
  endPanel();
}

void GalleryScreen::drawValues(float width) {
  if (beginGrid("##values", width >= 900 ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##meters", "Meters and ratings");
    meter(85.0f, "Confidence 85: In the zone");
    meter(55.0f, "Rhythm 55: In rhythm");
    meter(30.0f, "Fitness 30: Exhausted");
    meter(64.0f, "Compact", true);
    caption("Ratings: 80+ green, 60-79 ink, under 60 muted");
    for (int v : {91, 72, 48}) {
      rating(v);
      ImGui::SameLine(0, 18);
    }
    ImGui::NewLine();
    caption("Progress bar");
    progressBar(0.42f, "Lot 25 of 60");
    caption("Key / value rows");
    keyValue("Purse left", money(4115.0));
    keyValue("Squad", "21 (18-25)");
    endPanel();

    gridNext();
    beginPanel("##tags", "Tags and pills");
    caption("Career phase");
    for (int age : {21, 27, 31, 34, 37}) {
      phaseTag(age);
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    caption("Squad tags");
    osTag();
    ImGui::SameLine(0, 6);
    qTag();
    ImGui::SameLine(0, 6);
    ipTag();
    ImGui::SameLine(0, 6);
    styleChip("RHB");
    ImGui::SameLine(0, 6);
    styleChip("146 kph");
    ImGui::SameLine(0, 6);
    styleChip("Edited", true);
    caption("Tones");
    for (auto [t, label] : {std::pair{TagTone::Default, "Default"}, std::pair{TagTone::Hot, "Hot"}, std::pair{TagTone::Strong, "Strong"},
                            std::pair{TagTone::Weak, "Weak"}, std::pair{TagTone::Muted, "Muted"}}) {
      tag(label, t);
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    caption("Fitness and injury");
    fitnessTag(samplePlayer("A", Role::BAT, 82));
    ImGui::SameLine(0, 10);
    fitnessTag(samplePlayer("B", Role::BAT, 58));
    ImGui::SameLine(0, 10);
    fitnessTag(samplePlayer("C", Role::BAT, 90, true));
    caption("Form pips (recent results)");
    formPips({'W', 'W', 'L', 'N', 'W'});
    caption("Player link");
    playerLink("gallery", "Jasprit Bumra");
    endPanel();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##kpis", "KPI tiles");
  if (beginGrid("##kpigrid", 4, 10)) {
    gridNext();
    kpiTile("Purse left", money(3515.0), TagTone::Hot);
    gridNext();
    kpiTile("Wins", "9", TagTone::Strong);
    gridNext();
    kpiTile("Squad", "16 (18-25)", TagTone::Weak);
    gridNext();
    kpiTile("Overseas", "4/8");
    endGrid();
  }
  endPanel();
}

void GalleryScreen::drawControls(float width) {
  if (beginGrid("##controls", width >= 900 ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##buttons", "Buttons");
    button("Primary", ButtonKind::Primary);
    ImGui::SameLine();
    button("Default");
    ImGui::SameLine();
    button("Quiet", ButtonKind::Quiet);
    ImGui::SameLine();
    button("Small", ButtonKind::Small);
    ImGui::SameLine();
    button("Danger", ButtonKind::Danger);
    ImGui::SameLine();
    button("Disabled", ButtonKind::Default, false);
    caption("Full width");
    buttonFull("Next ball", ButtonKind::Primary);
    caption("Icon button");
    iconButton("edit", "Edit");
    ImGui::SameLine();
    iconButton("swap", "Swap");
    ImGui::SameLine();
    iconButton("x", "Close");
    caption("Modal");
    if (button("Open a modal")) m_modal = true;
    endPanel();

    gridNext();
    beginPanel("##choices", "Choices");
    caption("Chips (pill radio)");
    const char* chips[] = {"Surprise me", "Flat deck", "Green top"};
    for (int i = 0; i < 3; ++i) {
      if (chip(chips[i], m_chip == i)) m_chip = i;
      ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    caption("Segmented control");
    m_segment = segmented({"Slow", "Med", "Fast"}, m_segment);
    caption("Tabs (small)");
    static int small = 0;
    small = tabs({{"a", "This over", ""}, {"b", "Last 5 overs", ""}, {"c", "Innings", ""}}, small, true);
    ImGui::Dummy(ImVec2(0, 4));
    selectBox("##select", "Dropdown", {"Batter", "Bowler", "All-rounder"}, &m_select, "Choose a role", 260);
    caption("Switch and checkbox");
    switchToggle("Night broadcast theme", &m_switchState);
    checkbox("Include retired", &m_check);
    endPanel();
    endGrid();
  }
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##forms", "Forms");
  if (beginGrid("##formgrid", width >= 900 ? 3 : 1, 18)) {
    gridNext();
    textInput("##name", "Text input", &m_text, 24);
    gridNext();
    eyebrowText("Number stepper");
    numberStepper("##years", &m_stepper, 1, 3, 1, (std::to_string(m_stepper) + (m_stepper == 1 ? " year" : " years")).c_str());
    gridNext();
    eyebrowText("Money stepper");
    numberStepperD("##money", &m_money, 30, 2700, 25, money(m_money));
    endGrid();
  }
  sliderInt("##slider", "Slider", &m_slider, 1, 99);
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##notices", "Notices and confirmation");
  notice("Saved. The match goes into the career.", NoticeKind::Ok);
  ImGui::Dummy(ImVec2(0, 4));
  notice("Two-ball rule: dew expected in the chase.", NoticeKind::Warn);
  ImGui::Dummy(ImVec2(0, 4));
  notice("The squad is full (25 players).", NoticeKind::Error);
  ImGui::Dummy(ImVec2(0, 4));
  confirmInline("Release him into the auction?", "Release", "Keep him");
  endPanel();

  if (m_modal && beginModal("##gallery_modal", &m_modal, "A modal", 520)) {
    ImGui::TextWrapped("Centred over a dimmed backdrop; pops in; closes with the cross or Esc.");
    if (button("Close", ButtonKind::Primary)) m_modal = false;
    endModal();
  }
}

void GalleryScreen::drawTables(float width) {
  (void)width;
  beginPanel("##setupstep", "Setup stepper");
  stepper(Screen::training);
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##table", "Sortable table");
  if (!m_table.initialised) m_table = {2, false, true, true};
  std::vector<TableColumn> cols = {textColumn("Player", 2.2f), textColumn("Team", 1.0f), numColumn("Runs"),
                                   numColumn("SR", "Strike rate", false, "Strike rate: min 30 balls"), numColumn("Econ", "Economy", true)};
  const struct {
    const char* name;
    const char* team;
    int runs;
    double sr, econ;
  } data[] = {{"Suryakumar Yadev", "MUI", 512, 168.2, 0}, {"Rahul Tripathy", "SNH", 488, 151.4, 0}, {"Jasprit Bumra", "MUI", 41, 102.5, 6.8},
              {"Rashid Kahn", "SNH", 120, 140.0, 6.1}, {"Hardik Pandia", "MUI", 301, 149.0, 8.9}, {"Andre Russel", "KNR", 340, 177.1, 9.6}};
  std::vector<TableRowData> rows;
  for (const auto& d : data) {
    TableRowData r;
    r.mine = std::string(d.team) == "MUI";
    TableCell team = textCell(d.team, CellTone::Muted);
    team.badgeTeam = d.team;
    TableCell runs;
    runs.text = std::to_string(d.runs);
    runs.sort = d.runs;
    TableCell sr;
    sr.text = fmt(d.sr, 1);
    sr.sort = d.sr;
    TableCell econ = d.econ > 0 ? TableCell{} : dashCell();
    if (d.econ > 0) {
      econ.text = fmt(d.econ, 2);
      econ.sort = d.econ;
    }
    r.cells = {textCell(d.name), team, runs, sr, econ};
    rows.push_back(r);
  }
  sortableTable("##demo", cols, rows, m_table, 4, true, "Your rows are tinted gold");
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));
  beginPanel("##folds", "Fold row, collapsed panel, pick row");
  if (foldRow("##fold", "Field", "Auto (balanced) · 2 on the boundary")) {
    ImGui::TextUnformatted("The body of a fold row.");
    endFoldRow();
  }
  ImGui::Dummy(ImVec2(0, 4));
  if (collapsedPanel("Commentary", "4.3  Bumra to Sharma, FOUR!", &m_fold)) {
    beginPanel("##opened", "Commentary");
    ImGui::TextUnformatted("The panel, opened.");
    if (button("Hide", ButtonKind::Small)) m_fold = false;
    endPanel();
  }
  ImGui::Dummy(ImVec2(0, 4));
  beginPickRow("##pick");
  avatar(samplePlayer("Jasprit Bumra", Role::BOWL, 100), kTeams[1].color, 34);
  ImGui::SameLine(0, 10);
  ImGui::TextUnformatted("Jasprit Bumra · 2 overs left");
  if (endPickRow()) triggerMomentBanner({MomentKind::Wicket, "Picked", "JASPRIT BUMRA"});
  endPanel();
  beginStickyActions();
  button("Back");
  ImGui::SameLine();
  rightAlign(buttonWidth("Continue", ButtonKind::Primary));
  button("Continue", ButtonKind::Primary);
  endStickyActions();
}

void GalleryScreen::drawMatch(float width) {
  if (!m_demo) {
    m_demo = std::make_unique<app::MatchSession>();
    m_demo->newMatch(1, 2, 2026);
    m_demo->simMatch();
  }
  const Match& m = m_demo->match();
  beginPanel("##board", "Scoreboard, moment banner, win bar, fans");
  ScoreboardViewData sb;
  sb.battingTeam = kTeams[0];
  sb.bowlingTeam = kTeams[1];
  sb.runs = 178;
  sb.wickets = 3;
  sb.balls = 112;
  sb.headline = "Need 12 from 8 balls";
  sb.stats = {{"Run rate", "9.54"}, {"Target", "190"}, {"Req. rate", "9.00", true}};
  sb.overLabel = "This over";
  for (const char* t : {"1", "4", "W", "6", "\xe2\x80\xa2"}) {
    BallPipData pip;
    pip.text = t;
    pip.runs = t[0] >= '0' && t[0] <= '9' ? t[0] - '0' : 0;
    pip.isFour = pip.runs == 4;
    pip.isSix = pip.runs == 6;
    pip.isWicket = t[0] == 'W';
    sb.currentOverPips.push_back(pip);
  }
  sb.overRuns = 11;
  sb.tags = {"Death overs", "Dew settling"};
  sb.battingWinPct = 64.0f;
  sb.fansHome = FansSideData{kTeams[0], 21450, 1};
  sb.fansAway = FansSideData{kTeams[1], 6120, -1};
  sb.groundCapacity = 33000;
  sb.conditions = "Flat deck · pitch wear 62%";
  const ImVec2 o = ImGui::GetCursorScreenPos();
  drawScoreboard(nullptr, sb, ImGui::GetContentRegionAvail().x);
  drawMomentBanner(ImGui::GetWindowDrawList(), o.x + ImGui::GetContentRegionAvail().x - 30, o.y + 22);
  ImGui::Dummy(ImVec2(0, 6));
  for (auto [kind, label] : {std::pair{MomentKind::Four, "Four"}, std::pair{MomentKind::Six, "Six!"}, std::pair{MomentKind::Wicket, "Wicket"},
                             std::pair{MomentKind::Milestone, "Fifty"}, std::pair{MomentKind::Result, "Result"}}) {
    if (button((std::string("Show ") + label).c_str(), ButtonKind::Small)) triggerMomentBanner({kind, label, "SURYAKUMAR YADEV"});
    ImGui::SameLine(0, 6);
  }
  ImGui::NewLine();
  ImGui::Dummy(ImVec2(0, 6));
  winBar(64.0f, "CHK", "MUI");
  endPanel();
  ImGui::Dummy(ImVec2(0, 8));

  const bool two = width >= 900;
  if (beginGrid("##matchgfx", two ? 2 : 1, 18)) {
    gridNext();
    beginPanel("##fieldgfx", "Field view");
    static FieldViewState fv;
    FieldViewData data;
    auto shot = [](float dir, float dist, int runs) {
      FieldShot s;
      s.dir = dir;
      s.dist = dist;
      s.runs = runs;
      s.isFour = runs == 4;
      s.isSix = runs == 6;
      s.isWicket = runs < 0;
      return s;
    };
    data.shots = {shot(-30, 1.0f, 4), shot(40, 1.0f, 6), shot(10, 0.6f, 1), shot(-70, 0.0f, -1), shot(75, 0.8f, 2)};
    data.shots.back().saved = 4;
    data.shots.back().fielderDir = 80;
    data.shots.back().fielderDist = 0.95f;
    data.shots.back().tip = "A four cut off: 2 runs";
    data.fielders = {{80, 0.95f, "Deep point"}, {-50, 0.95f, "Deep midwicket"}, {20, 0.45f, "Mid-off"}, {-20, 0.45f, "Mid-on"}};
    fieldViewCanvas(data, std::min(ImGui::GetContentRegionAvail().x, 420.0f));
    (void)fv;
    endPanel();
    gridNext();
    beginPanel("##charts", "Charts (a simmed match)");
    const float w = ImGui::GetContentRegionAvail().x;
    drawWorm(m, w, 220);
    ImGui::Dummy(ImVec2(0, 6));
    drawManhattan(m, w, 180);
    ImGui::Dummy(ImVec2(0, 6));
    drawWinChart(m, m_demo->winPoints(), w, 160);
    endPanel();
    endGrid();
  }
}

}  // namespace t20::ui
