#include "ui/match/captain_panel.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "core/str.hpp"
#include "engine/ai.hpp"
#include "engine/battle.hpp"
#include "engine/commentary.hpp"
#include "engine/field.hpp"
#include "engine/league.hpp"
#include "engine/mindset.hpp"
#include "engine/rules.hpp"
#include "engine/skills.hpp"
#include "engine/tactics.hpp"
#include "ui/format.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/avatar.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/panel.hpp"
#include "ui/widgets/tags.hpp"

namespace t20::ui {

namespace {

const Player& P(const Match& m, const PlayerId& id) { return m.players.at(id); }

PlayerMeta metaOf(const Player& p) {
  PlayerMeta pm;
  pm.id = p.id;
  pm.name = p.name;
  pm.role = p.role;
  return pm;
}

std::string bowlShort(const Player& p) { return p.bowlType ? std::string(bowlTypeDef(*p.bowlType).shortName) : ""; }

int rounded(double v) { return static_cast<int>(std::lround(v)); }

void hint(const std::string& text) {
  const auto& th = currentTheme();
  FontScope fs(FontRole::Hint);
  ImGui::PushTextWrapPos(0);
  ImGui::TextColored(vec(th.muted), "%s", text.c_str());
  ImGui::PopTextWrapPos();
}

void heading(const std::string& text) {
  FontScope fs(FontRole::H3);
  ImGui::TextUnformatted(text.c_str());
}

// "v Surname: 14 off 9, out once" + "Bowler on top" (green when it favours the user's side)
void h2hLine(const app::MatchSession& s, const std::string& label, const PlayerId& bat, const PlayerId& bowl,
             std::string_view side) {
  const auto& th = currentTheme();
  {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.ink2), "%s: %s", label.c_str(), h2hText(s.headToHead(bat, bowl)).c_str());
  }
  const std::string_view v = s.edgeVerdictOf(bat, bowl);
  if (!v.empty()) {
    ImGui::SameLine(0, 6);
    tag(v == "bowler" ? "Bowler on top" : "Batter on top", v == side ? TagTone::Strong : TagTone::Weak);
  }
}

// Ratings drawn in a column at the right edge of a pick row (the JS "pick-side")
void ratingAt(ImDrawList* dl, float rightX, float y, const char* label, double value) {
  const auto& th = currentTheme();
  const int v = rounded(value);
  const ColorRGBA& c = v >= 80 ? th.green : v >= 60 ? th.ink : th.muted;
  const std::string num = std::to_string(v);
  const ImVec2 ls = textSize(FontRole::BoardLabel, label), ns = textSize(FontRole::H2, num);
  drawText(dl, FontRole::BoardLabel, ImVec2(rightX - ls.x, y), col(th.muted), label);
  drawText(dl, FontRole::H2, ImVec2(rightX - ns.x, y + ls.y + 1), col(c), num);
}

// One row with an avatar on the left; `body` draws the text column
template <class F>
bool pickRow(const char* id, bool enabled, const Player& p, const std::string& teamColor, F&& body) {
  beginPickRow(id, enabled);
  avatar(metaOf(p), teamColor, 34.0f);
  ImGui::SameLine(0, 12);
  ImGui::BeginGroup();
  body();
  ImGui::EndGroup();
  return endPickRow();
}

void markDecision(CaptainPanelState& st) {
  // the dock's "jump to decision": bring this block into view (scrolls the page)
  if (st.scrollToDecision) {
    ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
    st.scrollToDecision = false;
  }
}

// SameLine, or a new line when an item `width` wide would cross the content's right edge
void sameLineOrWrap(float width, float spacing = 6.0f) {
  const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  ImGui::SameLine(0, spacing);
  if (ImGui::GetCursorScreenPos().x + width > right) ImGui::NewLine();
}

float tagWidth(const std::string& text) { return textSize(FontRole::Eyebrow, upper(text)).x + 18.0f; }

// --- 5.1 Review call ---------------------------------------------------------------------

void reviewCall(app::MatchSession& s, CaptainPanelState& st) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  const Umpire& u = *m.pendingBall->umpire;
  const Player& striker = P(m, *inn.striker);
  const Player& bowler = P(m, *inn.currentBowler);
  const Player& keeper = P(m, m.teams[static_cast<size_t>(inn.bowlingSide)].keeperId);
  const std::string what = u.type == Out::lbw ? "lbw" : "caught behind";
  const bool batting = u.reviewer == "bat";
  const int left = batting ? inn.reviewsLeft.bat : inn.reviewsLeft.bowl;
  const BatterInnings& b = inn.batters.at(*inn.striker);
  const std::string bat = surname(striker.name), bowl = surname(bowler.name), kp = surname(keeper.name);
  const std::string read =
      batting ? (u.sure ? cat(bat, " is adamant: \"", u.type == Out::lbw ? "That was going down leg" : "Never touched it", "!\"")
                        : cat(bat, " isn't sure. He has a word with his partner and shrugs."))
              : (u.sure ? cat(bowl, " and ", kp, " are convinced: \"", u.type == Out::lbw ? "That was hitting middle" : "He nicked it", "!\"")
                        : cat(kp, " isn't sure, and ", bowl, " only half appealed."));

  heading(batting ? cat(striker.name, " is given out ", what, "!") : cat("Not out! The umpire turns down the ", what, " appeal"));
  markDecision(st);
  ImGui::SameLine(0, 10);
  tag(cat(left, " review", left == 1 ? "" : "s", " left"), TagTone::Muted);
  ImGui::PushTextWrapPos(0);
  ImGui::TextColored(vec(th.ink), "%s", cat(bat, " is on ", b.runs, " (", b.balls, "). ", read).c_str());
  ImGui::PopTextWrapPos();
  hint("Players are usually right when they're sure, but not always. A review that overturns the decision is kept; one that fails is lost.");
  if (button("Review  (R)", ButtonKind::Primary)) s.reviewCall(true);
  ImGui::SameLine();
  if (button(batting ? "Walk off  (A)" : "Accept, play on  (A)")) s.reviewCall(false);
  bool autoReview = m.settings.autoReview;
  if (switchToggle("Let my vice-captain make review calls", &autoReview)) s.setAutoReview(autoReview);
}

// --- 5.2 Bowler picker --------------------------------------------------------------------

void bowlerPicker(app::MatchSession& s, CaptainPanelState& st) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  std::vector<PlayerId> atCrease;
  for (const auto& id : {inn.striker, inn.nonStriker})
    if (id) atCrease.push_back(*id);
  std::vector<BowlOption> opts = bowlingOptions(m);
  std::stable_sort(opts.begin(), opts.end(), [&](const BowlOption& a, const BowlOption& b) {
    if (a.ok != b.ok) return a.ok;
    return P(m, a.id).bowl > P(m, b.id).bowl;
  });
  const int quota = bowlerQuota(&inn);
  const int over = inn.legalBalls / 6 + 1;
  const BallContext ctx{over - 1, phaseAt(&inn, over - 1), m.pitch, matchWear(m), dewActive(m, over - 1)};
  const std::string color = m.teams[static_cast<size_t>(inn.bowlingSide)].color;

  heading(cat("Who bowls over ", over, "?"));
  markDecision(st);
  ImGui::SameLine(0, 16);
  bool autoBowl = m.settings.autoBowl;
  if (switchToggle("Pick bowlers for me", &autoBowl)) s.setAutoBowl(autoBowl);

  for (const BowlOption& o : opts) {
    const Player& p = P(m, o.id);
    const BowlingFigures* f = inn.bowling.find(o.id);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowRight = rowStart.x + ImGui::GetContentRegionAvail().x - 12;
    const bool clicked = pickRow(o.id.c_str(), o.ok, p, color, [&] {
      ImGui::PushTextWrapPos(rowRight - 90 - ImGui::GetCursorScreenPos().x + ImGui::GetCursorPosX());
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::TextUnformatted(p.name.c_str());
      }
      std::string meta = bowlShort(p);
      const auto kph = speedKph(p);
      if (kph && p.bowl >= 35) meta += cat(" · ", js_num(*kph), " kph");
      if (p.bowlStyle) meta += " · " + *p.bowlStyle;
      meta += f ? cat(" · ", oversText(f->balls), "-", f->maidens, "-", f->runs, "-", f->wickets) : std::string(" · yet to bowl");
      if (meta.rfind(" · ", 0) == 0) meta.erase(0, std::string(" · ").size());  // no bowling type
      {
        FontScope fs(FontRole::Hint);
        ImGui::TextColored(vec(th.muted), "%s", meta.c_str());
      }
      if (o.ok) {
        const Edge e = specialistEdge(p, ctx);
        const char* suits = e.wicket >= 1.08 ? "Suits these conditions" : e.boundary <= 0.93 ? "Hard to hit now" : nullptr;
        if (suits) {
          ImGui::SameLine(0, 8);
          FontScope fs(FontRole::Hint);
          ImGui::TextColored(vec(th.green), "%s", suits);
        }
      }
      if (p.bowl >= 35)
        for (size_t i = 0; i < atCrease.size(); ++i) {
          if (i) ImGui::SameLine(0, 16);  // one "h2h-row"
          h2hLine(s, "v " + surname(P(m, atCrease[i]).name), atCrease[i], o.id, "bowler");
        }
      ImGui::PopTextWrapPos();
    });
    // right side: overs left (or why not) and the bowling rating
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string side = o.ok ? cat(quota - o.bowled, " left") : o.reason;
    const ImVec2 ss = textSize(FontRole::Hint, side);
    drawText(dl, FontRole::Hint, ImVec2(rowRight - 52 - ss.x, rowStart.y + 22), col(o.ok ? th.ink2 : th.red), side);
    ratingAt(dl, rowRight, rowStart.y + 10, "BOWL", p.bowl);
    if (clicked) s.pickBowler(o.id);
  }
}

// --- 5.3 Batter picker --------------------------------------------------------------------

void batterPicker(app::MatchSession& s, CaptainPanelState& st) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  const Player* bowler = inn.currentBowler ? &P(m, *inn.currentBowler) : nullptr;
  const NextBatterCall call = nextBatterCall(m);  // what the vice-captain would do
  const std::string color = m.teams[static_cast<size_t>(inn.batting)].color;

  heading("Wicket! Who goes in next?");
  markDecision(st);
  if (!call.reason.empty()) hint(cat("Vice-captain: send in ", P(m, call.id).name, ". ", call.reason, "."));

  for (size_t i = 0; i < inn.yetToBat.size(); ++i) {
    const PlayerId& id = inn.yetToBat[i];
    const Player& p = P(m, id);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowRight = rowStart.x + ImGui::GetContentRegionAvail().x - 12;
    const bool clicked = pickRow(id.c_str(), true, p, color, [&] {
      {
        FontScope fs(FontRole::BodyStrong);
        ImGui::TextUnformatted(p.name.c_str());
      }
      if (!call.reason.empty() && id == call.id) {
        ImGui::SameLine(0, 8);
        tag("Suggested", TagTone::Strong);
      }
      const std::string meta = i == 0 ? p.batStyle + " · next in your order"
                                      : cat(p.batStyle, " · no. ", inn.battedOrder.size() + i + 1, " in your order");
      {
        FontScope fs(FontRole::Hint);
        ImGui::TextColored(vec(th.muted), "%s", meta.c_str());
      }
      if (bowler) h2hLine(s, "v " + surname(bowler->name), id, bowler->id, "batter");
    });
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ratingAt(dl, rowRight, rowStart.y + 10, "POWER", p.power);
    ratingAt(dl, rowRight - 62, rowStart.y + 10, "BAT", p.bat);
    if (clicked) s.pickBatter(id);
  }
}

// --- 5.4 Batting controls -----------------------------------------------------------------

const char* tempoHelp(const Tempo& t) {
  if (!t) return "Your batters read the match situation themselves.";
  switch (rounded(*t)) {
    case 1: return "Survive. Few boundaries, very few wickets.";
    case 2: return "Rotate strike and pick off bad balls.";
    case 3: return "Play on merit.";
    case 4: return "Look for the boundary. Wickets get more likely.";
    default: return "Swing at everything. Big overs or a collapse.";
  }
}

void batterTempo(app::MatchSession& s, const PlayerId& id) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  const Player& p = P(m, id);
  const Tempo current = batterTempoOf(m, id);
  {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.ink2), "%s", upper(p.name + (inn.striker && *inn.striker == id ? " · on strike" : "")).c_str());
  }
  std::vector<std::string> labels{"Auto"};
  for (const auto& a : AGGRESSION) labels.emplace_back(a.label);
  const int idx = current ? rounded(*current) : 0;
  ImGui::PushID(id.c_str());
  const int next = segmented(labels, idx);
  ImGui::PopID();
  if (next != idx) s.setBatterTempo(id, next == 0 ? Tempo{} : Tempo{static_cast<double>(AGGRESSION[static_cast<size_t>(next - 1)].level)});
  hint(tempoHelp(current));
  if (inn.currentBowler) h2hLine(s, "v " + surname(P(m, *inn.currentBowler).name), id, *inn.currentBowler, "batter");
  ImGui::Dummy(ImVec2(0, 4));
}

void batterTarget(app::MatchSession& s) {
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  if (!inn.currentBowler || inn.superOver) return;
  const Player& bowler = P(m, *inn.currentBowler);
  std::optional<TargetMode> mode;
  if (auto it = m.settings.targets.find(bowler.id); it != m.settings.targets.end()) mode = it->second;
  const int idx = !mode ? 0 : *mode == TargetMode::attack ? 1 : 2;
  static const char* labels[] = {"Normal", "Target him", "See him off"};
  if (!foldRow("target", ("Both batters v " + surname(bowler.name)).c_str(), labels[idx])) return;
  const int next = segmented({labels[0], labels[1], labels[2]}, idx);
  if (next != idx)
    s.setBatterTarget(bowler.id, next == 0 ? std::optional<TargetMode>{} : next == 1 ? TargetMode::attack : TargetMode::block);
  const BowlingFigures* f = inn.bowling.find(bowler.id);
  const std::string lastPlan = inn.lastPlan && inn.lastPlan->bowler == bowler.id
                                   ? lower_ascii(std::string(planByKey(bowler, inn.lastPlan->plan).label))
                                   : "unknown";
  hint(cat(mode == TargetMode::attack  ? "Your batters take him on: more boundaries, more risk."
           : mode == TargetMode::block ? "Play him out and score off the others."
                                       : "Play him on merit.",
           " He's ", f ? cat(f->wickets, "-", f->runs, " off ", oversText(f->balls)) : std::string("yet to bowl"), "; rated ",
           js_num(bowler.bowl), ". Their plan last ball: ", lastPlan, "."));
  endFoldRow();
}

// --- 5.5 Bowling controls -----------------------------------------------------------------

void bowlingPlan(app::MatchSession& s) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  if (!inn.currentBowler || !inn.striker || inn.superOver) return;
  const Player& bowler = P(m, *inn.currentBowler);
  const Player& striker = P(m, *inn.striker);
  const auto& plans = plansFor(bowler);
  // No entry = the vice-captain's plan (the engine reads a missing plan as auto)
  std::optional<Plan> current;
  if (auto it = inn.plans.find(bowler.id); it != inn.plans.end()) current = it->second;
  const std::vector<Weak> weak = weaknesses(striker);

  {
    FontScope fs(FontRole::Eyebrow);
    ImGui::TextColored(vec(th.ink2), "%s", upper("Plan for " + bowler.name).c_str());
  }
  // Scouting line
  {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "Scouting: %s", striker.name.c_str());
  }
  // "Weak against [tags] · [strength tags]" on one line, as in the JS
  ImGui::SameLine(0, 8);
  if (weak.empty()) {
    FontScope fs(FontRole::Hint);
    ImGui::TextColored(vec(th.muted), "No obvious weakness.");
  } else {
    {
      FontScope fs(FontRole::Hint);
      ImGui::TextColored(vec(th.muted), "Weak against");
    }
    for (Weak w : weak) {
      const std::string t(weakLabel(w));
      sameLineOrWrap(tagWidth(t), 4);
      tag(t, TagTone::Weak);
    }
  }
  for (Strength x : strengths(striker)) {
    const std::string t(strengthLabel(x));
    sameLineOrWrap(tagWidth(t));
    tag(t, TagTone::Strong);
  }
  bool firstH2h = true;
  for (const auto& id : {inn.striker, inn.nonStriker}) {
    if (!id) continue;
    if (!firstH2h) ImGui::SameLine(0, 16);
    firstH2h = false;
    h2hLine(s, surname(bowler.name) + " v " + surname(P(m, *id).name), *id, bowler.id, "bowler");
  }

  // Plan chips (a dot marks a plan that hits the striker's weakness)
  const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  auto chipFlow = [&](const char* label, bool selected, bool hits, bool first) {
    const float w = textSize(FontRole::BodyStrong, label).x + 28 + (hits ? 10 : 0);
    if (!first) {
      ImGui::SameLine(0, 6);
      if (ImGui::GetCursorScreenPos().x + w > right) ImGui::NewLine();
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool c = chip(label, selected);
    if (hits) ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(ImGui::GetItemRectMax().x - 9, p.y + 9), 3.5f, col(th.red));
    return c;
  };
  if (chipFlow("Auto", !current, false, true)) s.setBowlingPlan(std::nullopt);
  for (const PlanDef& pl : plans) {
    const bool hits = pl.exploits && std::find(weak.begin(), weak.end(), *pl.exploits) != weak.end();
    if (chipFlow(std::string(pl.label).c_str(), current && *current == pl.key, hits, false)) s.setBowlingPlan(pl.key);
  }

  std::string help;
  const PlanDef* def = current ? &planByKey(bowler, *current) : nullptr;
  if (!def) {
    help = "Your vice-captain picks the plan ball by ball.";
  } else {
    help = std::string(def->help);
    if (def->skill)
      help += cat(" Depends on his ", *def->skill == BowlK::death ? std::string("death bowling") : std::string(name(*def->skill)), " (",
                  js_num(bowler.skills.bowl[*def->skill]), ").");
  }
  if (inn.lastPlan && inn.lastPlan->bowler == bowler.id && inn.lastPlan->fits) help += " Last ball hit his weakness.";
  hint(help);

  // Predictability: the striker has been seeing the same plan
  if (def && *current != Plan::stock) {
    const auto it = inn.planStreak.find(striker.id);
    const int seen = it != inn.planStreak.end() && it->second.plan == *current ? it->second.n : 0;
    if (seen >= READ_AFTER - 1) {
      const std::string text = cat(surname(striker.name), " has had ", seen, " balls of ", lower_ascii(std::string(def->label)),
                                   " in a row. ",
                                   seen > READ_AFTER ? "He's reading it now: the edge is gone and he's lining it up. Mix it up."
                                                     : "Keep going and he'll start waiting for it.");
      notice(text, seen > READ_AFTER ? NoticeKind::Error : NoticeKind::Warn);
    }
  }
  ImGui::Dummy(ImVec2(0, 4));
}

void fieldControl(app::MatchSession& s, CaptainPanelState& st) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const Innings& inn = inningsOf(m);
  if (!inn.currentBowler || inn.superOver) return;
  const Phase phase = phaseAt(&inn, inn.legalBalls / 6);
  const FieldPreset current = inn.userField ? inn.userField->preset : FieldPreset::auto_;
  std::vector<Pos> positions;
  if (inn.userField && inn.userField->positions) positions = *inn.userField->positions;
  else if (current == FieldPreset::auto_ && inn.fieldNow) positions = inn.fieldNow->positions;
  else if (current != FieldPreset::auto_ && current != FieldPreset::custom) positions = presetByKey(current).positions;
  else positions = presetByKey(FieldPreset::balanced).positions;
  const bool pulledIn = current != FieldPreset::auto_ && !fieldProblems(positions, phase).empty();
  const FieldShape d = describeField(positions);
  const FieldPreset nowPreset = inn.fieldNow ? inn.fieldNow->preset : FieldPreset::balanced;
  const std::string label = current == FieldPreset::auto_  ? cat("Auto (", lower_ascii(std::string(presetByKey(nowPreset).label)), ")")
                            : current == FieldPreset::custom ? std::string("Custom")
                                                             : std::string(presetByKey(current).label);
  if (!foldRow("field", "Field", cat(label, " · ", d.deep, " on the boundary").c_str())) return;

  const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  auto flow = [&](bool first, const char* text) {
    if (first) return;
    ImGui::SameLine(0, 6);
    if (ImGui::GetCursorScreenPos().x + textSize(FontRole::BodyStrong, text).x + 28 > right) ImGui::NewLine();
  };
  flow(true, "Auto");
  if (chip("Auto##field", current == FieldPreset::auto_)) {
    s.setField(FieldPreset::auto_);
    st.editingField = false;
  }
  for (const PresetDef& pr : PRESETS) {
    const std::string l(pr.label);
    flow(false, l.c_str());
    if (chip((l + "##field").c_str(), current == pr.key)) {
      s.setField(pr.key);
      st.editingField = false;
    }
  }
  flow(false, "Customise");
  if (chip("Customise", current == FieldPreset::custom || st.editingField)) {
    if (st.editingField) {
      st.editingField = false;
    } else {
      st.fieldDraft = positions;
      st.editingField = true;
    }
  }
  const std::string help = current == FieldPreset::auto_    ? "Your vice-captain sets the field for each ball."
                           : current == FieldPreset::custom ? "Your own field."
                                                            : std::string(presetByKey(current).help);
  hint(cat(help, " ", d.close, " close, ", d.ring, " in the ring, ", d.deep, " on the boundary; ", d.off, " on the off side.",
           pulledIn ? cat(" Powerplay: only ", phase == Phase::pp ? 2 : 5, " may be outside the circle, so some are brought in.")
                    : std::string()));

  if (st.editingField) {
    // Custom editor: toggle up to 9 of the 23 positions, zone by zone
    static const std::pair<Zone, const char*> zones[] = {
        {Zone::close, "Close catchers"}, {Zone::ring, "Inside the circle"}, {Zone::deep, "Boundary"}};
    for (const auto& [zone, zoneLabel] : zones) {
      {
        FontScope fs(FontRole::Eyebrow);
        ImGui::TextColored(vec(th.muted), "%s", upper(zoneLabel).c_str());
      }
      bool first = true;
      for (size_t i = 0; i < kPosCount; ++i) {
        const Pos k = static_cast<Pos>(i);
        if (position(k).zone != zone) continue;
        const std::string l(position(k).label);
        flow(first, l.c_str());
        first = false;
        auto it = std::find(st.fieldDraft.begin(), st.fieldDraft.end(), k);
        if (chip((l + "##pos").c_str(), it != st.fieldDraft.end())) {
          if (it != st.fieldDraft.end()) st.fieldDraft.erase(it);
          else if (st.fieldDraft.size() < 9) st.fieldDraft.push_back(k);
        }
      }
    }
    const auto problems = fieldProblems(st.fieldDraft, phase);
    std::string ptxt = cat(st.fieldDraft.size(), "/9 placed · ", describeField(st.fieldDraft).deep, " outside the circle.");
    for (const auto& pr : problems) ptxt += " " + pr;
    {
      FontScope fs(FontRole::Hint);
      ImGui::PushTextWrapPos(0);
      ImGui::TextColored(vec(problems.empty() ? th.muted : th.red), "%s", ptxt.c_str());
      ImGui::PopTextWrapPos();
    }
    if (button("Set this field", ButtonKind::Primary, problems.empty())) {
      s.setField(FieldPreset::custom, st.fieldDraft);
      st.editingField = false;
    }
    ImGui::SameLine();
    if (button("Cancel", ButtonKind::Quiet)) st.editingField = false;
  }
  endFoldRow();
}

}  // namespace

const char* pendingDecisionLabel(const Match& m) {
  if (m.status != MatchStatus::live) return nullptr;
  switch (m.awaiting) {
    case Awaiting::bowler: return "Pick your bowler";
    case Awaiting::batter: return "Send in your batter";
    case Awaiting::review: return "Review it?";
    default: return nullptr;
  }
}

void drawCaptainPanel(app::MatchSession& s, CaptainPanelState& st, const std::function<void()>& onNewFixture) {
  const auto& th = currentTheme();
  const Match& m = s.match();

  if (m.status == MatchStatus::complete) {
    beginPanel("##captain", "Result");
    {
      FontScope fs(FontRole::H2);
      ImGui::TextColored(vec(th.goldText), "%s", m.result ? m.result->text.c_str() : "Match over");
    }
    if (m.potm) ImGui::TextColored(vec(th.ink2), "Player of the match: %s", P(m, *m.potm).name.c_str());
    if (button("Rematch", ButtonKind::Primary)) s.rematch();
    ImGui::SameLine();
    if (button("New fixture", ButtonKind::Quiet) && onNewFixture) onNewFixture();
    endPanel();
    return;
  }

  if (m.status == MatchStatus::break_) {
    beginPanel("##captain", "Innings break");
    const Innings& first = m.innings[0];
    const MatchTeam& chasing = m.teams[static_cast<size_t>(1 - first.batting)];
    const bool userChasing = m.userTeam && *m.userTeam == 1 - first.batting;
    // Same target and overs the chase will start with: DLS-revised if rain cut the first innings
    const bool dls = m.dls && m.dls->target;
    const int target = dls ? *m.dls->target : first.runs + 1;
    const int overs = m.oversPerSide;
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(cat(m.teams[static_cast<size_t>(first.batting)].name, " posted ", first.runs, "/", first.wickets, ". ",
                               chasing.name, " need ", target, " to win",
                               dls ? cat(" from ", overs, " overs (DLS)") : overs != 20 ? cat(" from ", overs, " overs") : std::string(),
                               ", a required rate of ", js_fixed(static_cast<double>(target) / overs, 2), ".")
                               .c_str());
    ImGui::PopTextWrapPos();
    if (button(userChasing ? "Start the chase" : "Take the field", ButtonKind::Primary)) s.startChase();
    ImGui::SameLine();
    if (button("Sim to result", ButtonKind::Quiet)) s.simMatch();
    endPanel();
    return;
  }

  const bool batting = userIsBatting(m), bowling = userIsBowling(m);
  const Innings& inn = inningsOf(m);
  beginPanel("##captain", "Your call");
  if (batting || bowling) {
    tag(batting ? "You're batting" : "You're in the field", TagTone::Hot);
    if (!inn.superOver) {
      const int left = batting ? inn.reviewsLeft.bat : inn.reviewsLeft.bowl;
      ImGui::SameLine(0, 10);
      FontScope fs(FontRole::Hint);
      ImGui::TextColored(vec(th.muted), "DRS: %d review%s left", left, left == 1 ? "" : "s");
    }
  }
  ImGui::Dummy(ImVec2(0, 2));

  if (m.awaiting == Awaiting::review && m.pendingBall && m.pendingBall->umpire) reviewCall(s, st);
  if (m.awaiting == Awaiting::bowler) bowlerPicker(s, st);
  if (m.awaiting == Awaiting::batter) batterPicker(s, st);

  if (m.awaiting == Awaiting::none && batting) {
    for (const auto& id : {inn.striker, inn.nonStriker})
      if (id) batterTempo(s, *id);
    hint("Each batter plays to his own instruction and adjusts it to the match situation; the better his game awareness, the "
         "smarter the adjustment. History counts: a bowler who is on top of a batter is more likely to get him again.");
    batterTarget(s);
  }
  if (m.awaiting == Awaiting::none && bowling) {
    bowlingPlan(s);
    fieldControl(s, st);
  }
  if (m.awaiting == Awaiting::none && (batting || bowling) && m.userTeam) {
    const CaptainStyleDef& cs = captainStyle(m.teams[static_cast<size_t>(1 - *m.userTeam)].captain);
    FontScope fs(FontRole::Hint);
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(vec(th.muted), "Their captain: %s. %s", std::string(cs.label).c_str(), std::string(cs.note).c_str());
    ImGui::PopTextWrapPos();
  }
  if (m.awaiting == Awaiting::none && bowling && canChangeBowler(m)) {
    ImGui::TextColored(vec(th.ink), "%s is marking out his run-up for over %d.", P(m, *inn.currentBowler).name.c_str(),
                       inn.legalBalls / 6 + 1);
    ImGui::SameLine();
    if (button("Change bowler", ButtonKind::Small)) s.changeBowler();
  }
  if (m.awaiting == Awaiting::none && bowling) {
    bool autoBowl = m.settings.autoBowl;
    if (switchToggle("Pick bowlers for me##live", &autoBowl)) s.setAutoBowl(autoBowl);
  }
  ImGui::Dummy(ImVec2(0, 2));
  if (button("Sim innings", ButtonKind::Quiet)) s.simInnings();
  ImGui::SameLine();
  if (button("Sim match", ButtonKind::Quiet)) s.simMatch();
  endPanel();
}

}  // namespace t20::ui
