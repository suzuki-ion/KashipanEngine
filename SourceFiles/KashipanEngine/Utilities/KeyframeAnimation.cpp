#include "Utilities/KeyframeAnimation.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

void KeyframeAnimation::AddKeyframe(float time, float value, EaseType easeType) {
    Keyframe key;
    key.time = time;
    key.value = value;
    key.easeType = easeType;
    // 時刻昇順を保って挿入する（同時刻のキーは後から追加した方が後ろになる）
    const auto it = std::upper_bound(keyframes_.begin(), keyframes_.end(), time,
        [](float t, const Keyframe &k) { return t < k.time; });
    keyframes_.insert(it, key);
}

float KeyframeAnimation::Evaluate(float time) const {
    if (keyframes_.empty()) return 0.0f;
    if (time <= keyframes_.front().time) return keyframes_.front().value;
    if (time >= keyframes_.back().time) return keyframes_.back().value;

    // timeより後の最初のキーを探し、その1つ前のキーとの間をイージング補間する
    const auto next = std::upper_bound(keyframes_.begin(), keyframes_.end(), time,
        [](float t, const Keyframe &k) { return t < k.time; });
    const Keyframe &to = *next;
    const Keyframe &from = *(next - 1);
    const float span = to.time - from.time;
    if (span <= 0.0f) return to.value;
    const float t = (time - from.time) / span;
    return Lerp(from.value, to.value, Apply(t, from.easeType));
}

JSON KeyframeAnimation::SaveToJson() const {
    JSON json = JSON::object();
    JSON keysJson = JSON::array();
    for (const auto &key : keyframes_) {
        keysJson.push_back(JSON{
            { "time", key.time },
            { "value", key.value },
            { "ease", EaseTypeToString(key.easeType) },
        });
    }
    json["keyframes"] = std::move(keysJson);
    return json;
}

bool KeyframeAnimation::LoadFromJson(const JSON &json) {
    // 全キーを検証してから差し替える（不正なデータの場合は以前のキー列を保持する）
    if (!json.is_object() || !json.contains("keyframes") || !json["keyframes"].is_array()) {
        return false;
    }
    const auto &keysJson = json["keyframes"];
    if (keysJson.size() > kMaxKeyframes) return false;
    auto finiteNumber = [](const JSON &keyJson, const char *name, float &out) {
        if (!keyJson.contains(name) || !keyJson[name].is_number()) return false;
        const double value = keyJson[name].get<double>();
        if (!std::isfinite(value) || std::abs(value) > kMaxAbsValue) return false;
        out = static_cast<float>(value);
        return true;
    };
    std::vector<Keyframe> loaded;
    loaded.reserve(keysJson.size());
    for (const auto &keyJson : keysJson) {
        if (!keyJson.is_object()) return false;
        Keyframe key;
        if (!finiteNumber(keyJson, "time", key.time) || key.time < 0.0f) return false;
        if (!finiteNumber(keyJson, "value", key.value)) return false;
        // easeは省略時Linear（従来形式との互換）。指定された場合は既知の名前のみ受け付ける
        if (keyJson.contains("ease")) {
            if (!keyJson["ease"].is_string() || !TryStringToEaseType(keyJson["ease"].get<std::string>(), key.easeType)) return false;
        }
        loaded.push_back(key);
    }
    std::stable_sort(loaded.begin(), loaded.end(),
        [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
    keyframes_ = std::move(loaded);
    return true;
}

} // namespace KashipanEngine
