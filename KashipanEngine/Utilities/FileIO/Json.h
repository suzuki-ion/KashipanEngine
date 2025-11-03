#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace KashipanEngine {

using JSON = nlohmann::json;

// 基本的な読み込み・保存機能
JSON LoadJSON(const std::string &filepath);
bool SaveJSON(const JSON &jsonData, const std::string &filepath, int indent = 4);

// 安全な値取得機能
template<typename T>
std::optional<T> GetJSONValue(const JSON &json, const std::string &key);

template<typename T>
T GetJSONValueOrDefault(const JSON &json, const std::string &key, const T &defaultValue);

// ネストしたキーへのアクセス（例: "object.array[0].value"）
std::optional<JSON> GetNestedJSONValue(const JSON &json, const std::string &path);

// JSON検証・ユーティリティ機能
bool ValidateJSONStructure(const JSON &json, const std::vector<std::string> &requiredKeys);
bool IsJSONFileValid(const std::string &filepath);

// JSON合成機能
JSON MergeJSON(const JSON &base, const JSON &overlay, bool deepMerge = true);

// 配列操作
bool AppendToJSONArray(JSON &json, const std::string &arrayKey, const JSON &value);
bool RemoveFromJSONArray(JSON &json, const std::string &arrayKey, size_t index);

// デバッグ・表示機能
std::string JSONToFormattedString(const JSON &json, int indent = 4);
void PrintJSON(const JSON &json, const std::string &title = "JSON Data");

} // namespace KashipanEngine