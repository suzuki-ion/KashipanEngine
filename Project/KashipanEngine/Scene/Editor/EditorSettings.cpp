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

float EditorSettings::GetFloat(const std::string &key, float defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_number()) return defaultValue;
    return it->get<float>();
}

void EditorSettings::SetFloat(const std::string &key, float value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_number() && it->get<float>() == value) return;
    sData_[key] = value;
    SaveJSON(sData_, kFilePath);
}

std::string EditorSettings::GetString(const std::string &key, const std::string &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_string()) return defaultValue;
    return it->get<std::string>();
}

void EditorSettings::SetString(const std::string &key, const std::string &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_string() && it->get<std::string>() == value) return;
    sData_[key] = value;
    SaveJSON(sData_, kFilePath);
}

JSON EditorSettings::GetJSON(const std::string &key, const JSON &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    return *it;
}

void EditorSettings::SetJSON(const std::string &key, const JSON &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == value) return;
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
