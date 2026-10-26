#include "ui/match/impact_panel.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/str.hpp"
#include "engine/league.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"
#include "ui/widgets/controls.hpp"
#include "ui/widgets/panel.hpp"

namespace t20::ui {

namespace {

const Player& P(const Match& m, const PlayerId& id) { return m.players.at(id); }

// "Name · bat 78 · bowl 64 · OS" (ImpactPanel.jsx `short`)
std::string shortLine(const Player& p) {
  return cat(p.name, " · bat ", js_num(p.bat), p.bowl >= 35 ? cat(" · bowl ", js_num(p.bowl)) : std::string(),
             p.overseas ? " · OS" : "");
}

void hint(const std::string& text) {
  const auto& th = currentTheme();
  FontScope fs(FontRole::Hint);
  ImGui::PushTextWrapPos(0);
  ImGui::TextColored(vec(th.muted), "%s", text.c_str());
  ImGui::PopTextWrapPos();
}

// Inline text link (the JS .plink): gold text, underlined on hover
bool textLink(const char* text) {
  const auto& th = currentTheme();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImVec2 ts = textSize(FontRole::Hint, text);
  ImGui::PushID(text);
  const bool clicked = ImGui::InvisibleButton("##link", ts);
  ImGui::PopID();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  drawText(dl, FontRole::Hint, p, col(th.goldText), text);
  if (hovered) {
    dl->AddLine(ImVec2(p.x, p.y + ts.y), ImVec2(p.x + ts.x, p.y + ts.y), col(th.goldText));
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  return clicked;
}

void eyebrow(const char* text) {
  FontScope fs(FontRole::Eyebrow);
  ImGui::TextColored(vec(currentTheme().goldText), "%s", upper(text).c_str());
}

}  // namespace

void drawImpactPanel(app::MatchSession& s, ImpactPanelState& st) {
  const auto& th = currentTheme();
  const Match& m = s.match();
  const int side = m.userTeam ? *m.userTeam : 0;
  const MatchTeam& team = m.teams[static_cast<size_t>(side)];
  const MatchTeam& opp = m.teams[static_cast<size_t>(1 - side)];
  if (team.bench.empty() && !team.impact) return;

  const std::string oppNote =
      opp.impact ? cat(opp.name, " used theirs: ", P(m, opp.impact->in).name, " for ", P(m, opp.impact->out).name, ".") : "";

  if (team.impact) {
    beginPanel("##impact");
    eyebrow("Impact Player used");
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(
        cat(P(m, team.impact->in).name, " came in for ", P(m, team.impact->out).name, " (", team.impact->when, ").").c_str());
    ImGui::PopTextWrapPos();
    if (!oppNote.empty()) hint(oppNote);
    endPanel();
    return;
  }

  const ImpactOptions opts = impactOptions(m, side);
  if (!opts.allowed) st.open = false;
  // A choice that is no longer on offer (the window moved on, a batter walked in) is dropped
  if (!st.outId.empty() && std::find(opts.outs.begin(), opts.outs.end(), st.outId) == opts.outs.end()) st.outId.clear();
  if (!st.inId.empty() &&
      std::none_of(opts.ins.begin(), opts.ins.end(), [&](const ImpactIn& x) { return x.id == st.inId; }))
    st.inId.clear();

  beginPanel("##impact");
  // decision-head: eyebrow + status on the left, the toggle on the right
  const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  ImGui::BeginGroup();
  eyebrow("Impact Player");
  hint(opts.allowed ? "Available now: one substitution from your 5 named substitutes." : opts.reason);
  ImGui::EndGroup();
  if (opts.allowed) {
    const char* label = st.open ? "Close" : "Make a substitution";
    const float bw = textSize(FontRole::BodyStrong, label).x + 24;
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(right - bw, ImGui::GetItemRectMin().y));
    if (button(label, ButtonKind::Small)) st.open = !st.open;
  }
  if (!oppNote.empty()) hint(oppNote);

  if (st.open && opts.allowed) {
    if (const auto plan = aiImpactPlan(m, side)) {
      {
        FontScope fs(FontRole::Hint);
        ImGui::TextColored(vec(th.ink2), "Suggestion: bring in %s for %s.", P(m, plan->in).name.c_str(),
                           P(m, plan->out).name.c_str());
      }
      ImGui::SameLine(0, 8);
      if (textLink("Use suggestion")) {
        st.outId = plan->out;
        st.inId = plan->in;
      }
    }

    std::vector<std::string> outLabels, inLabels;
    for (const PlayerId& id : opts.outs) outLabels.push_back(shortLine(P(m, id)));
    for (const ImpactIn& x : opts.ins) inLabels.push_back(shortLine(P(m, x.id)));
    int outIdx = -1, inIdx = -1;
    for (size_t i = 0; i < opts.outs.size(); ++i)
      if (opts.outs[i] == st.outId) outIdx = static_cast<int>(i);
    for (size_t i = 0; i < opts.ins.size(); ++i)
      if (opts.ins[i].id == st.inId) inIdx = static_cast<int>(i);

    const float half = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
    const ImVec2 row = ImGui::GetCursorScreenPos();
    if (selectBox("impact-out", "Replace", outLabels, &outIdx, "Choose a player", half))
      st.outId = opts.outs[static_cast<size_t>(outIdx)];
    // side by side at the same top edge (SameLine would align text baselines instead)
    const float rowBottom = ImGui::GetItemRectMax().y;
    ImGui::SetCursorScreenPos(ImVec2(row.x + half + 12, row.y));
    if (selectBox("impact-in", "Bring in", inLabels, &inIdx, "Choose a substitute", half))
      st.inId = opts.ins[static_cast<size_t>(inIdx)].id;
    ImGui::SetCursorScreenPos(ImVec2(row.x, std::max(rowBottom, ImGui::GetItemRectMax().y) + ImGui::GetStyle().ItemSpacing.y));

    // IPL rule: at most 4 overseas players on the field
    int overseasAfter = 0;
    if (!st.outId.empty() && !st.inId.empty()) {
      for (const PlayerId& id : team.players)
        if (id != st.outId && P(m, id).overseas) ++overseasAfter;
      if (P(m, st.inId).overseas) ++overseasAfter;
    }
    const bool tooMany = overseasAfter > MAX_OVERSEAS;
    if (tooMany)
      notice(cat("That would put ", overseasAfter, " overseas players on the field; the limit is ", MAX_OVERSEAS, "."),
             NoticeKind::Error);
    hint("The replaced player takes no further part. The Impact Player can bowl a full 4 overs, but only 11 players may bat.");
    if (button("Bring him in", ButtonKind::Primary, !st.outId.empty() && !st.inId.empty() && !tooMany)) {
      if (s.impactSub(side, st.outId, st.inId)) st = ImpactPanelState{};
    }
  }
  endPanel();
}

}  // namespace t20::ui
