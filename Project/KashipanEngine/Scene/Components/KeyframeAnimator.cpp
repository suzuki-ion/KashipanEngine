#include "KeyframeAnimator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Assets/AnimationManager.h"
#include "Assets/SkeletonManager.h"
#include "Scene/SceneContext.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/3D/Transform3D.h"

#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

namespace {
float EvaluateTimelineFloat(const KeyframeTimeline &timeline, float time) {
    if (timeline.keys.empty()) return 0.0f;
    if (timeline.keys.size() == 1) return std::get<float>(timeline.keys.front().value);

    const float lastKeyTime = timeline.keys.back().time;
    float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return std::get<float>(timeline.keys.back().value);

    float sampledTime = time;
    if (timeline.loop) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const KeyframeNode &key) {
            return t < key.time;
        });

    if (upper == timeline.keys.begin()) return std::get<float>(timeline.keys.front().value);
    if (upper == timeline.keys.end()) return std::get<float>(timeline.keys.back().value);

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float normalized = Normalize01(sampledTime, from.time, to.time);
    return Eased(std::get<float>(from.value), std::get<float>(to.value), normalized, from.easeType);
}

Vector3 EvaluateTimelineVector3(const KeyframeTimeline &timeline, float time) {
    if (timeline.keys.empty()) return Vector3();
    if (timeline.keys.size() == 1) return std::get<Vector3>(timeline.keys.front().value);

    const float lastKeyTime = timeline.keys.back().time;
    float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return std::get<Vector3>(timeline.keys.back().value);

    float sampledTime = time;
    if (timeline.loop) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const KeyframeNode &key) {
            return t < key.time;
        });

    if (upper == timeline.keys.begin()) return std::get<Vector3>(timeline.keys.front().value);
    if (upper == timeline.keys.end()) return std::get<Vector3>(timeline.keys.back().value);

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float normalized = Normalize01(sampledTime, from.time, to.time);

    const auto &fromValue = std::get<Vector3>(from.value);
    const auto &toValue = std::get<Vector3>(to.value);
    return Vector3{
        Eased(fromValue.x, toValue.x, normalized, from.easeType),
        Eased(fromValue.y, toValue.y, normalized, from.easeType),
        Eased(fromValue.z, toValue.z, normalized, from.easeType)
    };
}

Quaternion EvaluateTimelineQuaternion(const KeyframeTimeline &timeline, float time) {
    if (timeline.keys.empty()) return Quaternion::Identity();
    if (timeline.keys.size() == 1) return std::get<Quaternion>(timeline.keys.front().value);

    const float lastKeyTime = timeline.keys.back().time;
    float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return std::get<Quaternion>(timeline.keys.back().value);

    float sampledTime = time;
    if (timeline.loop) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const KeyframeNode &key) {
            return t < key.time;
        });

    if (upper == timeline.keys.begin()) return std::get<Quaternion>(timeline.keys.front().value);
    if (upper == timeline.keys.end()) return std::get<Quaternion>(timeline.keys.back().value);

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float normalized = Normalize01(sampledTime, from.time, to.time);
    return Quaternion::Slerp(std::get<Quaternion>(from.value), std::get<Quaternion>(to.value), normalized);
}

KeyframeValue EvaluateTimeline(const KeyframeTimeline &timeline, float time) {
    switch (timeline.valueType) {
    case KeyframeValueType::Vector3:
        return EvaluateTimelineVector3(timeline, time);
    case KeyframeValueType::Quaternion:
        return EvaluateTimelineQuaternion(timeline, time);
    case KeyframeValueType::Float:
    default:
        return EvaluateTimelineFloat(timeline, time);
    }
}

std::string EnsureJsonExtension(std::string path) {
    if (path.empty()) return path;

    auto lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
        });

    if (!lowerPath.ends_with(".json")) {
        path += ".json";
    }
    return path;
}

void SortTimelineKeys(KeyframeTimeline &timeline) {
    std::sort(timeline.keys.begin(), timeline.keys.end(), [](const KeyframeNode &lhs, const KeyframeNode &rhs) {
        return lhs.time < rhs.time;
        });

    if (!timeline.keys.empty()) {
        const float maxKeyTime = timeline.keys.back().time;
        if (timeline.duration < maxKeyTime) {
            timeline.duration = maxKeyTime;
        }
    }
}

} // namespace

bool KeyframeAnimator::PlayFromAnimationHandle(uint32_t handle, const std::string &objectName, bool loop) {
    if (handle == AnimationManager::kInvalidHandle) return false;
    if (objectName.empty()) return false;

    auto *ctx = GetOwnerContext();
    if (!ctx) return false;

    Transform2D *transform2D = nullptr;
    Transform3D *transform3D = nullptr;

    if (auto *obj2D = ctx->GetObject2D(objectName)) {
        transform2D = obj2D->GetComponent2D<Transform2D>();
    }
    if (!transform2D) {
        if (auto *obj3D = ctx->GetObject3D(objectName)) {
            transform3D = obj3D->GetComponent3D<Transform3D>();
        }
    }

    if (!transform2D && !transform3D) return false;

    const auto &data = AnimationManager::GetAnimationData(handle);
    if (data.GetClipCount() == 0) return false;

    const auto *clip = data.GetClip(0);
    if (!clip) return false;

    timelines_.clear();
    playbackStates_.clear();

    for (const auto &src : clip->timelines) {
        KeyframeTimeline timeline;
        timeline.name = src.name;
        timeline.duration = src.duration;
        timeline.valueType = src.valueType;
        timeline.keys = src.keys;
        timeline.loop = loop;

        if (transform2D) {
            if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto t = transform2D->GetTranslate();
                    t.x = v;
                    transform2D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto t = transform2D->GetTranslate();
                    t.y = v;
                    transform2D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto t = transform2D->GetTranslate();
                    t.z = v;
                    transform2D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto s = transform2D->GetScale();
                    s.x = v;
                    transform2D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto s = transform2D->GetScale();
                    s.y = v;
                    transform2D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto s = transform2D->GetScale();
                    s.z = v;
                    transform2D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto r = transform2D->GetRotate();
                    r.x = v;
                    transform2D->SetRotate(r);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto r = transform2D->GetRotate();
                    r.y = v;
                    transform2D->SetRotate(r);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform2D](float v) {
                    if (!transform2D) return;
                    auto r = transform2D->GetRotate();
                    r.z = v;
                    transform2D->SetRotate(r);
                }));
            }
        } else if (transform3D) {
            if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto t = transform3D->GetTranslate();
                    t.x = v;
                    transform3D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto t = transform3D->GetTranslate();
                    t.y = v;
                    transform3D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto t = transform3D->GetTranslate();
                    t.z = v;
                    transform3D->SetTranslate(t);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto s = transform3D->GetScale();
                    s.x = v;
                    transform3D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto s = transform3D->GetScale();
                    s.y = v;
                    transform3D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto s = transform3D->GetScale();
                    s.z = v;
                    transform3D->SetScale(s);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto r = transform3D->GetRotate();
                    r.x = v;
                    transform3D->SetRotate(r);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto r = transform3D->GetRotate();
                    r.y = v;
                    transform3D->SetRotate(r);
                }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform3D](float v) {
                    if (!transform3D) return;
                    auto r = transform3D->GetRotate();
                    r.z = v;
                    transform3D->SetRotate(r);
                }));
            } else if (timeline.valueType == KeyframeValueType::Quaternion && (timeline.name.ends_with(".Rotate") || timeline.name.ends_with(".RotateQuat"))) {
                timeline.applyFunctions.push_back(std::function<void(const Quaternion &)>([transform3D](const Quaternion &v) {
                    if (!transform3D) return;
                    transform3D->SetRotateQuaternion(v);
                }));
            }
        }

        if (timeline.applyFunctions.empty()) continue;

        SortTimelineKeys(timeline);
        timelines_.emplace(timeline.name, std::move(timeline));

        KeyframePlaybackState state;
        state.timelineName = src.name;
        state.elapsedTime = 0.0f;
        state.playing = true;
        state.paused = false;
        playbackStates_.emplace(state.timelineName, std::move(state));
    }

    return !timelines_.empty();
}

bool KeyframeAnimator::PlayFromAnimationAndSkeletonHandle(uint32_t animationHandle, uint32_t skeletonHandle, bool loop, std::string clipName) {
    if (animationHandle == AnimationManager::kInvalidHandle) return false;
    if (skeletonHandle == SkeletonManager::kInvalidHandle) return false;

    const auto &data = AnimationManager::GetAnimationData(animationHandle);
    if (data.GetClipCount() == 0) return false;

    const auto *clip = !clipName.empty() ? data.FindClipByName(clipName) : data.GetClip(0);
    if (!clip) return false;

    const auto &skeletonData = SkeletonManager::GetSkeletonData(skeletonHandle);
    if (skeletonData.GetSkeleton().joints.empty()) return false;

    timelines_.clear();
    playbackStates_.clear();

    for (auto &joint : skeletonData.GetSkeleton().joints) {
        auto timelineIndices = clip->nodeNameToTimelineIndices.find(joint.name);
        if (timelineIndices == clip->nodeNameToTimelineIndices.end()) continue;
        for (auto timelineIndex : timelineIndices->second) {
            if (timelineIndex < 0 || timelineIndex >= static_cast<uint32_t>(clip->timelines.size())) continue;
            const auto &src = clip->timelines[timelineIndex];
            KeyframeTimeline timeline;
            timeline.name = src.name;
            timeline.duration = src.duration;
            timeline.valueType = src.valueType;
            timeline.keys = src.keys;
            timeline.loop = loop;

            auto *transform = joint.transform.get();
            if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto t = transform->GetTranslate();
                    t.x = v;
                    transform->SetTranslate(t);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto t = transform->GetTranslate();
                    t.y = v;
                    transform->SetTranslate(t);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Translate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto t = transform->GetTranslate();
                    t.z = v;
                    transform->SetTranslate(t);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto s = transform->GetScale();
                    s.x = v;
                    transform->SetScale(s);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto s = transform->GetScale();
                    s.y = v;
                    transform->SetScale(s);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Scale.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto s = transform->GetScale();
                    s.z = v;
                    transform->SetScale(s);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.X")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto r = transform->GetRotate();
                    r.x = v;
                    transform->SetRotate(r);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Y")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto r = transform->GetRotate();
                    r.y = v;
                    transform->SetRotate(r);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Float && timeline.name.ends_with(".Rotate.Z")) {
                timeline.applyFunctions.push_back(std::function<void(float)>([transform](float v) {
                    if (!transform) return;
                    auto r = transform->GetRotate();
                    r.z = v;
                    transform->SetRotate(r);
                    }));
            } else if (timeline.valueType == KeyframeValueType::Quaternion && (timeline.name.ends_with(".Rotate") || timeline.name.ends_with(".RotateQuat"))) {
                timeline.applyFunctions.push_back(std::function<void(const Quaternion &)>([transform](const Quaternion &v) {
                    if (!transform) return;
                    transform->SetRotateQuaternion(v);
                    }));
            }

            if (timeline.applyFunctions.empty()) continue;

            SortTimelineKeys(timeline);
            timelines_.emplace(timeline.name, std::move(timeline));

            KeyframePlaybackState state;
            state.timelineName = src.name;
            state.elapsedTime = 0.0f;
            state.playing = true;
            state.paused = false;
            state.skeletonHandle = skeletonHandle;
            playbackStates_.emplace(state.timelineName, std::move(state));
        }
    }

    return !timelines_.empty();
}

KeyframeAnimator::KeyframeAnimator()
    : ISceneComponent("KeyframeAnimator", 1) {
}

void KeyframeAnimator::Initialize() {
    playbackStates_.clear();
}

void KeyframeAnimator::Finalize() {
    playbackStates_.clear();
    timelines_.clear();
}

void KeyframeAnimator::Update() {
    if (playbackStates_.empty()) return;

    const float dt = std::max(0.0f, GetDeltaTime()) * GetGameSpeed();
    for (auto &statePair : playbackStates_) {
        auto &state = statePair.second;
        if (!state.playing || state.paused) continue;

        auto timelineIt = timelines_.find(state.timelineName);
        if (timelineIt == timelines_.end()) continue;

        state.elapsedTime += dt;

        const auto &timeline = timelineIt->second;
        const auto value = EvaluateTimeline(timeline, state.elapsedTime);
        for (const auto &apply : timeline.applyFunctions) {
            if (const auto fn = std::get_if<std::function<void(float)>>(&apply)) {
                if (const auto v = std::get_if<float>(&value)) {
                    (*fn)(*v);
                }
                continue;
            }
            if (const auto fn = std::get_if<std::function<void(const Vector3 &)>>(&apply)) {
                if (const auto v = std::get_if<Vector3>(&value)) {
                    (*fn)(*v);
                }
                continue;
            }
            if (const auto fn = std::get_if<std::function<void(const Quaternion &)>>(&apply)) {
                if (const auto v = std::get_if<Quaternion>(&value)) {
                    (*fn)(*v);
                }
            }
        }

        if (state.skeletonHandle != 0) {
            SkeletonManager::UpdateSkeletonJointTransforms(state.skeletonHandle);
        }

        if (!timeline.loop) {
            const float lastKeyTime = timeline.keys.empty() ? 0.0f : timeline.keys.back().time;
            const float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
            if (state.elapsedTime >= endTime) {
                state.playing = false;
            }
        }
    }
}

bool KeyframeAnimator::AddTimeline(const std::string &timelineName) {
    if (timelineName.empty()) return false;
    if (timelines_.contains(timelineName)) return false;

    KeyframeTimeline timeline;
    timeline.name = timelineName;
    timeline.valueType = KeyframeValueType::Float;
    timelines_.emplace(timelineName, std::move(timeline));

    KeyframePlaybackState state;
    state.timelineName = timelineName;
    state.elapsedTime = 0.0f;
    state.playing = true;
    state.paused = false;
    playbackStates_.emplace(timelineName, std::move(state));
    return true;
}

bool KeyframeAnimator::AddTimeline(const KeyframeTimeline &timeline) {
    if (timeline.name.empty()) return false;
    if (timelines_.contains(timeline.name)) return false;

    KeyframeTimeline entry = timeline;
    if (entry.keys.empty()) {
        entry.valueType = KeyframeValueType::Float;
    }
    SortTimelineKeys(entry);
    timelines_.emplace(entry.name, std::move(entry));

    KeyframePlaybackState state;
    state.timelineName = timeline.name;
    state.elapsedTime = 0.0f;
    state.playing = true;
    state.paused = false;
    playbackStates_.emplace(timeline.name, std::move(state));
    return true;
}

bool KeyframeAnimator::RemoveTimeline(const std::string &timelineName) {
    if (timelineName.empty()) return false;
    const auto before = timelines_.size();
    timelines_.erase(timelineName);
    playbackStates_.erase(timelineName);
    return before != timelines_.size();
}

bool KeyframeAnimator::HasTimeline(const std::string &timelineName) const {
    return timelines_.contains(timelineName);
}

bool KeyframeAnimator::AddTimelineKey(const std::string &timelineName, float time, float value, EaseType easeType) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;

    if (it->second.valueType != KeyframeValueType::Float && !it->second.keys.empty()) return false;
    it->second.valueType = KeyframeValueType::Float;

    KeyframeNode key;
    key.time = std::max(0.0f, time);
    key.value = value;
    key.easeType = easeType;

    auto &keys = it->second.keys;
    auto insertPos = std::lower_bound(keys.begin(), keys.end(), key.time,
        [](const KeyframeNode &lhs, float rhsTime) {
            return lhs.time < rhsTime;
        });
    keys.insert(insertPos, key);

    if (it->second.duration < key.time) {
        it->second.duration = key.time;
    }

    return true;
}

bool KeyframeAnimator::UpdateTimelineKey(const std::string &timelineName, size_t keyIndex, float value) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    if (keyIndex >= it->second.keys.size()) return false;
    if (it->second.valueType != KeyframeValueType::Float) return false;

    it->second.keys[keyIndex].value = value;
    return true;
}

bool KeyframeAnimator::RemoveTimelineKey(const std::string &timelineName, size_t keyIndex) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    if (keyIndex >= it->second.keys.size()) return false;

    auto &keys = it->second.keys;
    keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(keyIndex));

    if (!keys.empty()) {
        const float maxKeyTime = keys.back().time;
        if (it->second.duration <= maxKeyTime) {
            it->second.duration = maxKeyTime;
        }
    } else if (it->second.duration <= 0.0f) {
        it->second.duration = 0.0f;
    }

    return true;
}

bool KeyframeAnimator::SetTimelineLoop(const std::string &timelineName, bool loop) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    it->second.loop = loop;
    return true;
}

bool KeyframeAnimator::IsTimelinePlaying(const std::string &timelineName) const {
    auto it = playbackStates_.find(timelineName);
    if (it == playbackStates_.end()) return false;
    return it->second.playing;
}

bool KeyframeAnimator::IsTimelinePaused(const std::string &timelineName) const {
    auto it = playbackStates_.find(timelineName);
    if (it == playbackStates_.end()) return false;
    return it->second.paused;
}

bool KeyframeAnimator::SaveSettings(const std::string &filePath) {
    const std::string normalizedPath = EnsureJsonExtension(filePath);
    if (normalizedPath.empty()) return false;

    JSON root = JSON::object();
    root["version"] = 1;

    JSON timelinesJson = JSON::object();
    for (const auto &pair : timelines_) {
        if (pair.second.valueType != KeyframeValueType::Float) {
            continue;
        }
        JSON t = JSON::object();
        t["duration"] = pair.second.duration;
        t["loop"] = pair.second.loop;
        JSON keys = JSON::array();
        for (const auto &k : pair.second.keys) {
            JSON key = JSON::object();
            key["time"] = k.time;
            key["value"] = std::get<float>(k.value);
            key["easeType"] = static_cast<int>(k.easeType);
            keys.push_back(std::move(key));
        }
        t["keys"] = std::move(keys);
        timelinesJson[pair.first] = std::move(t);
    }
    root["timelines"] = std::move(timelinesJson);

    return SaveJSON(root, normalizedPath, 4);
}

bool KeyframeAnimator::LoadSettings(const std::string &filePath) {
    const std::string normalizedPath = EnsureJsonExtension(filePath);
    if (normalizedPath.empty()) return false;

    JSON root = LoadJSON(normalizedPath);
    if (root.is_discarded() || !root.is_object()) return false;

    timelines_.clear();
    playbackStates_.clear();

    if (root.contains("timelines") && root["timelines"].is_object()) {
        for (auto it = root["timelines"].begin(); it != root["timelines"].end(); ++it) {
            const std::string timelineName = it.key();
            if (!it.value().is_object()) continue;

            KeyframeTimeline timeline;
            timeline.name = timelineName;
            timeline.duration = it.value().value("duration", 0.0f);
            timeline.loop = it.value().value("loop", false);
            timeline.valueType = KeyframeValueType::Float;

            if (it.value().contains("keys") && it.value()["keys"].is_array()) {
                for (const auto &k : it.value()["keys"]) {
                    KeyframeNode key;
                    key.time = std::max(0.0f, k.value("time", 0.0f));
                    key.value = k.value("value", 0.0f);
                    key.easeType = static_cast<EaseType>(k.value("easeType", static_cast<int>(EaseType::Linear)));
                    timeline.keys.push_back(key);
                }
            }

            SortTimelineKeys(timeline);
            timelines_.emplace(timelineName, std::move(timeline));

            KeyframePlaybackState state;
            state.timelineName = timelineName;
            state.elapsedTime = 0.0f;
            state.playing = true;
            state.paused = false;
            playbackStates_.emplace(timelineName, std::move(state));
        }
    }

    return true;
}

#ifdef USE_IMGUI
void KeyframeAnimator::ShowImGui() {}
#endif

} // namespace KashipanEngine
