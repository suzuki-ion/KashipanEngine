#include "PlayerSettings.h"

#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

std::string PlayerSettings::GetFilePath() {
    return ProjectPaths::InProjectRoot(kFileName);
}

bool PlayerSettings::GetBool(const std::string &key, bool defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_boolean()) return defaultValue;
    return it->get<bool>();
}

void PlayerSettings::SetBool(const std::string &key, bool value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_boolean() && it->get<bool>() == value) return;
    sData_[key] = value;
    Save();
}

int PlayerSettings::GetInt(const std::string &key, int defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_number()) return defaultValue;
    return it->get<int>();
}

void PlayerSettings::SetInt(const std::string &key, int value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_number() && it->get<int>() == value) return;
    sData_[key] = value;
    Save();
}

float PlayerSettings::GetFloat(const std::string &key, float defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_number()) return defaultValue;
    return it->get<float>();
}

void PlayerSettings::SetFloat(const std::string &key, float value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_number() && it->get<float>() == value) return;
    sData_[key] = value;
    Save();
}

std::string PlayerSettings::GetString(const std::string &key, const std::string &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end() || !it->is_string()) return defaultValue;
    return it->get<std::string>();
}

void PlayerSettings::SetString(const std::string &key, const std::string &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && it->is_string() && it->get<std::string>() == value) return;
    sData_[key] = value;
    Save();
}

Vector2 PlayerSettings::GetVector2(const std::string &key, const Vector2 &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    try { return FromJSON<Vector2>(*it); } catch (const std::exception &) { return defaultValue; }
}

void PlayerSettings::SetVector2(const std::string &key, const Vector2 &value) {
    EnsureLoaded();
    JSON json = ToJSON(value);
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == json) return;
    sData_[key] = json;
    Save();
}

Vector3 PlayerSettings::GetVector3(const std::string &key, const Vector3 &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    try { return FromJSON<Vector3>(*it); } catch (const std::exception &) { return defaultValue; }
}

void PlayerSettings::SetVector3(const std::string &key, const Vector3 &value) {
    EnsureLoaded();
    JSON json = ToJSON(value);
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == json) return;
    sData_[key] = json;
    Save();
}

Vector4 PlayerSettings::GetVector4(const std::string &key, const Vector4 &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    try { return FromJSON<Vector4>(*it); } catch (const std::exception &) { return defaultValue; }
}

void PlayerSettings::SetVector4(const std::string &key, const Vector4 &value) {
    EnsureLoaded();
    JSON json = ToJSON(value);
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == json) return;
    sData_[key] = json;
    Save();
}

Quaternion PlayerSettings::GetQuaternion(const std::string &key, const Quaternion &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    try { return FromJSON<Quaternion>(*it); } catch (const std::exception &) { return defaultValue; }
}

void PlayerSettings::SetQuaternion(const std::string &key, const Quaternion &value) {
    EnsureLoaded();
    JSON json = ToJSON(value);
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == json) return;
    sData_[key] = json;
    Save();
}

JSON PlayerSettings::GetJSON(const std::string &key, const JSON &defaultValue) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it == sData_.end()) return defaultValue;
    return *it;
}

void PlayerSettings::SetJSON(const std::string &key, const JSON &value) {
    EnsureLoaded();
    auto it = sData_.find(key);
    if (it != sData_.end() && *it == value) return;
    sData_[key] = value;
    Save();
}

void PlayerSettings::EnsureLoaded() {
    if (sLoaded_) return;
    sLoaded_ = true;
    sData_ = LoadJSON(GetFilePath());
    if (!sData_.is_object()) sData_ = JSON::object();
}

void PlayerSettings::Save() {
    SaveJSON(sData_, GetFilePath());
}

} // namespace KashipanEngine
