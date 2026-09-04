#pragma once
#include <json.hpp>
#include <optional>
#include <string>
#include <vector>
#include "Debug/Logger.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Color.h"
#include "Assets/TextureRef.h"
#include "Assets/TextureCubeRef.h"
#include "Utilities/UUID128.h"

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
inline JSON ToJSON(const bool &value) { return JSON(value); }
inline JSON ToJSON(const int8_t &value) { return JSON(value); }
inline JSON ToJSON(const uint8_t &value) { return JSON(value); }
inline JSON ToJSON(const int16_t &value) { return JSON(value); }
inline JSON ToJSON(const uint16_t &value) { return JSON(value); }
inline JSON ToJSON(const int32_t &value) { return JSON(value); }
inline JSON ToJSON(const uint32_t &value) { return JSON(value); }
inline JSON ToJSON(const int64_t &value) { return JSON(value); }
inline JSON ToJSON(const uint64_t &value) { return JSON(value); }
inline JSON ToJSON(const float &value) { return JSON(value); }
inline JSON ToJSON(const double &value) { return JSON(value); }
inline JSON ToJSON(const std::string &value) { return JSON(value); }
inline JSON ToJSON(const Vector2 &value) { return JSON({ {"x", value.x}, {"y", value.y} }); }
inline JSON ToJSON(const Vector3 &value) { return JSON({ {"x", value.x}, {"y", value.y}, {"z", value.z} }); }
inline JSON ToJSON(const Vector4 &value) { return JSON({ {"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w} }); }
inline JSON ToJSON(const Quaternion &value) { return JSON({ {"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w} }); }
inline JSON ToJSON(const Color &value) { return JSON({ {"r", value.r}, {"g", value.g}, {"b", value.b}, {"a", value.a} }); }
inline JSON ToJSON(const TextureRef &value) { return JSON({ {"assetPath", value.assetPath} }); }
inline JSON ToJSON(const TextureCubeRef &value) { return JSON({ {"assetPath", value.assetPath} }); }
inline JSON ToJSON(const Matrix3x3 &value) {
    return JSON({
        {"m00", value.m[0][0]}, {"m01", value.m[0][1]}, {"m02", value.m[0][2]},
        {"m10", value.m[1][0]}, {"m11", value.m[1][1]}, {"m12", value.m[1][2]},
        {"m20", value.m[2][0]}, {"m21", value.m[2][1]}, {"m22", value.m[2][2]}
        });
}
inline JSON ToJSON(const Matrix4x4 &value) {
    return JSON({
        {"m00", value.m[0][0]}, {"m01", value.m[0][1]}, {"m02", value.m[0][2]}, {"m03", value.m[0][3]},
        {"m10", value.m[1][0]}, {"m11", value.m[1][1]}, {"m12", value.m[1][2]}, {"m13", value.m[1][3]},
        {"m20", value.m[2][0]}, {"m21", value.m[2][1]}, {"m22", value.m[2][2]}, {"m23", value.m[2][3]},
        {"m30", value.m[3][0]}, {"m31", value.m[3][1]}, {"m32", value.m[3][2]}, {"m33", value.m[3][3]}
        });
}
inline JSON ToJSON(const std::vector<JSON> &values) { return JSON(values); }
inline JSON ToJSON(const UUID128 &uuid) { return JSON(uuid.ToString()); }
template <typename T>
JSON ToJSON(const std::vector<T> &vec) {
    LogScope scope;
    JSON jsonArray = JSON::array();
    for (const auto &item : vec) {
        jsonArray.push_back(ToJSON(item));
    }
    return jsonArray;
}
template <typename T>
JSON ToJSON(const std::optional<T> &opt) {
    LogScope scope;
    if (opt.has_value()) {
        return ToJSON(opt.value());
    } else {
        return nullptr;
    }
}
template <typename K, typename V>
JSON ToJSON(const std::map<K, V> &map) {
    LogScope scope;
    JSON jsonObject = JSON::object();
    for (const auto &[key, value] : map) {
        jsonObject[std::to_string(key)] = ToJSON(value);
    }
    return jsonObject;
}
template <typename K, typename V>
JSON ToJSON(const std::unordered_map<K, V> &map) {
    LogScope scope;
    JSON jsonObject = JSON::object();
    for (const auto &[key, value] : map) {
        jsonObject[std::to_string(key)] = ToJSON(value);
    }
    return jsonObject;
}

// JSONから各種型への変換関数
template <typename T>
struct FromJSONImpl {
    static T invoke(const JSON &json) {
        return json.get<T>();
    }
};
template <typename T>
T FromJSON(const JSON &json) {
    return FromJSONImpl<T>::invoke(json);
}
template <>
struct FromJSONImpl<Vector2> {
    static Vector2 invoke(const JSON &json) {
        return Vector2(json.at("x").get<float>(), json.at("y").get<float>());
    }
};
template <>
struct FromJSONImpl<Vector3> {
    static Vector3 invoke(const JSON &json) {
        return Vector3(json.at("x").get<float>(), json.at("y").get<float>(), json.at("z").get<float>());
    }
};
template <>
struct FromJSONImpl<Vector4> {
    static Vector4 invoke(const JSON &json) {
        return Vector4(json.at("x").get<float>(), json.at("y").get<float>(), json.at("z").get<float>(), json.at("w").get<float>());
    }
};
template <>
struct FromJSONImpl<Quaternion> {
    static Quaternion invoke(const JSON &json) {
        return Quaternion(json.at("x").get<float>(), json.at("y").get<float>(), json.at("z").get<float>(), json.at("w").get<float>());
    }
};
template <>
struct FromJSONImpl<Matrix3x3> {
    static Matrix3x3 invoke(const JSON &json) {
        return Matrix3x3(
            json.at("m00").get<float>(), json.at("m01").get<float>(), json.at("m02").get<float>(),
            json.at("m10").get<float>(), json.at("m11").get<float>(), json.at("m12").get<float>(),
            json.at("m20").get<float>(), json.at("m21").get<float>(), json.at("m22").get<float>()
        );
    }
};
template <>
struct FromJSONImpl<Matrix4x4> {
    static Matrix4x4 invoke(const JSON &json) {
        return Matrix4x4(
            json.at("m00").get<float>(), json.at("m01").get<float>(), json.at("m02").get<float>(), json.at("m03").get<float>(),
            json.at("m10").get<float>(), json.at("m11").get<float>(), json.at("m12").get<float>(), json.at("m13").get<float>(),
            json.at("m20").get<float>(), json.at("m21").get<float>(), json.at("m22").get<float>(), json.at("m23").get<float>(),
            json.at("m30").get<float>(), json.at("m31").get<float>(), json.at("m32").get<float>(), json.at("m33").get<float>()
        );
    }
};
template <>
struct FromJSONImpl<Color> {
    static Color invoke(const JSON &json) {
        return Color(json.at("r").get<float>(), json.at("g").get<float>(), json.at("b").get<float>(), json.at("a").get<float>());
    }
};
template <>
struct FromJSONImpl<TextureRef> {
    static TextureRef invoke(const JSON &json) {
        return TextureRef(json.at("assetPath").get<std::string>());
    }
};
template <>
struct FromJSONImpl<TextureCubeRef> {
    static TextureCubeRef invoke(const JSON &json) {
        return TextureCubeRef(json.at("assetPath").get<std::string>());
    }
};
template <>
struct FromJSONImpl<UUID128> {
    static UUID128 invoke(const JSON &json) {
        return UUID128(json.get<std::string>());
    }
};

template <typename T>
struct FromJSONImpl<std::vector<T>> {
    static std::vector<T> invoke(const JSON &json) {
        LogScope scope;
        std::vector<T> vec;
        for (const auto &item : json) {
            vec.push_back(FromJSON<T>(item));
        }
        return vec;
    }
};
template <typename T>
struct FromJSONImpl<std::optional<T>> {
    static std::optional<T> invoke(const JSON &json) {
        LogScope scope;
        if (json.is_null()) {
            return std::nullopt;
        } else {
            return FromJSON<T>(json);
        }
    }
};
template <typename K, typename V>
struct FromJSONImpl<std::map<K, V>> {
    static std::map<K, V> invoke(const JSON &json) {
        LogScope scope;
        std::map<K, V> map;
        for (auto it = json.begin(); it != json.end(); ++it) {
            map[static_cast<K>(std::stoi(it.key()))] = FromJSON<V>(it.value());
        }
        return map;
    }
};
template <typename K, typename V>
struct FromJSONImpl<std::unordered_map<K, V>> {
    static std::unordered_map<K, V> invoke(const JSON &json) {
        LogScope scope;
        std::unordered_map<K, V> map;
        for (auto it = json.begin(); it != json.end(); ++it) {
            map[static_cast<K>(std::stoi(it.key()))] = FromJSON<V>(it.value());
        }
        return map;
    }
};

} // namespace KashipanEngine