#include "EditorSettings.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include "Utilities/FileIO.h"

namespace KashipanEngine {

bool EditorSettings::GetBool(const std::string &key, bool defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_boolean()) return defaultValue;
    return it->get<bool>();
}

void EditorSettings::SetBool(const std::string &key, bool value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_boolean() && it->get<bool>() == value) return;
    sData_[key] = value;
    SaveJSON(sData_, kFilePath);
}

bool EditorSettings::PersistentTreeNode(const char *label, const std::string &key, bool defaultOpen) {
    const bool stored = GetBool(key, defaultOpen);
    ImGui::SetNextItemOpen(stored, ImGuiCond_Once);
    const bool open = ImGui::TreeNode(label);
    if (open != stored) SetBool(key, open);
    return open;
}

bool EditorSettings::PersistentCollapsingHeader(const char *label, const std::string &key, bool defaultOpen) {
    const bool stored = GetBool(key, defaultOpen);
    ImGui::SetNextItemOpen(stored, ImGuiCond_Once);
    const bool open = ImGui::CollapsingHeader(label);
    if (open != stored) SetBool(key, open);
    return open;
}

void EditorSettings::EnsureLoaded() {
    if (sLoaded_) return;
    sData_ = LoadJSON(kFilePath);
    if (!sData_.is_object()) sData_ = JSON::object();
    sLoaded_ = true;
}

} // namespace KashipanEngine
#endif // USE_IMGUI
