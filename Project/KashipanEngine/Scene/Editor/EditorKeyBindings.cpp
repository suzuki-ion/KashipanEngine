#include "EditorKeyBindings.h"
#ifdef USE_IMGUI

#include "Core/UserSettings.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

ImGuiKeyChord EditorKeyBindings::Get(const std::string &action, ImGuiKeyChord defaultChord) {
    LogScope scope;
    const JSON stored = UserSettings::GetJSON(kSettingsKey, JSON::object());
    if (stored.is_object()) {
        auto it = stored.find(action);
        if (it != stored.end() && it->is_number_integer()) {
            return static_cast<ImGuiKeyChord>(it->get<int>());
        }
    }
    return defaultChord;
}

void EditorKeyBindings::Set(const std::string &action, ImGuiKeyChord chord) {
    LogScope scope;
    JSON stored = UserSettings::GetJSON(kSettingsKey, JSON::object());
    if (!stored.is_object()) stored = JSON::object();
    stored[action] = static_cast<int>(chord);
    UserSettings::SetJSON(kSettingsKey, stored);
}

void EditorKeyBindings::ResetAll() {
    LogScope scope;
    UserSettings::SetJSON(kSettingsKey, JSON::object());
}

std::string EditorKeyBindings::ToDisplayString(ImGuiKeyChord chord) {
    LogScope scope;
    const ImGuiKey key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    if (chord == 0 || key == ImGuiKey_None) return "-";

    std::string result;
    if (chord & ImGuiMod_Ctrl) result += "Ctrl+";
    if (chord & ImGuiMod_Shift) result += "Shift+";
    if (chord & ImGuiMod_Alt) result += "Alt+";
    if (chord & ImGuiMod_Super) result += "Super+";
    const char *keyName = ImGui::GetKeyName(key);
    result += keyName ? keyName : "?";
    return result;
}

ImGuiKeyChord EditorKeyBindings::CaptureChordThisFrame() {
    LogScope scope;
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        const ImGuiKey key = static_cast<ImGuiKey>(k);
        // 修飾キー単体は組み合わせの構成要素であって、単体では割り当て対象にしない
        if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
            key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
            key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
            key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) {
            continue;
        }
        if (ImGui::IsKeyPressed(key, false)) {
            ImGuiKeyChord chord = key;
            const ImGuiIO &io = ImGui::GetIO();
            if (io.KeyCtrl) chord |= ImGuiMod_Ctrl;
            if (io.KeyShift) chord |= ImGuiMod_Shift;
            if (io.KeyAlt) chord |= ImGuiMod_Alt;
            if (io.KeySuper) chord |= ImGuiMod_Super;
            return chord;
        }
    }
    return ImGuiKey_None;
}

} // namespace KashipanEngine

#endif // USE_IMGUI
