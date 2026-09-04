#include "PlayerSettings.h"

#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/JSON.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

std::string PlayerSettings::GetFilePath() {
    LogScope scope;
    return ProjectPaths::InProjectRoot(kFileName);
}

bool PlayerSettings::GetBool(const std::string &key, bool defaultValue) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_boolean()) return defaultValue;
    return it->get<bool>();
}

void PlayerSettings::SetBool(const std::string &key, bool value) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_boolean() && it->get<bool>() == value) return;
    sData_[key] = value;
    Save();
}

float PlayerSettings::GetFloat(const std::string &key, float defaultValue) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_number()) return defaultValue;
    return it->get<float>();
}

void PlayerSettings::SetFloat(const std::string &key, float value) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_number() && it->get<float>() == value) return;
    sData_[key] = value;
    Save();
}

std::string PlayerSettings::GetString(const std::string &key, const std::string &defaultValue) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_string()) return defaultValue;
    return it->get<std::string>();
}

void PlayerSettings::SetString(const std::string &key, const std::string &value) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_string() && it->get<std::string>() == value) return;
    sData_[key] = value;
    Save();
}

JSON PlayerSettings::GetJSON(const std::string &key, const JSON &defaultValue) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    return *it;
}

void PlayerSettings::SetJSON(const std::string &key, const JSON &value) {
    LogScope scope;
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == value) return;
    sData_[key] = value;
    Save();
}

void PlayerSettings::EnsureLoaded() {
    LogScope scope;
    if (sLoaded_) return;
    sLoaded_ = true;
    sData_ = LoadJSON(GetFilePath());
    if (!sData_.is_object()) sData_ = JSON::object();
}

void PlayerSettings::Save() {
    LogScope scope;
    SaveJSON(sData_, GetFilePath());
}

} // namespace KashipanEngine
