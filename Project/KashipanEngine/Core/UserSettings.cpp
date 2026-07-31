#include "UserSettings.h"

#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

std::string UserSettings::GetFilePath() {
    return ProjectPaths::InEngineRoot(kFileName);
}

bool UserSettings::GetBool(const std::string &key, bool defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_boolean()) return defaultValue;
    return it->get<bool>();
}

void UserSettings::SetBool(const std::string &key, bool value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_boolean() && it->get<bool>() == value) return;
    sData_[key] = value;
    Save();
}

float UserSettings::GetFloat(const std::string &key, float defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_number()) return defaultValue;
    return it->get<float>();
}

void UserSettings::SetFloat(const std::string &key, float value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_number() && it->get<float>() == value) return;
    sData_[key] = value;
    Save();
}

std::string UserSettings::GetString(const std::string &key, const std::string &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_string()) return defaultValue;
    return it->get<std::string>();
}

void UserSettings::SetString(const std::string &key, const std::string &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_string() && it->get<std::string>() == value) return;
    sData_[key] = value;
    Save();
}

JSON UserSettings::GetJSON(const std::string &key, const JSON &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    return *it;
}

void UserSettings::SetJSON(const std::string &key, const JSON &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == value) return;
    sData_[key] = value;
    Save();
}

void UserSettings::EnsureLoaded() {
    if (sLoaded_) return;
    sData_ = LoadJSON(GetFilePath());
    if (!sData_.is_object()) sData_ = JSON::object();
    sLoaded_ = true;
}

void UserSettings::Save() {
    SaveJSON(sData_, GetFilePath());
}

} // namespace KashipanEngine
