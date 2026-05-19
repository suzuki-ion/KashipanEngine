#include "KeyframeAnimator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

namespace {
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

float EvaluateTimeline(const KeyframeTimeline &timeline, float time) {
    if (timeline.keys.empty()) return 0.0f;
    if (timeline.keys.size() == 1) return timeline.keys.front().value;

    const float lastKeyTime = timeline.keys.back().time;
    float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return timeline.keys.back().value;

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

    if (upper == timeline.keys.begin()) return timeline.keys.front().value;
    if (upper == timeline.keys.end()) return timeline.keys.back().value;

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float normalized = Normalize01(sampledTime, from.time, to.time);
    return Eased(from.value, to.value, normalized, from.easeType);
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
        const float value = EvaluateTimeline(timeline, state.elapsedTime);
        for (const auto &apply : timeline.applyFunctions) {
            if (apply) {
                apply(value);
            }
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
        JSON t = JSON::object();
        t["duration"] = pair.second.duration;
        t["loop"] = pair.second.loop;
        JSON keys = JSON::array();
        for (const auto &k : pair.second.keys) {
            keys.push_back({
                {"time", k.time},
                {"value", k.value},
                {"easeType", static_cast<int>(k.easeType)}
            });
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
