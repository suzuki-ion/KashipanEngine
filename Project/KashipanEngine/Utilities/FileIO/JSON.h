#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"

namespace KashipanEngine {

using JSON = nlohmann::json;
using Json = nlohmann::json;
using json = nlohmann::json;

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

// 各種型のJSON変換関数
JSON ToJSON(const bool &value);
JSON ToJSON(const int &value);
JSON ToJSON(const float &value);
JSON ToJSON(const double &value);
JSON ToJSON(const std::string &value);
JSON ToJSON(const Vector2 &value);
JSON ToJSON(const Vector3 &value);
JSON ToJSON(const Vector4 &value);
JSON ToJSON(const Quaternion &value);
JSON ToJSON(const Matrix3x3 &value);
JSON ToJSON(const Matrix4x4 &value);
JSON ToJSON(const std::vector<JSON> &values);
template <typename T>
JSON ToJSON(const std::vector<T> &vec) {
    JSON jsonArray = JSON::array();
    for (const auto &item : vec) {
        jsonArray.push_back(ToJSON(item));
    }
    return jsonArray;
}
template <typename T>
JSON ToJSON(const std::optional<T> &opt) {
    if (opt.has_value()) {
        return ToJSON(opt.value());
    } else {
        return nullptr;
    }
}
template <typename K, typename V>
JSON ToJSON(const std::map<K, V> &map) {
    JSON jsonObject = JSON::object();
    for (const auto &[key, value] : map) {
        jsonObject[std::to_string(key)] = ToJSON(value);
    }
    return jsonObject;
}
template <typename K, typename V>
JSON ToJSON(const std::unordered_map<K, V> &map) {
    JSON jsonObject = JSON::object();
    for (const auto &[key, value] : map) {
        jsonObject[std::to_string(key)] = ToJSON(value);
    }
    return jsonObject;
}

} // namespace KashipanEngine