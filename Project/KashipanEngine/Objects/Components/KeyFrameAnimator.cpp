#include "Objects/Components/KeyFrameAnimator.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Debug/Logger.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/ObjectContext.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

std::unique_ptr<IObjectComponent> KeyFrameAnimator::Clone() const {
    auto ptr = std::make_unique<KeyFrameAnimator>();
    ptr->animations_.reserve(animations_.size());
    for (const auto &entry : animations_) {
        AnimationEntry copy;
        copy.name = entry.name;
        copy.jsonPath = entry.jsonPath;
        copy.playbackSpeed = entry.playbackSpeed;
        copy.timeOffset = entry.timeOffset;
        copy.valueScale = entry.valueScale;
        copy.valueOffset = entry.valueOffset;
        copy.loop = entry.loop;
        copy.playOnStart = entry.playOnStart;
        copy.bindings = entry.bindings;
        ptr->animations_.push_back(std::move(copy));
    }
    return ptr;
}

//==================================================
// 再生制御
//==================================================

bool KeyFrameAnimator::Play(const std::string &name) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->currentTime = 0.0f;
    entry->playing = true;
    return true;
}

bool KeyFrameAnimator::Stop(const std::string &name) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->playing = false;
    return true;
}

void KeyFrameAnimator::PlayAll() {
    for (auto &entry : animations_) {
        entry.currentTime = 0.0f;
        entry.playing = true;
    }
}

void KeyFrameAnimator::StopAll() {
    for (auto &entry : animations_) {
        entry.playing = false;
    }
}

bool KeyFrameAnimator::IsPlaying(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->playing;
}

bool KeyFrameAnimator::SetPlaybackSpeed(const std::string &name, float speed) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->playbackSpeed = speed;
    return true;
}

float KeyFrameAnimator::GetPlaybackSpeed(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->playbackSpeed : 0.0f;
}

bool KeyFrameAnimator::TryGetValue(const std::string &name, float &outValue) const {
    const auto *entry = FindAnimation(name);
    if (!entry) return false;
    outValue = entry->lastValue;
    return true;
}

//==================================================
// ライフサイクル
//==================================================

void KeyFrameAnimator::Initialize() {
    // playOnStartの適用は最初のUpdate時に行う
    // （シーン読み込み時はInitialize後にLoadFromJsonが走るため、この時点ではエントリが無い場合がある）
    applyPlayOnStart_ = true;
    for (auto &entry : animations_) {
        entry.playing = false;
        entry.currentTime = 0.0f;
    }
}

void KeyFrameAnimator::Update() {
    if (applyPlayOnStart_) {
        applyPlayOnStart_ = false;
        for (auto &entry : animations_) {
            if (entry.playOnStart) {
                entry.currentTime = 0.0f;
                entry.playing = true;
            }
        }
    }

    const float deltaTime = GetDeltaTime();
    for (auto &entry : animations_) {
        if (!entry.playing) continue;
        EnsureLoaded(entry);
        if (!entry.loaded) continue;

        entry.currentTime += deltaTime * entry.playbackSpeed;
        const float duration = entry.animation.GetDuration();
        if (entry.loop) {
            // ループ時は再生位置を[0, duration)へ巻き戻す（逆再生にも対応）
            if (duration > 0.0f) {
                entry.currentTime = std::fmod(entry.currentTime, duration);
                if (entry.currentTime < 0.0f) entry.currentTime += duration;
            }
        } else if (entry.playbackSpeed >= 0.0f ? (entry.currentTime >= duration) : (entry.currentTime <= 0.0f)) {
            // ループ無しの場合は末端（逆再生時は先頭）に到達したら自動停止する
            entry.currentTime = (entry.playbackSpeed >= 0.0f) ? duration : 0.0f;
            entry.playing = false;
        }

        float evalTime = entry.currentTime + entry.timeOffset;
        if (entry.loop && duration > 0.0f) {
            evalTime = std::fmod(evalTime, duration);
            if (evalTime < 0.0f) evalTime += duration;
        }
        const float value = entry.animation.Evaluate(evalTime) * entry.valueScale + entry.valueOffset;
        entry.lastValue = value;
        ApplyValue(entry, value);
    }
}

//==================================================
// 内部処理
//==================================================

KeyFrameAnimator::AnimationEntry *KeyFrameAnimator::FindAnimation(const std::string &name) {
    for (auto &entry : animations_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

const KeyFrameAnimator::AnimationEntry *KeyFrameAnimator::FindAnimation(const std::string &name) const {
    for (const auto &entry : animations_) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

void KeyFrameAnimator::EnsureLoaded(AnimationEntry &entry) {
    if (entry.loaded || entry.loadFailed || entry.jsonPath.empty()) return;
    const JSON json = LoadJSON(entry.jsonPath);
    if (json.is_discarded() || !entry.animation.LoadFromJson(json)) {
        entry.loadFailed = true;
        Log(Translation("engine.keyframeanimator.load.failed") + entry.jsonPath, LogSeverity::Warning);
        return;
    }
    entry.loaded = true;
}

void KeyFrameAnimator::ApplyValue(const AnimationEntry &entry, float value) {
    auto *objectContext = GetOwnerObjectContext();
    if (!objectContext) return;
    for (const auto &binding : entry.bindings) {
        ApplyParameterBinding(objectContext, binding, value, this);
    }
}

//==================================================
// ImGui
//==================================================

#if defined(USE_IMGUI)

void KeyFrameAnimator::ShowImGui() {
    if (ImGui::Button(TranslationLabel("component.keyframeanimator.add_animation"))) {
        AnimationEntry entry;
        int number = 0;
        std::string newName = "Anim0";
        while (FindAnimation(newName)) {
            ++number;
            newName = "Anim" + std::to_string(number);
        }
        entry.name = newName;
        animations_.push_back(std::move(entry));
    }

    const auto candidates = CollectParameterBindingCandidates(GetOwnerObjectContext(), this);
    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(animations_.size()); ++i) {
        auto &entry = animations_[i];
        ImGui::PushID(i);
        // 名前変更でヘッダーの開閉状態が変わらないよう、IDは"###"でインデックス固定にする
        if (ImGui::CollapsingHeader((entry.name + "###animEntry").c_str())) {
            ShowAnimationImGui(entry, candidates);
            if (ImGui::SmallButton(TranslationLabel("component.keyframeanimator.delete_animation"))) {
                removeIndex = i;
            }
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0) {
        animations_.erase(animations_.begin() + removeIndex);
    }
}

void KeyFrameAnimator::ShowAnimationImGui(AnimationEntry &entry, const std::vector<ParameterBindingCandidate> &candidates) {
    ImGui::InputText(TranslationLabel("component.keyframeanimator.name"), &entry.name);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Play/Stop等で指定する識別名");
    }
    ImGui::InputText(TranslationLabel("component.keyframeanimator.json_path"), &entry.jsonPath);
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("component.keyframeanimator.reload"))) {
        entry.loaded = false;
        entry.loadFailed = false;
        entry.animation.Clear();
        EnsureLoaded(entry);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "キーフレームjsonのパス（Keyframe Animation Editorで作成できる）。Reloadで読み込む");
    }
    if (!entry.loaded && !entry.loadFailed && !entry.jsonPath.empty()) {
        EnsureLoaded(entry);
    }
    if (entry.loaded) {
        ImGui::Text(TranslationC("component.keyframeanimator.keys_d_duration_2fs"), static_cast<int>(entry.animation.GetKeyframeCount()), entry.animation.GetDuration());
    } else if (entry.loadFailed) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", TranslationC("component.keyframeanimator.loadfailed"));
    } else {
        ImGui::TextDisabled("%s", TranslationC("component.keyframeanimator.desc_1"));
    }

    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.playback_speed"), &entry.playbackSpeed, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "再生速度の倍率（負の値で逆再生）");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.time_offset"), &entry.timeOffset, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "評価時刻へ加算する再生時オフセット（秒）。同じアニメを複数オブジェクトで位相をずらして再生する用途など");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.value_scale"), &entry.valueScale, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "評価値にかけるスケール。Value Offsetの加算より先に適用される（適用値 = 評価値 × Scale + Offset）");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.value_offset"), &entry.valueOffset, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "評価値へ加算するデフォルト値オフセット。適用先ごとの基準値の違いを吸収する");
    }
    ImGui::Checkbox(TranslationLabel("component.keyframeanimator.loop"), &entry.loop);
    ImGui::SameLine();
    ImGui::Checkbox(TranslationLabel("component.keyframeanimator.play_on_start"), &entry.playOnStart);

    if (entry.playing) {
        if (ImGui::Button(TranslationLabel("component.keyframeanimator.stop"))) {
            entry.playing = false;
        }
        ImGui::SameLine();
        ImGui::Text(TranslationC("component.keyframeanimator.time_2fs_value_3f"), entry.currentTime, entry.lastValue);
    } else {
        if (ImGui::Button(TranslationLabel("component.keyframeanimator.play"))) {
            entry.currentTime = 0.0f;
            entry.playing = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "再生を開始する（値の適用はゲームループ実行中のみ行われる）");
        }
    }

    // 適用先バインディング
    ImGui::Separator();
    ShowParameterBindingListImGui(entry.bindings, candidates);
    ImGui::Separator();
}

#endif // USE_IMGUI

//==================================================
// シリアライズ
//==================================================

JSON KeyFrameAnimator::SaveToJson() const {
    JSON json = JSON::object();
    JSON animationsJson = JSON::array();
    for (const auto &entry : animations_) {
        JSON entryJson = JSON::object();
        entryJson["name"] = entry.name;
        entryJson["jsonPath"] = entry.jsonPath;
        entryJson["playbackSpeed"] = entry.playbackSpeed;
        entryJson["timeOffset"] = entry.timeOffset;
        entryJson["valueScale"] = entry.valueScale;
        entryJson["valueOffset"] = entry.valueOffset;
        entryJson["loop"] = entry.loop;
        entryJson["playOnStart"] = entry.playOnStart;
        JSON bindingsJson = JSON::array();
        for (const auto &binding : entry.bindings) {
            bindingsJson.push_back(SaveParameterBindingToJson(binding));
        }
        entryJson["bindings"] = std::move(bindingsJson);
        animationsJson.push_back(std::move(entryJson));
    }
    json["animations"] = std::move(animationsJson);
    return json;
}

bool KeyFrameAnimator::LoadFromJson(const JSON &json) {
    animations_.clear();
    if (!json.contains("animations") || !json["animations"].is_array()) {
        return true;
    }
    for (const auto &entryJson : json["animations"]) {
        if (!entryJson.is_object()) continue;
        AnimationEntry entry;
        entry.name = entryJson.value("name", std::string{});
        entry.jsonPath = entryJson.value("jsonPath", std::string{});
        entry.playbackSpeed = entryJson.value("playbackSpeed", 1.0f);
        entry.timeOffset = entryJson.value("timeOffset", 0.0f);
        entry.valueScale = entryJson.value("valueScale", 1.0f);
        entry.valueOffset = entryJson.value("valueOffset", 0.0f);
        entry.loop = entryJson.value("loop", true);
        entry.playOnStart = entryJson.value("playOnStart", false);
        if (entryJson.contains("bindings") && entryJson["bindings"].is_array()) {
            for (const auto &bindingJson : entryJson["bindings"]) {
                if (!bindingJson.is_object()) continue;
                entry.bindings.push_back(LoadParameterBindingFromJson(bindingJson));
            }
        }
        animations_.push_back(std::move(entry));
    }
    return true;
}

} // namespace KashipanEngine
