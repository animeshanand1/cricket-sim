#include "ui/widgets/modal.hpp"

#include <imgui.h>

#include <algorithm>

#include "ui/anim.hpp"
#include "ui/gfx.hpp"
#include "ui/icons.hpp"
#include "ui/theme.hpp"

namespace t20::ui {

bool beginModal(const char* modalId, bool* isOpen, const char* title, float maxWidth) {
  const bool wanted = isOpen && *isOpen;
  if (!wanted) {
    if (ImGui::IsPopupOpen(modalId)) {
      // Closed from outside (the game state): let ImGui forget it on the next Begin
      if (ImGui::BeginPopupModal(modalId, nullptr, ImGuiWindowFlags_NoTitleBar)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
    }
    return false;
  }
  const auto& th = currentTheme();
  const std::string tweenId = std::string("modal:") + modalId;
  if (!ImGui::IsPopupOpen(modalId)) {
    ImGui::OpenPopup(modalId);
    startTween(tweenId, 0.0f, 1.0f, 0.22, EaseType::EaseOutQuad);
  }

  // Centred, at most maxWidth wide and 80px short of the window; pops in from 12px lower
  const ImVec2 disp = ImGui::GetIO().DisplaySize;
  const float w = std::min(maxWidth, disp.x - 48.0f);
  const float t = sampleTween(tweenId, 1.0f);
  ImGui::SetNextWindowPos(ImVec2(disp.x * 0.5f, disp.y * 0.5f + (1.0f - t) * 12.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0), ImVec2(w, disp.y - 80.0f));
  ImGui::PushStyleColor(ImGuiCol_PopupBg, vec(th.surface));
  ImGui::PushStyleColor(ImGuiCol_Border, vec(th.lineStrong));
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(3 / 255.0f, 7 / 255.0f, 12 / 255.0f, 0.72f * t));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, metrics().modalRadius);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 20));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f + 0.65f * t);
  const bool open = ImGui::BeginPopupModal(modalId, nullptr,
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(3);
  if (!open) {
    *isOpen = false;
    return false;
  }

  // Title row: title on the left, close button on the right; Esc closes too
  const float right = ImGui::GetWindowContentRegionMax().x;
  const float y0 = ImGui::GetCursorPosY();
  if (title && *title) {
    FontScope fs(FontRole::H2);
    ImGui::TextUnformatted(title);
  }
  const float y1 = ImGui::GetCursorPosY();
  ImGui::SetCursorPos(ImVec2(right - 28, y0 - 2));
  ImGui::PushID("##modal_close");
  const bool closeClicked = iconWidget("x", 18.0f, th.ink2, "Close (Esc)");
  ImGui::PopID();
  if (closeClicked || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    *isOpen = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SetCursorPosY(std::max(y1, ImGui::GetCursorPosY()));
  ImGui::Dummy(ImVec2(0, 4));
  return true;
}

void endModal() {
  ImGui::EndPopup();
}

}  // namespace t20::ui
