#include "UserSettings.h"

#include <algorithm>
#include <filesystem>

#include "Core/ProjectPaths.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/RawFile.h"
#include "Utilities/FileIO/TextFile.h"

namespace KashipanEngine {

namespace {

/// @brief プリセット名をファイル名として使えるよう、Windowsで使用できない文字を置換する
std::string SanitizeFileName(const std::string &name) {
    std::string result = name;
    for (char &c : result) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    if (result.empty()) result = "_";
    return result;
}

/// @brief フォルダ直下（非再帰）にある指定拡張子のファイル名一覧を、拡張子を除いた状態で取得する
std::vector<std::string> ListFileStems(const std::string &folderPath, const std::string &extension) {
    const DirectoryData dir = GetDirectoryDataByExtension(folderPath, { extension }, false, false);
    std::vector<std::string> names;
    names.reserve(dir.files.size());
    for (const auto &file : dir.files) {
        names.push_back(PathToUtf8String(Utf8StringToPath(file).stem()));
    }
    std::sort(names.begin(), names.end());
    return names;
}

/// @brief 複数行のiniテキストを、SaveTextFileが行単位で書き出せるよう分割する
std::vector<std::string> SplitLines(const std::string &text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t newlinePos = text.find('\n', start);
        if (newlinePos == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, newlinePos - start));
        start = newlinePos + 1;
    }
    return lines;
}

} // namespace

std::string UserSettings::GetSettingsFilePath() {
    return ProjectPaths::InEngineRoot(std::string(kFolderName) + "/" + kSettingsFileName);
}

std::string UserSettings::GetColorPresetsFolderPath() {
    return ProjectPaths::InEngineRoot(std::string(kFolderName) + "/" + kColorPresetsSubfolder);
}

std::string UserSettings::GetLayoutPresetsFolderPath() {
    return ProjectPaths::InEngineRoot(std::string(kFolderName) + "/" + kLayoutPresetsSubfolder);
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

std::vector<std::string> UserSettings::GetColorPresetNames() {
    EnsureLoaded();
    return ListFileStems(GetColorPresetsFolderPath(), ".json");
}

JSON UserSettings::GetColorPreset(const std::string &name, const JSON &defaultValue) {
    EnsureLoaded();
    const JSON data = LoadJSON(GetColorPresetsFolderPath() + "/" + SanitizeFileName(name) + ".json");
    return data.is_array() ? data : defaultValue;
}

void UserSettings::SetColorPreset(const std::string &name, const JSON &colors) {
    EnsureLoaded();
    SaveJSON(colors, GetColorPresetsFolderPath() + "/" + SanitizeFileName(name) + ".json");
}

void UserSettings::DeleteColorPreset(const std::string &name) {
    EnsureLoaded();
    RemoveFile(GetColorPresetsFolderPath() + "/" + SanitizeFileName(name) + ".json");
}

std::vector<std::string> UserSettings::GetLayoutPresetNames() {
    EnsureLoaded();
    return ListFileStems(GetLayoutPresetsFolderPath(), ".ini");
}

std::string UserSettings::GetLayoutPreset(const std::string &name) {
    EnsureLoaded();
    const TextFileData data = LoadTextFile(GetLayoutPresetsFolderPath() + "/" + SanitizeFileName(name) + ".ini");
    std::string text;
    for (size_t i = 0; i < data.lines.size(); ++i) {
        if (i != 0) text += '\n';
        text += data.lines[i];
    }
    return text;
}

void UserSettings::SetLayoutPreset(const std::string &name, const std::string &iniText) {
    EnsureLoaded();
    TextFileData data;
    data.filePath = GetLayoutPresetsFolderPath() + "/" + SanitizeFileName(name) + ".ini";
    data.lines = SplitLines(iniText);
    SaveTextFile(data);
}

void UserSettings::DeleteLayoutPreset(const std::string &name) {
    EnsureLoaded();
    RemoveFile(GetLayoutPresetsFolderPath() + "/" + SanitizeFileName(name) + ".ini");
}

void UserSettings::EnsureLoaded() {
    if (sLoaded_) return;
    sLoaded_ = true;
    MigrateFromLegacyFileIfNeeded();
    sData_ = LoadJSON(GetSettingsFilePath());
    if (!sData_.is_object()) sData_ = JSON::object();
}

void UserSettings::Save() {
    SaveJSON(sData_, GetSettingsFilePath());
}

void UserSettings::MigrateFromLegacyFileIfNeeded() {
    const std::string settingsPath = GetSettingsFilePath();
    if (IsFileExist(settingsPath)) return; // 移行済み、または新規インストール後に既にSettings.jsonがある

    const std::string legacyPath = ProjectPaths::InEngineRoot(kLegacyFileName);
    JSON legacy = LoadJSON(legacyPath);
    if (!legacy.is_object()) return; // 旧ファイルが存在しない＝新規インストール

    // 配色プリセットを1項目1ファイルへ分割
    auto presetsIt = legacy.find("editorUI.presets");
    if (presetsIt != legacy.end() && presetsIt->is_object()) {
        for (auto it = presetsIt->begin(); it != presetsIt->end(); ++it) {
            SaveJSON(it.value(), GetColorPresetsFolderPath() + "/" + SanitizeFileName(it.key()) + ".json");
        }
        legacy.erase("editorUI.presets");
    }

    // レイアウトプリセットを1項目1ファイル（生のiniテキスト）へ分割
    auto layoutsIt = legacy.find("editorUI.layoutPresets");
    if (layoutsIt != legacy.end() && layoutsIt->is_object()) {
        for (auto it = layoutsIt->begin(); it != layoutsIt->end(); ++it) {
            if (!it.value().is_string()) continue;
            TextFileData data;
            data.filePath = GetLayoutPresetsFolderPath() + "/" + SanitizeFileName(it.key()) + ".ini";
            data.lines = SplitLines(it.value().get_ref<const std::string &>());
            SaveTextFile(data);
        }
        legacy.erase("editorUI.layoutPresets");
    }

    // 残り（フォント・スケール・言語・キーバインド・各種Addedフラグ等）はそのままSettings.jsonへ
    SaveJSON(legacy, settingsPath);

    // 旧ファイルは万一に備えて削除せず、バックアップとしてリネームして残す
    std::error_code ec;
    std::filesystem::rename(
        Utf8StringToPath(legacyPath), Utf8StringToPath(legacyPath + ".bak"), ec);
}

} // namespace KashipanEngine
