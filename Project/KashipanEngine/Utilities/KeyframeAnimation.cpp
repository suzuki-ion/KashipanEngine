#include "Utilities/KeyframeAnimation.h"

#include <algorithm>

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
    keyframes_.clear();
    if (!json.is_object() || !json.contains("keyframes") || !json["keyframes"].is_array()) {
        return false;
    }
    for (const auto &keyJson : json["keyframes"]) {
        if (!keyJson.is_object()) continue;
        Keyframe key;
        key.time = keyJson.value("time", 0.0f);
        key.value = keyJson.value("value", 0.0f);
        key.easeType = StringToEaseType(keyJson.value("ease", std::string{ "Linear" }));
        keyframes_.push_back(key);
    }
    std::stable_sort(keyframes_.begin(), keyframes_.end(),
        [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
    return true;
}

} // namespace KashipanEngine
