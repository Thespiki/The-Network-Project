#include "ui/UiContext.h"

#include <imgui.h>

namespace tnp::ui {
namespace {

/// How long a status message stays on screen. Long enough to read a sentence,
/// short enough not to become permanent furniture.
constexpr double kStatusDuration = 6.0;

} // namespace

void UiContext::setStatus(std::string message, bool isError) {
    statusMessage = std::move(message);
    statusIsError = isError;
    statusExpiresAt = ImGui::GetTime() + kStatusDuration;
}

bool UiContext::hasStatus() const {
    return !statusMessage.empty() && ImGui::GetTime() < statusExpiresAt;
}

} // namespace tnp::ui
