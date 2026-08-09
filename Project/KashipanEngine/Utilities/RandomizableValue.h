#pragma once
#include <algorithm>
#include "Math/Vector3.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/RandomValue.h"

namespace KashipanEngine {

namespace detail {
inline float RandomizeInRange(float min, float max) {
    return GetRandomFloat(std::min(min, max), std::max(min, max));
}
inline int RandomizeInRange(int min, int max) {
    return GetRandomInt(std::min(min, max), std::max(min, max));
}
inline Vector3 RandomizeInRange(const Vector3 &min, const Vector3 &max) {
    return Vector3(
        RandomizeInRange(min.x, max.x),
        RandomizeInRange(min.y, max.y),
        RandomizeInRange(min.z, max.z));
}
} // namespace detail

/// @brief 「固定値」か「Min〜Maxのランダム範囲」かを切り替えられる値
/// @tparam T float、int、Vector3のいずれか
template <typename T>
struct RandomizableValue {
    bool randomize = false;
    T value{};
    T min{};
    T max{};

    /// @brief 現在の設定に基づき値を取得する（ランダム時は呼ぶたびに異なる値になる）
    T Get() const {
        return randomize ? detail::RandomizeInRange(min, max) : value;
    }
};

template <typename T>
JSON ToJSON(const RandomizableValue<T> &v) {
    JSON json = JSON::object();
    json["randomize"] = v.randomize;
    json["value"] = ToJSON(v.value);
    json["min"] = ToJSON(v.min);
    json["max"] = ToJSON(v.max);
    return json;
}

template <typename T>
struct FromJSONImpl<RandomizableValue<T>> {
    static RandomizableValue<T> invoke(const JSON &json) {
        RandomizableValue<T> v;
        v.randomize = json.value("randomize", false);
        if (json.contains("value")) v.value = FromJSON<T>(json["value"]);
        if (json.contains("min")) v.min = FromJSON<T>(json["min"]);
        if (json.contains("max")) v.max = FromJSON<T>(json["max"]);
        return v;
    }
};

} // namespace KashipanEngine
