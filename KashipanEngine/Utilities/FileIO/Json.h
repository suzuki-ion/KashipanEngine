#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace KashipanEngine {

using Json = nlohmann::json;

// 基本的な読み込み・保存機能
Json LoadJson(const std::string &filepath);
bool SaveJson(const Json &jsonData, const std::string &filepath, int indent = 4);

// 安全な値取得機能
template<typename T>
std::optional<T> GetJsonValue(const Json &json, const std::string &key);

template<typename T>
T GetJsonValueOrDefault(const Json &json, const std::string &key, const T &defaultValue);

// ネストしたキーへのアクセス（例: "object.array[0].value"）
std::optional<Json> GetNestedJsonValue(const Json &json, const std::string &path);

// JSON検証・ユーティリティ機能
bool ValidateJsonStructure(const Json &json, const std::vector<std::string> &requiredKeys);
bool IsJsonFileValid(const std::string &filepath);

// JSON合成機能
Json MergeJson(const Json &base, const Json &overlay, bool deepMerge = true);

// 配列操作
bool AppendToJsonArray(Json &json, const std::string &arrayKey, const Json &value);
bool RemoveFromJsonArray(Json &json, const std::string &arrayKey, size_t index);

// デバッグ・表示機能
std::string JsonToFormattedString(const Json &json, int indent = 4);
void PrintJson(const Json &json, const std::string &title = "JSON Data");

/// @brief Jsonデータ管理クラス
class JsonData {
public:
    JsonData() = default;
    JsonData(const std::string &filepath) : jsonData_(LoadJson(filepath)) {}
    JsonData(const Json &json) : jsonData_(json) {}
    ~JsonData() = default;

    bool Load(const std::string &filepath) {
        jsonData_ = LoadJson(filepath);
        return !jsonData_.is_null();
    }
    bool LoadToString(const std::string &jsonString) {
        jsonData_ = Json::parse(jsonString, nullptr, false, true);
        return !jsonData_.is_null();
    }
    bool Save(const std::string &filepath, int indent = 4) const {
        return SaveJson(jsonData_, filepath, indent);
    }
    const Json &GetJson() const { return jsonData_; }
    Json &GetJson() { return jsonData_; }
    bool Clear() {
        jsonData_.clear();
        return jsonData_.is_null();
    }

    template<typename T>
    std::optional<T> GetValue(const std::string &key) const {
        return GetJsonValue<T>(jsonData_, key);
    }
    template<typename T>
    T GetValue(const std::string &key, const T &defaultValue) const {
        return GetJsonValueOrDefault<T>(jsonData_, key, defaultValue);
    }
    std::optional<Json> GetNestedValue(const std::string &path) const {
        return GetNestedJsonValue(jsonData_, path);
    }
    bool ValidateStructure(const std::vector<std::string> &requiredKeys) const {
        return ValidateJsonStructure(jsonData_, requiredKeys);
    }
    bool IsValidFile(const std::string &filepath) const {
        return IsJsonFileValid(filepath);
    }
    bool Merge(const Json &overlay, bool deepMerge = true) {
        if (jsonData_.is_null()) {
            jsonData_ = overlay;
            return true;
        }
        jsonData_ = MergeJson(jsonData_, overlay, deepMerge);
        return true;
    }
    bool AppendToArray(const std::string &arrayKey, const Json &value) {
        return AppendToJsonArray(jsonData_, arrayKey, value);
    }
    bool RemoveFromArray(const std::string &arrayKey, size_t index) {
        return RemoveFromJsonArray(jsonData_, arrayKey, index);
    }
    std::string ToFormattedString(int indent = 4) const {
        return JsonToFormattedString(jsonData_, indent);
    }
    void Print(const std::string &title = "JSON Data") const {
        PrintJson(jsonData_, title);
    }

private:
    Json jsonData_;
};

} // namespace KashipanEngine