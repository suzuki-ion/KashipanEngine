#include "Scene/Components/ModelAnimator.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>
#include <numbers>
#include <unordered_set>

#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {
static constexpr const char *kPropertyPaths[] = {
    "Transform.Translate.X",
    "Transform.Translate.Y",
    "Transform.Translate.Z",
    "Transform.Scale.X",
    "Transform.Scale.Y",
    "Transform.Scale.Z",
    "Transform.Rotate.X",
    "Transform.Rotate.Y",
    "Transform.Rotate.Z",
    "Material.Color.R",
    "Material.Color.G",
    "Material.Color.B",
    "Material.Color.A"
};

float DegreesToRadians(float degrees) {
    return degrees * (std::numbers::pi_v<float> / 180.0f);
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
}

ModelAnimator::ModelAnimator()
    : ISceneComponent("ModelAnimator", 1) {
}

void ModelAnimator::Initialize() {
    Stop();
}

void ModelAnimator::Finalize() {
    Stop();
    presets_.clear();
    timelines_.clear();
    presetBindings_.clear();
}

void ModelAnimator::Update() {
    if (playbacks_.empty()) return;

    const float dt = std::max(0.0f, GetDeltaTime());

    for (auto it = playbacks_.begin(); it != playbacks_.end();) {
        auto &playback = *it;
        if (!playback.paused) {
            playback.elapsedTime += dt;
        }

        auto presetIt = presets_.find(playback.presetName);
        auto bindingsIt = presetBindings_.find(playback.presetName);
        if (presetIt == presets_.end() || bindingsIt == presetBindings_.end()) {
            it = playbacks_.erase(it);
            continue;
        }

        for (const auto &binding : bindingsIt->second) {
            auto modelIt = playback.activeModels.find(binding.objectName);
            if (modelIt == playback.activeModels.end() || !modelIt->second) continue;

            auto timelineIt = timelines_.find(binding.timelineName);
            if (timelineIt == timelines_.end()) continue;

            const float value = EvaluateTimeline(timelineIt->second, playback.elapsedTime);
            if (binding.apply) {
                binding.apply(modelIt->second, value);
            }
        }

        if (HasAnyLoopingTimeline(bindingsIt->second)) {
            ++it;
            continue;
        }

        float maxDuration = 0.0f;
        for (const auto &binding : bindingsIt->second) {
            auto timelineIt = timelines_.find(binding.timelineName);
            if (timelineIt == timelines_.end() || timelineIt->second.keys.empty()) continue;
            maxDuration = std::max(maxDuration, timelineIt->second.keys.back().time);
        }

        if (playback.elapsedTime >= maxDuration) {
            it = playbacks_.erase(it);
        } else {
            ++it;
        }
    }
}

bool ModelAnimator::AddPresetObject(const std::string &presetName, const std::string &objectName, const std::string &parentObjectName) {
    if (presetName.empty() || objectName.empty()) return false;

    auto &preset = presets_[presetName];
    auto it = std::find_if(preset.begin(), preset.end(), [&](const PresetObject &entry) {
        return entry.objectName == objectName;
    });

    if (it != preset.end()) {
        it->parentObjectName = parentObjectName;
        return true;
    }

    preset.push_back(PresetObject{objectName, parentObjectName});
    return true;
}

bool ModelAnimator::RemovePresetObject(const std::string &presetName, const std::string &objectName) {
    auto presetIt = presets_.find(presetName);
    if (presetIt == presets_.end()) return false;

    auto &preset = presetIt->second;
    const auto before = preset.size();
    std::erase_if(preset, [&](const PresetObject &entry) {
        return entry.objectName == objectName;
    });

    return before != preset.size();
}

void ModelAnimator::ClearPreset(const std::string &presetName) {
    presets_.erase(presetName);
    presetBindings_.erase(presetName);
    Stop(presetName);
}

bool ModelAnimator::AddTimelineKey(const std::string &timelineName, float time, float value, EaseType easeType) {
    if (timelineName.empty()) return false;

    TimelineKey key;
    key.time = std::max(0.0f, time);
    key.value = value;
    key.easeType = easeType;

    auto &timeline = timelines_[timelineName];
    auto insertPos = std::lower_bound(timeline.keys.begin(), timeline.keys.end(), key.time,
        [](const TimelineKey &lhs, float rhsTime) {
            return lhs.time < rhsTime;
        });
    timeline.keys.insert(insertPos, key);

#if defined(USE_IMGUI)
    if (selectedTimelineName_ == timelineName) {
        RefreshTimelineEditorRange();
    }
#endif

    return true;
}

bool ModelAnimator::UpdateTimelineKey(const std::string &timelineName, size_t keyIndex, float time, float value, EaseType easeType) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    if (keyIndex >= it->second.keys.size()) return false;

    auto &keys = it->second.keys;
    keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(keyIndex));

    TimelineKey updated;
    updated.time = std::max(0.0f, time);
    updated.value = value;
    updated.easeType = easeType;

    auto insertPos = std::lower_bound(keys.begin(), keys.end(), updated.time,
        [](const TimelineKey &lhs, float rhsTime) {
            return lhs.time < rhsTime;
        });
    keys.insert(insertPos, updated);

#if defined(USE_IMGUI)
    if (selectedTimelineName_ == timelineName) {
        RefreshTimelineEditorRange();
    }
#endif

    return true;
}

bool ModelAnimator::RemoveTimelineKey(const std::string &timelineName, size_t keyIndex) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    if (keyIndex >= it->second.keys.size()) return false;

    it->second.keys.erase(it->second.keys.begin() + static_cast<std::ptrdiff_t>(keyIndex));

#if defined(USE_IMGUI)
    if (selectedTimelineName_ == timelineName) {
        RefreshTimelineEditorRange();
    }
#endif

    return true;
}

bool ModelAnimator::SetTimelineLoop(const std::string &timelineName, bool loop) {
    auto it = timelines_.find(timelineName);
    if (it == timelines_.end()) return false;
    it->second.loop = loop;
    return true;
}

void ModelAnimator::ClearTimeline(const std::string &timelineName) {
    timelines_.erase(timelineName);

#if defined(USE_IMGUI)
    if (selectedTimelineName_ == timelineName) {
        selectedTimelineName_.clear();
    }
#endif
}

bool ModelAnimator::AddBinding(const std::string &presetName, const std::string &objectName, const std::string &timelineName, ApplyFunction apply) {
    if (presetName.empty() || objectName.empty() || timelineName.empty() || !apply) return false;

    auto &bindings = presetBindings_[presetName];
    bindings.push_back(Binding{objectName, timelineName, std::move(apply), ""});
    return true;
}

bool ModelAnimator::AddBindingByPath(const std::string &presetName, const std::string &objectName, const std::string &timelineName, const std::string &propertyPath) {
    if (presetName.empty() || objectName.empty() || timelineName.empty() || propertyPath.empty()) return false;

    auto apply = MakeApplyFunction(propertyPath);
    if (!apply) return false;

    auto &bindings = presetBindings_[presetName];
    bindings.push_back(Binding{objectName, timelineName, std::move(apply), propertyPath});
    return true;
}

void ModelAnimator::ClearBindings(const std::string &presetName) {
    presetBindings_.erase(presetName);
}

bool ModelAnimator::Play(const std::string &presetName) {
    auto presetIt = presets_.find(presetName);
    if (presetIt == presets_.end()) return false;

    std::unordered_map<std::string, Model *> activeModels;
    if (!ResolveActiveModels(presetName, activeModels)) return false;

    ApplyPresetHierarchy(presetIt->second, activeModels);

    auto it = std::find_if(playbacks_.begin(), playbacks_.end(), [&](const PlaybackState &p) {
        return p.presetName == presetName;
    });

    if (it != playbacks_.end()) {
        it->elapsedTime = 0.0f;
        it->paused = false;
        it->activeModels = std::move(activeModels);
        return true;
    }

    PlaybackState state;
    state.presetName = presetName;
    state.elapsedTime = 0.0f;
    state.paused = false;
    state.activeModels = std::move(activeModels);
    playbacks_.push_back(std::move(state));
    return true;
}

void ModelAnimator::Stop() {
    playbacks_.clear();
}

bool ModelAnimator::Stop(const std::string &presetName) {
    const auto before = playbacks_.size();
    std::erase_if(playbacks_, [&](const PlaybackState &p) {
        return p.presetName == presetName;
    });
    return before != playbacks_.size();
}

void ModelAnimator::Pause() {
    for (auto &p : playbacks_) {
        p.paused = true;
    }
}

void ModelAnimator::Resume() {
    for (auto &p : playbacks_) {
        p.paused = false;
    }
}

bool ModelAnimator::IsPlaying() const {
    return !playbacks_.empty();
}

bool ModelAnimator::IsPaused() const {
    return std::any_of(playbacks_.begin(), playbacks_.end(), [](const PlaybackState &p) {
        return p.paused;
    });
}

bool ModelAnimator::SaveToJsonFile(const std::string &filePath) const {
    const std::string normalizedPath = EnsureJsonExtension(filePath);
    if (normalizedPath.empty()) return false;

    JSON root = JSON::object();
    root["version"] = 2;

    JSON presetsJson = JSON::object();
    for (const auto &presetPair : presets_) {
        JSON items = JSON::array();
        for (const auto &item : presetPair.second) {
            items.push_back({
                {"objectName", item.objectName},
                {"parentObjectName", item.parentObjectName}
            });
        }
        presetsJson[presetPair.first] = std::move(items);
    }
    root["presets"] = std::move(presetsJson);

    JSON timelinesJson = JSON::object();
    for (const auto &timelinePair : timelines_) {
        JSON t = JSON::object();
        t["loop"] = timelinePair.second.loop;
        JSON keys = JSON::array();
        for (const auto &k : timelinePair.second.keys) {
            keys.push_back({
                {"time", k.time},
                {"value", k.value},
                {"easeType", static_cast<int>(k.easeType)}
            });
        }
        t["keys"] = std::move(keys);
        timelinesJson[timelinePair.first] = std::move(t);
    }
    root["timelines"] = std::move(timelinesJson);

    JSON bindingsJson = JSON::object();
    for (const auto &bindPair : presetBindings_) {
        JSON items = JSON::array();
        for (const auto &b : bindPair.second) {
            if (b.propertyPath.empty()) continue;
            items.push_back({
                {"objectName", b.objectName},
                {"timelineName", b.timelineName},
                {"propertyPath", b.propertyPath}
            });
        }
        bindingsJson[bindPair.first] = std::move(items);
    }
    root["bindings"] = std::move(bindingsJson);

    return SaveJSON(root, normalizedPath, 4);
}

bool ModelAnimator::LoadFromJsonFile(const std::string &filePath) {
    const std::string normalizedPath = EnsureJsonExtension(filePath);
    if (normalizedPath.empty()) return false;

    JSON root = LoadJSON(normalizedPath);
    if (root.is_discarded() || !root.is_object()) return false;

    presets_.clear();
    timelines_.clear();
    presetBindings_.clear();

    if (root.contains("presets") && root["presets"].is_object()) {
        for (auto it = root["presets"].begin(); it != root["presets"].end(); ++it) {
            const std::string presetName = it.key();
            if (!it.value().is_array()) continue;
            for (const auto &entry : it.value()) {
                AddPresetObject(
                    presetName,
                    entry.value("objectName", ""),
                    entry.value("parentObjectName", ""));
            }
        }
    }

    if (root.contains("timelines") && root["timelines"].is_object()) {
        for (auto it = root["timelines"].begin(); it != root["timelines"].end(); ++it) {
            const std::string timelineName = it.key();
            if (!it.value().is_object()) continue;

            const bool loop = it.value().value("loop", false);
            if (it.value().contains("keys") && it.value()["keys"].is_array()) {
                for (const auto &k : it.value()["keys"]) {
                    AddTimelineKey(
                        timelineName,
                        k.value("time", 0.0f),
                        k.value("value", 0.0f),
                        static_cast<EaseType>(k.value("easeType", static_cast<int>(EaseType::Linear))));
                }
            }
            SetTimelineLoop(timelineName, loop);
        }
    }

    if (root.contains("bindings") && root["bindings"].is_object()) {
        for (auto it = root["bindings"].begin(); it != root["bindings"].end(); ++it) {
            const std::string presetName = it.key();
            if (!it.value().is_array()) continue;
            for (const auto &entry : it.value()) {
                AddBindingByPath(
                    presetName,
                    entry.value("objectName", ""),
                    entry.value("timelineName", ""),
                    entry.value("propertyPath", ""));
            }
        }
    }

    Stop();
    return true;
}

float ModelAnimator::EvaluateTimeline(const Timeline &timeline, float time) {
    if (timeline.keys.empty()) return 0.0f;
    if (timeline.keys.size() == 1) return timeline.keys.front().value;

    const float endTime = timeline.keys.back().time;
    float sampledTime = time;

    if (timeline.loop && endTime > 0.0f) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const TimelineKey &key) {
            return t < key.time;
        });

    if (upper == timeline.keys.begin()) return timeline.keys.front().value;
    if (upper == timeline.keys.end()) return timeline.keys.back().value;

    const auto &to = *upper;
    const auto &from = *(upper - 1);

    const float normalized = Normalize01(sampledTime, from.time, to.time);
    return Eased(from.value, to.value, normalized, from.easeType);
}

bool ModelAnimator::HasAnyLoopingTimeline(const std::vector<Binding> &bindings) const {
    for (const auto &binding : bindings) {
        auto it = timelines_.find(binding.timelineName);
        if (it != timelines_.end() && it->second.loop) {
            return true;
        }
    }
    return false;
}

bool ModelAnimator::ResolveActiveModels(const std::string &presetName, std::unordered_map<std::string, Model *> &outModels) const {
    outModels.clear();

    auto *ctx = GetOwnerContext();
    if (!ctx) return false;

    auto presetIt = presets_.find(presetName);
    if (presetIt == presets_.end()) return false;

    for (const auto &entry : presetIt->second) {
        auto *obj = ctx->GetObject3D(entry.objectName);
        if (!obj) continue;
        auto *model = dynamic_cast<Model *>(obj);
        if (!model) continue;
        outModels[entry.objectName] = model;
    }

    return true;
}

void ModelAnimator::ApplyPresetHierarchy(const std::vector<PresetObject> &preset, const std::unordered_map<std::string, Model *> &activeModels) {
    for (const auto &entry : preset) {
        if (entry.parentObjectName.empty()) continue;

        auto childIt = activeModels.find(entry.objectName);
        auto parentIt = activeModels.find(entry.parentObjectName);
        if (childIt == activeModels.end() || parentIt == activeModels.end()) continue;

        auto *childTr = childIt->second->GetComponent3D<Transform3D>();
        auto *parentTr = parentIt->second->GetComponent3D<Transform3D>();
        if (!childTr || !parentTr) continue;

        childTr->SetParentTransform(parentTr);
    }
}

bool ModelAnimator::RebuildBindingFunction(Binding &binding) {
    if (binding.propertyPath.empty()) return false;
    auto fn = MakeApplyFunction(binding.propertyPath);
    if (!fn) return false;
    binding.apply = std::move(fn);
    return true;
}

bool ModelAnimator::IsColorPath(const std::string &propertyPath) {
    return propertyPath == "Material.Color.R"
        || propertyPath == "Material.Color.G"
        || propertyPath == "Material.Color.B"
        || propertyPath == "Material.Color.A";
}

ModelAnimator::ApplyFunction ModelAnimator::MakeApplyFunction(const std::string &propertyPath) {
    if (propertyPath == "Transform.Translate.X") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto t = tr->GetTranslate();
            t.x = v;
            tr->SetTranslate(t);
        };
    }
    if (propertyPath == "Transform.Translate.Y") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto t = tr->GetTranslate();
            t.y = v;
            tr->SetTranslate(t);
        };
    }
    if (propertyPath == "Transform.Translate.Z") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto t = tr->GetTranslate();
            t.z = v;
            tr->SetTranslate(t);
        };
    }
    if (propertyPath == "Transform.Scale.X") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto sc = tr->GetScale();
            sc.x = v;
            tr->SetScale(sc);
        };
    }
    if (propertyPath == "Transform.Scale.Y") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto sc = tr->GetScale();
            sc.y = v;
            tr->SetScale(sc);
        };
    }
    if (propertyPath == "Transform.Scale.Z") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto sc = tr->GetScale();
            sc.z = v;
            tr->SetScale(sc);
        };
    }
    if (propertyPath == "Transform.Rotate.X") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto r = tr->GetRotate();
            r.x = DegreesToRadians(v);
            tr->SetRotate(r);
        };
    }
    if (propertyPath == "Transform.Rotate.Y") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto r = tr->GetRotate();
            r.y = DegreesToRadians(v);
            tr->SetRotate(r);
        };
    }
    if (propertyPath == "Transform.Rotate.Z") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *tr = s->GetComponent3D<Transform3D>();
            if (!tr) return;
            auto r = tr->GetRotate();
            r.z = DegreesToRadians(v);
            tr->SetRotate(r);
        };
    }
    if (propertyPath == "Material.Color.R") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *mat = s->GetComponent3D<Material3D>();
            if (!mat) return;
            auto c = mat->GetColor();
            c.x = std::clamp(v, 0.0f, 1.0f);
            mat->SetColor(c);
        };
    }
    if (propertyPath == "Material.Color.G") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *mat = s->GetComponent3D<Material3D>();
            if (!mat) return;
            auto c = mat->GetColor();
            c.y = std::clamp(v, 0.0f, 1.0f);
            mat->SetColor(c);
        };
    }
    if (propertyPath == "Material.Color.B") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *mat = s->GetComponent3D<Material3D>();
            if (!mat) return;
            auto c = mat->GetColor();
            c.z = std::clamp(v, 0.0f, 1.0f);
            mat->SetColor(c);
        };
    }
    if (propertyPath == "Material.Color.A") {
        return [](Model *s, float v) {
            if (!s) return;
            auto *mat = s->GetComponent3D<Material3D>();
            if (!mat) return;
            auto c = mat->GetColor();
            c.w = std::clamp(v, 0.0f, 1.0f);
            mat->SetColor(c);
        };
    }

    return {};
}

#if defined(USE_IMGUI)
const std::vector<const char *> &ModelAnimator::GetEaseTypeNames() {
    static const std::vector<const char *> kNames = {
        "Linear",
        "EaseInQuad", "EaseOutQuad", "EaseInOutQuad", "EaseOutInQuad",
        "EaseInCubic", "EaseOutCubic", "EaseInOutCubic", "EaseOutInCubic",
        "EaseInQuart", "EaseOutQuart", "EaseInOutQuart", "EaseOutInQuart",
        "EaseInQuint", "EaseOutQuint", "EaseInOutQuint", "EaseOutInQuint",
        "EaseInSine", "EaseOutSine", "EaseInOutSine", "EaseOutInSine",
        "EaseInExpo", "EaseOutExpo", "EaseInOutExpo", "EaseOutInExpo",
        "EaseInCirc", "EaseOutCirc", "EaseInOutCirc", "EaseOutInCirc",
        "EaseInBack", "EaseOutBack", "EaseInOutBack", "EaseOutInBack",
        "EaseInElastic", "EaseOutElastic", "EaseInOutElastic", "EaseOutInElastic",
        "EaseInBounce", "EaseOutBounce", "EaseInOutBounce", "EaseOutInBounce"
    };
    return kNames;
}

void ModelAnimator::RefreshTimelineEditorRange() {
    auto it = timelines_.find(selectedTimelineName_);
    if (it == timelines_.end() || it->second.keys.empty()) {
        timelineViewStartTime_ = 0.0f;
        timelineViewDuration_ = 5.0f;
        return;
    }

    if (timelineViewDuration_ <= 0.1f) {
        timelineViewDuration_ = 5.0f;
    }

    const float maxTime = std::max(0.0f, it->second.keys.back().time);
    const float maxStart = std::max(0.0f, maxTime - timelineViewDuration_ + 1.0f);
    timelineViewStartTime_ = std::clamp(timelineViewStartTime_, 0.0f, maxStart);
}

size_t ModelAnimator::FindNearestTimelineKey(const Timeline &timeline, float targetTime, float maxDistance) const {
    size_t best = static_cast<size_t>(-1);
    float bestDist = maxDistance;
    for (size_t i = 0; i < timeline.keys.size(); ++i) {
        const float d = std::abs(timeline.keys[i].time - targetTime);
        if (d <= bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void ModelAnimator::ShowImGui() {
    ShowImGuiWindowPresets();
    ShowImGuiWindowHierarchy();
    ShowImGuiWindowTimelines();
    ShowImGuiWindowTimelineEditor();
    ShowImGuiWindowBindings();
    ShowImGuiWindowPlayers();
    ShowImGuiWindowStorage();
}

void ModelAnimator::ShowImGuiWindowPresets() {
    if (!ImGui::Begin("ModelAnimator - Presets")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("PresetName", presetNameBuffer_.data(), presetNameBuffer_.size());
    ImGui::InputText("ObjectName", objectNameBuffer_.data(), objectNameBuffer_.size());
    ImGui::InputText("ParentObject", parentObjectNameBuffer_.data(), parentObjectNameBuffer_.size());

    if (ImGui::Button("Add/Update Object")) {
        AddPresetObject(
            presetNameBuffer_.data(),
            objectNameBuffer_.data(),
            parentObjectNameBuffer_.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Object")) {
        RemovePresetObject(presetNameBuffer_.data(), objectNameBuffer_.data());
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Preset")) {
        ClearPreset(presetNameBuffer_.data());
    }

    ImGui::Separator();
    ImGui::Text("Presets: %d", static_cast<int>(presets_.size()));

    std::string removePresetObject;
    for (const auto &pair : presets_) {
        const bool selected = (selectedPresetName_ == pair.first);
        if (ImGui::Selectable(pair.first.c_str(), selected)) {
            selectedPresetName_ = pair.first;
            std::snprintf(presetNameBuffer_.data(), presetNameBuffer_.size(), "%s", pair.first.c_str());
        }

        if (selected) {
            ImGui::Indent();
            for (const auto &obj : pair.second) {
                ImGui::PushID(obj.objectName.c_str());
                ImGui::BulletText(
                    "%s  (parent: %s)",
                    obj.objectName.c_str(),
                    obj.parentObjectName.empty() ? "<none>" : obj.parentObjectName.c_str());

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    std::snprintf(objectNameBuffer_.data(), objectNameBuffer_.size(), "%s", obj.objectName.c_str());
                    std::snprintf(parentObjectNameBuffer_.data(), parentObjectNameBuffer_.size(), "%s", obj.parentObjectName.c_str());
                }

                if (ImGui::BeginPopupContextItem("PresetObjectCtx")) {
                    if (ImGui::MenuItem("Delete")) {
                        removePresetObject = obj.objectName;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
    }

    if (!removePresetObject.empty() && !selectedPresetName_.empty()) {
        RemovePresetObject(selectedPresetName_, removePresetObject);
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowHierarchy() {
    if (!ImGui::Begin("ModelAnimator - Preset Hierarchy")) {
        ImGui::End();
        return;
    }

    if (selectedPresetName_.empty()) {
        ImGui::TextUnformatted("Select preset in Presets window.");
        ImGui::End();
        return;
    }

    auto presetIt = presets_.find(selectedPresetName_);
    if (presetIt == presets_.end()) {
        ImGui::TextUnformatted("Selected preset not found.");
        ImGui::End();
        return;
    }

    const auto &preset = presetIt->second;
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_set<std::string> existing;
    for (const auto &p : preset) {
        existing.insert(p.objectName);
    }

    for (const auto &p : preset) {
        children[p.parentObjectName].push_back(p.objectName);
    }

    std::function<void(const std::string &)> drawNode = [&](const std::string &name) {
        if (ImGui::TreeNode(name.c_str())) {
            auto it = children.find(name);
            if (it != children.end()) {
                for (const auto &child : it->second) {
                    drawNode(child);
                }
            }
            ImGui::TreePop();
        }
    };

    auto rootIt = children.find("");
    if (rootIt != children.end()) {
        for (const auto &root : rootIt->second) {
            drawNode(root);
        }
    }

    for (const auto &p : preset) {
        if (!p.parentObjectName.empty() && !existing.contains(p.parentObjectName)) {
            ImGui::Text("[Missing Parent: %s]", p.parentObjectName.c_str());
            drawNode(p.objectName);
        }
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowTimelines() {
    if (!ImGui::Begin("ModelAnimator - Timelines")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("TimelineName", timelineNameBuffer_.data(), timelineNameBuffer_.size());
    ImGui::Checkbox("Loop", &imguiTimelineLoop_);
    
    if (ImGui::Button("Add Timeline")) {
        if (!timelineNameBuffer_[0]) {
            ImGui::OpenPopup("InvalidTimelineName");
        } else if (timelines_.contains(timelineNameBuffer_.data())) {
            ImGui::OpenPopup("TimelineExists");
        } else {
            AddTimelineKey(timelineNameBuffer_.data(), 0.0f, 0.0f);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Apply Loop To Timeline")) {
        SetTimelineLoop(timelineNameBuffer_.data(), imguiTimelineLoop_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Timeline")) {
        ClearTimeline(timelineNameBuffer_.data());
    }

    ImGui::Separator();
    ImGui::Text("Timelines: %d", static_cast<int>(timelines_.size()));

    for (auto &pair : timelines_) {
        const bool selected = (selectedTimelineName_ == pair.first);
        if (ImGui::Selectable(pair.first.c_str(), selected)) {
            selectedTimelineName_ = pair.first;
            std::snprintf(timelineNameBuffer_.data(), timelineNameBuffer_.size(), "%s", pair.first.c_str());
            imguiTimelineLoop_ = pair.second.loop;
            selectedTimelineKeyIndex_ = -1;
            RefreshTimelineEditorRange();
        }

        if (selected) {
            ImGui::Indent();
            ImGui::Text("Keys: %d", static_cast<int>(pair.second.keys.size()));
            ImGui::Text("Loop: %s", pair.second.loop ? "true" : "false");
            ImGui::Unindent();
        }
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowTimelineEditor() {
    if (!ImGui::Begin("ModelAnimator - Timeline Editor")) {
        ImGui::End();
        return;
    }

    if (selectedTimelineName_.empty()) {
        ImGui::TextUnformatted("Select timeline in Timelines window.");
        ImGui::End();
        return;
    }

    auto timelineIt = timelines_.find(selectedTimelineName_);
    if (timelineIt == timelines_.end()) {
        ImGui::TextUnformatted("Selected timeline not found.");
        ImGui::End();
        return;
    }

    auto &timeline = timelineIt->second;
    RefreshTimelineEditorRange();

    ImGui::DragFloat("Visible Duration", &timelineViewDuration_, 0.05f, 0.5f, 60.0f, "%.2f sec");

    const ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 180.0f);
    ImGui::InvisibleButton("TimelineCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();

    const float visibleStart = timelineViewStartTime_;
    const float visibleEnd = timelineViewStartTime_ + timelineViewDuration_;

    auto toScreenX = [&](float t) {
        const float n = (t - visibleStart) / std::max(0.0001f, timelineViewDuration_);
        return p0.x + std::clamp(n, 0.0f, 1.0f) * (p1.x - p0.x);
    };

    auto toTime = [&](float x) {
        const float n = std::clamp((x - p0.x) / std::max(1.0f, p1.x - p0.x), 0.0f, 1.0f);
        return visibleStart + n * timelineViewDuration_;
    };

    draw->AddRectFilled(p0, p1, IM_COL32(28, 28, 34, 255));
    draw->AddRect(p0, p1, IM_COL32(120, 120, 140, 255));

    const float rulerH = 24.0f;
    draw->AddRectFilled(p0, ImVec2(p1.x, p0.y + rulerH), IM_COL32(36, 36, 44, 255));
    draw->AddLine(ImVec2(p0.x, p0.y + rulerH), ImVec2(p1.x, p0.y + rulerH), IM_COL32(90, 90, 110, 255));

    const int tickCount = 10;
    for (int i = 0; i <= tickCount; ++i) {
        const float t = visibleStart + (timelineViewDuration_ * static_cast<float>(i) / static_cast<float>(tickCount));
        const float x = toScreenX(t);
        draw->AddLine(ImVec2(x, p0.y + rulerH - 8.0f), ImVec2(x, p1.y), IM_COL32(60, 60, 70, 255));

        char label[32];
        std::snprintf(label, sizeof(label), "%.2f", t);
        draw->AddText(ImVec2(x + 2.0f, p0.y + 2.0f), IM_COL32(210, 210, 220, 255), label);
    }

    for (size_t i = 0; i < timeline.keys.size(); ++i) {
        const auto &k = timeline.keys[i];
        if (k.time < visibleStart || k.time > visibleEnd) continue;

        const float x = toScreenX(k.time);
        const bool selected = (selectedTimelineKeyIndex_ == static_cast<int>(i));
        draw->AddLine(ImVec2(x, p0.y + rulerH), ImVec2(x, p1.y), IM_COL32(100, 180, 255, 255));
        draw->AddCircleFilled(
            ImVec2(x, p0.y + rulerH + (p1.y - (p0.y + rulerH)) * 0.5f),
            selected ? 6.0f : 5.0f,
            selected ? IM_COL32(255, 140, 120, 255) : IM_COL32(255, 220, 120, 255));
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        timelineViewStartTime_ -= ImGui::GetIO().MouseWheel * (timelineViewDuration_ * 0.12f);
        timelineViewStartTime_ = std::max(0.0f, timelineViewStartTime_);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float t = toTime(mouse.x);
        const float timeThreshold = timelineViewDuration_ * 0.01f;
        const size_t nearest = FindNearestTimelineKey(timeline, t, timeThreshold);
        if (nearest == static_cast<size_t>(-1)) {
            AddTimelineKey(selectedTimelineName_, t, 0.0f, EaseType::Linear);
            selectedTimelineKeyIndex_ = static_cast<int>(FindNearestTimelineKey(timeline, t, timeThreshold * 2.0f));
        } else {
            selectedTimelineKeyIndex_ = static_cast<int>(nearest);
        }
        timelineDragKeyIndex_ = selectedTimelineKeyIndex_;
    }

    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && timelineDragKeyIndex_ >= 0 && timelineDragKeyIndex_ < static_cast<int>(timeline.keys.size())) {
        const float t = toTime(mouse.x);
        const auto old = timeline.keys[static_cast<size_t>(timelineDragKeyIndex_)];
        UpdateTimelineKey(selectedTimelineName_, static_cast<size_t>(timelineDragKeyIndex_), t, old.value, old.easeType);
        selectedTimelineKeyIndex_ = static_cast<int>(FindNearestTimelineKey(timeline, t, timelineViewDuration_ * 0.02f));
        timelineDragKeyIndex_ = selectedTimelineKeyIndex_;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        timelineDragKeyIndex_ = -1;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        const float t = toTime(mouse.x);
        const size_t nearest = FindNearestTimelineKey(timeline, t, timelineViewDuration_ * 0.02f);
        if (nearest != static_cast<size_t>(-1)) {
            RemoveTimelineKey(selectedTimelineName_, nearest);
            if (selectedTimelineKeyIndex_ == static_cast<int>(nearest)) {
                selectedTimelineKeyIndex_ = -1;
            }
        }
    }

    ImGui::Separator();
    if (selectedTimelineKeyIndex_ >= 0 && selectedTimelineKeyIndex_ < static_cast<int>(timeline.keys.size())) {
        auto k = timeline.keys[static_cast<size_t>(selectedTimelineKeyIndex_)];
        ImGui::Text("Selected Key: #%d", selectedTimelineKeyIndex_);

        bool changed = false;
        changed |= ImGui::DragFloat("Time", &k.time, 0.01f, 0.0f, 9999.0f);
        changed |= ImGui::DragFloat("Value", &k.value, IsColorPath(kPropertyPaths[imguiPropertyPathIndex_]) ? 0.005f : 0.01f,
            IsColorPath(kPropertyPaths[imguiPropertyPathIndex_]) ? 0.0f : -10000.0f,
            IsColorPath(kPropertyPaths[imguiPropertyPathIndex_]) ? 1.0f : 10000.0f);

        int ease = static_cast<int>(k.easeType);
        changed |= ImGui::Combo("Ease", &ease, GetEaseTypeNames().data(), static_cast<int>(GetEaseTypeNames().size()));

        if (changed) {
            UpdateTimelineKey(selectedTimelineName_, static_cast<size_t>(selectedTimelineKeyIndex_), k.time, k.value, static_cast<EaseType>(ease));
        }

        if (ImGui::Button("Delete Selected Key")) {
            RemoveTimelineKey(selectedTimelineName_, static_cast<size_t>(selectedTimelineKeyIndex_));
            selectedTimelineKeyIndex_ = -1;
        }
    } else {
        ImGui::TextUnformatted("Click key to edit. Click empty area to add key.");
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowBindings() {
    if (!ImGui::Begin("ModelAnimator - Bindings")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("BindingPreset", bindingPresetBuffer_.data(), bindingPresetBuffer_.size());
    ImGui::InputText("BindingObject", bindingObjectBuffer_.data(), bindingObjectBuffer_.size());
    ImGui::InputText("BindingTimeline", bindingTimelineBuffer_.data(), bindingTimelineBuffer_.size());

    imguiPropertyPathIndex_ = std::clamp(imguiPropertyPathIndex_, 0, static_cast<int>(std::size(kPropertyPaths)) - 1);
    ImGui::Combo("PropertyPath", &imguiPropertyPathIndex_, kPropertyPaths, static_cast<int>(std::size(kPropertyPaths)));

    if (ImGui::Button("Add Binding")) {
        AddBindingByPath(bindingPresetBuffer_.data(), bindingObjectBuffer_.data(), bindingTimelineBuffer_.data(), kPropertyPaths[imguiPropertyPathIndex_]);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Preset Bindings")) {
        ClearBindings(bindingPresetBuffer_.data());
    }

    ImGui::Separator();
    ImGui::Text("Binding Map");

    int displayIndex = 0;
    std::optional<std::pair<std::string, size_t>> removeBinding;
    for (auto &pair : presetBindings_) {
        if (!ImGui::TreeNode(pair.first.c_str())) continue;

        for (size_t i = 0; i < pair.second.size(); ++i) {
            auto &b = pair.second[i];
            const int currentIndex = displayIndex++;
            ImGui::PushID(currentIndex);

            const std::string label = b.objectName + " :: " + b.propertyPath + " <= " + b.timelineName;
            const bool selected = (selectedBindingIndex_ == currentIndex);
            if (ImGui::Selectable(label.c_str(), selected)) {
                selectedBindingIndex_ = currentIndex;
                std::snprintf(bindingPresetBuffer_.data(), bindingPresetBuffer_.size(), "%s", pair.first.c_str());
                std::snprintf(bindingObjectBuffer_.data(), bindingObjectBuffer_.size(), "%s", b.objectName.c_str());
                std::snprintf(bindingTimelineBuffer_.data(), bindingTimelineBuffer_.size(), "%s", b.timelineName.c_str());
                for (int p = 0; p < static_cast<int>(std::size(kPropertyPaths)); ++p) {
                    if (b.propertyPath == kPropertyPaths[p]) {
                        imguiPropertyPathIndex_ = p;
                        break;
                    }
                }
            }

            if (ImGui::BeginPopupContextItem("BindingCtx")) {
                if (ImGui::MenuItem("Delete")) {
                    removeBinding = std::make_pair(pair.first, i);
                }
                ImGui::EndPopup();
            }

            if (selected) {
                ImGui::Indent();
                if (ImGui::Button("Apply Edit")) {
                    b.objectName = bindingObjectBuffer_.data();
                    b.timelineName = bindingTimelineBuffer_.data();
                    b.propertyPath = kPropertyPaths[imguiPropertyPathIndex_];
                    RebuildBindingFunction(b);
                }
                ImGui::Unindent();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if (removeBinding.has_value()) {
        auto it = presetBindings_.find(removeBinding->first);
        if (it != presetBindings_.end() && removeBinding->second < it->second.size()) {
            it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(removeBinding->second));
        }
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowPlayers() {
    if (!ImGui::Begin("ModelAnimator - Playback")) {
        ImGui::End();
        return;
    }

    ImGui::InputText("PlayPreset", playPresetBuffer_.data(), playPresetBuffer_.size());
    if (ImGui::Button("Play")) {
        Play(playPresetBuffer_.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop All")) {
        Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause All")) {
        Pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Resume All")) {
        Resume();
    }

    ImGui::Separator();
    ImGui::Text("Active Playbacks: %d", static_cast<int>(playbacks_.size()));

    for (auto &p : playbacks_) {
        ImGui::PushID(p.presetName.c_str());
        ImGui::Text("%s  time=%.3f  state=%s", p.presetName.c_str(), p.elapsedTime, p.paused ? "Paused" : "Playing");
        ImGui::SameLine();
        if (ImGui::SmallButton("Stop")) {
            Stop(p.presetName);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::InputText("JsonPath", jsonPathBuffer_.data(), jsonPathBuffer_.size());
    if (ImGui::Button("Load JSON")) {
        LoadFromJsonFile(jsonPathBuffer_.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Save JSON")) {
        SaveToJsonFile(jsonPathBuffer_.data());
    }

    ImGui::End();
}

void ModelAnimator::ShowImGuiWindowStorage() {
    if (!ImGui::Begin("ModelAnimator - Storage")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Preset count: %d", static_cast<int>(presets_.size()));
    ImGui::Text("Timeline count: %d", static_cast<int>(timelines_.size()));
    ImGui::Text("Binding preset count: %d", static_cast<int>(presetBindings_.size()));

    int totalObjects = 0;
    for (const auto &p : presets_) totalObjects += static_cast<int>(p.second.size());
    ImGui::Text("Total preset objects: %d", totalObjects);

    int totalKeys = 0;
    for (const auto &t : timelines_) totalKeys += static_cast<int>(t.second.keys.size());
    ImGui::Text("Total timeline keys: %d", totalKeys);

    int totalBindings = 0;
    for (const auto &b : presetBindings_) totalBindings += static_cast<int>(b.second.size());
    ImGui::Text("Total bindings: %d", totalBindings);

    ImGui::Separator();
    ImGui::Text("Property Paths (%d)", static_cast<int>(std::size(kPropertyPaths)));
    for (const auto *p : kPropertyPaths) {
        ImGui::TextUnformatted(p);
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
