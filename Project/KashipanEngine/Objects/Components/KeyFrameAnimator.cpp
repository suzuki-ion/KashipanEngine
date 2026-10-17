#include "Objects/Components/KeyFrameAnimator.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Objects/Components/ScriptComponent.h"
#include "Objects/ObjectContext.h"
#include "Scene/SceneContext.h"
#include "Utilities/FileIO/TextFile.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {
namespace {
using Keyframe = KeyframeAnimation::Keyframe;
/// @brief 短文の回転（度で保持したキー）を内部単位のラジアンへ変換する valueScale
constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
} // namespace

float KeyFrameAnimator::GetAnimationTime(const std::string &name) const {
    const auto *entry = FindAnimation(name); return entry ? entry->currentTime : 0.0f;
}
bool KeyFrameAnimator::SetCurrentTime(const std::string &name, float time) {
    auto *entry = FindAnimation(name);
    if (!entry || !std::isfinite(time) || time < 0) return false;
    EnsureLoaded(*entry); if (!entry->loaded || entry->loadFailed) return false;
    entry->currentTime = std::min(time, entry->animation.GetDuration());
    ApplyValue(*entry, entry->animation.Evaluate(entry->currentTime + entry->timeOffset) * entry->valueScale + entry->valueOffset);
    return true;
}
float KeyFrameAnimator::GetDuration(const std::string &name) const {
    const auto *entry = FindAnimation(name); return entry && entry->loaded ? entry->animation.GetDuration() : 0.0f;
}

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
        copy.embedded = entry.embedded;
        copy.shortTextManaged = entry.shortTextManaged;
        copy.sourceText = entry.sourceText;
        if (entry.embedded) {
            // 埋め込みキーはエントリの設定そのものなので複製する（ファイル参照は複製先で読み込み直す）
            copy.animation = entry.animation;
            copy.loaded = true;
        }
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
    StartEntry(*entry);
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
        StartEntry(entry);
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
// 設定
//==================================================

bool KeyFrameAnimator::AddAnimation(const std::string &name, const std::string &jsonPath) {
    if (name.empty() || FindAnimation(name)) return false;
    AnimationEntry entry;
    entry.name = name;
    entry.jsonPath = jsonPath;
    animations_.push_back(std::move(entry));
    return true;
}

bool KeyFrameAnimator::RemoveAnimation(const std::string &name) {
    auto it = std::find_if(animations_.begin(), animations_.end(),
        [&name](const AnimationEntry &entry) { return entry.name == name; });
    if (it == animations_.end()) return false;
    animations_.erase(it);
    return true;
}

bool KeyFrameAnimator::AddAnimationData(const std::string &name, const JSON &keyframesJson) {
    if (name.empty() || FindAnimation(name)) return false;
    AnimationEntry entry;
    entry.name = name;
    if (!entry.animation.LoadFromJson(keyframesJson)) return false;
    entry.embedded = true;
    entry.loaded = true;
    animations_.push_back(std::move(entry));
    return true;
}

bool KeyFrameAnimator::SetAnimationData(const std::string &name, const JSON &keyframesJson) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    KeyframeAnimation animation;
    if (!animation.LoadFromJson(keyframesJson)) return false;
    entry->animation = std::move(animation);
    entry->embedded = true;
    entry->jsonPath.clear();
    entry->loaded = true;
    entry->loadFailed = false;
    entry->currentTime = std::min(entry->currentTime, entry->animation.GetDuration());
    return true;
}

bool KeyFrameAnimator::IsEmbedded(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->embedded;
}

const KeyframeAnimation *KeyFrameAnimator::GetAnimationData(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->loaded ? &entry->animation : nullptr;
}

bool KeyFrameAnimator::SetShortTextSource(const std::string &name, bool managed, const std::string &sourceText) {
    auto *entry = FindAnimation(name);
    if (!entry || sourceText.size() > 512) return false;
    entry->shortTextManaged = managed;
    entry->sourceText = sourceText;
    return true;
}

bool KeyFrameAnimator::IsShortTextManaged(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->shortTextManaged;
}

std::vector<std::string> KeyFrameAnimator::GetAnimationNames() const {
    std::vector<std::string> names;
    names.reserve(animations_.size());
    for (const auto &entry : animations_) names.push_back(entry.name);
    return names;
}

const std::vector<KeyFrameAnimator::TargetBinding> *KeyFrameAnimator::GetBindings(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? &entry->bindings : nullptr;
}

JSON KeyFrameAnimator::SaveAnimationEntryToJson(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? SaveEntryToJson(*entry) : JSON();
}

bool KeyFrameAnimator::SetJsonPath(const std::string &name, const std::string &jsonPath) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    if (entry->embedded) {
        // 埋め込みキーを捨ててファイル参照へ戻す
        entry->embedded = false;
        entry->jsonPath = jsonPath;
        entry->loaded = false;
        entry->loadFailed = false;
        entry->animation.Clear();
        return true;
    }
    if (entry->jsonPath != jsonPath) {
        entry->jsonPath = jsonPath;
        entry->loaded = false;
        entry->loadFailed = false;
        entry->animation.Clear();
    }
    return true;
}

std::string KeyFrameAnimator::GetJsonPath(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->jsonPath : std::string();
}

bool KeyFrameAnimator::SetLoop(const std::string &name, bool loop) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->loop = loop;
    return true;
}

bool KeyFrameAnimator::GetLoop(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->loop;
}

bool KeyFrameAnimator::SetPlayOnStart(const std::string &name, bool playOnStart) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->playOnStart = playOnStart;
    return true;
}

bool KeyFrameAnimator::GetPlayOnStart(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry && entry->playOnStart;
}

bool KeyFrameAnimator::SetTimeOffset(const std::string &name, float timeOffset) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->timeOffset = timeOffset;
    return true;
}

float KeyFrameAnimator::GetTimeOffset(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->timeOffset : 0.0f;
}

bool KeyFrameAnimator::SetValueScale(const std::string &name, float valueScale) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->valueScale = valueScale;
    return true;
}

float KeyFrameAnimator::GetValueScale(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->valueScale : 1.0f;
}

bool KeyFrameAnimator::SetValueOffset(const std::string &name, float valueOffset) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->valueOffset = valueOffset;
    return true;
}

float KeyFrameAnimator::GetValueOffset(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->valueOffset : 0.0f;
}

bool KeyFrameAnimator::AddBinding(const std::string &name, const std::string &componentType, const std::string &parameterName,
    int channel, int componentIndex, bool isScriptVariable) {
    auto *entry = FindAnimation(name);
    if (!entry || parameterName.empty()) return false;
    TargetBinding binding;
    binding.componentType = componentType;
    binding.componentIndex = componentIndex;
    binding.parameterName = parameterName;
    binding.channel = channel;
    binding.isScriptVariable = isScriptVariable;
    entry->bindings.push_back(std::move(binding));
    return true;
}

bool KeyFrameAnimator::ClearBindings(const std::string &name) {
    auto *entry = FindAnimation(name);
    if (!entry) return false;
    entry->bindings.clear();
    return true;
}

size_t KeyFrameAnimator::GetBindingCount(const std::string &name) const {
    const auto *entry = FindAnimation(name);
    return entry ? entry->bindings.size() : 0;
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
    // エディターで再生していない間は値を書き込まない（playOnStartも再生開始後の最初のUpdateまで保留する）
    if (auto *scene = GetOwnerSceneContext(); scene && !scene->IsPlaying()) return;
    if (applyPlayOnStart_) {
        applyPlayOnStart_ = false;
        for (auto &entry : animations_) {
            if (entry.playOnStart) {
                StartEntry(entry);
            }
        }
    }

    const float deltaTime = GetDeltaTime();
    for (auto &entry : animations_) {
        if (!entry.playing) continue;
        EnsureLoaded(entry);
        if (!entry.loaded) continue;

        if (entry.startPending) {
            entry.startPending = false;
            if (entry.embedded) {
                // 埋め込みエントリは再生開始フレームで必ず開始時刻の値を書き込む（編集時の値から始まらないように）
                const float value = entry.animation.Evaluate(entry.currentTime + entry.timeOffset) * entry.valueScale + entry.valueOffset;
                entry.lastValue = value;
                ApplyValue(entry, value);
                continue;
            }
        }

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

void KeyFrameAnimator::StartEntry(AnimationEntry &entry) {
    // 逆再生は末端から始める（埋め込みエントリのみ。ファイル参照は従来どおり先頭から）
    entry.currentTime = (entry.embedded && entry.playbackSpeed < 0.0f) ? entry.animation.GetDuration() : 0.0f;
    entry.playing = true;
    entry.startPending = true;
}

void KeyFrameAnimator::EnsureLoaded(AnimationEntry &entry) {
    if (entry.embedded) {
        entry.loaded = true;
        return;
    }
    if (entry.loaded || entry.loadFailed || entry.jsonPath.empty()) return;
    const JSON json = LoadJSON(ProjectPaths::ToPhysical(entry.jsonPath));
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

namespace {
/// @brief キーフレームjsonを検索して選択するポップアップ（"..."ボタンで開く）
/// @details 候補はポップアップを開いた時にだけAssets配下の.jsonを走査し、"keyframes"を含むものに絞る
///          （KeyframeAnimation::LoadFromJsonが読める形式。毎フレーム走査すると重いため開いた時のみ）。
///          シーンのオブジェクト単位のjson（*.scene/配下）は数が多くキーフレームでもないため対象外にする
/// @return 選択によりjsonPathが変更された場合は true
bool ShowKeyframeJsonPicker(std::string &jsonPath) {
    static std::vector<std::string> sCandidates;
    static ImGuiTextFilter sFilter;
    bool changed = false;

    if (ImGui::Button("...")) {
        ImGui::OpenPopup("##KeyframeJsonPicker");
    }
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", TranslationC("component.keyframeanimator.json_path.search"));
    }

    if (ImGui::BeginPopup("##KeyframeJsonPicker")) {
        if (ImGui::IsWindowAppearing()) {
            sCandidates.clear();
            for (const auto &path : ProjectPaths::ListAssetFiles({ ".json" })) {
                if (path.find(".scene/") != std::string::npos) continue;
                const TextFileData text = LoadTextFile(ProjectPaths::ToPhysical(path));
                const bool isKeyframeJson = std::any_of(text.lines.begin(), text.lines.end(),
                    [](const std::string &line) { return line.find("\"keyframes\"") != std::string::npos; });
                if (isKeyframeJson) sCandidates.push_back(path);
            }
        }

        constexpr float kWidth = 320.0f;
        if (ImGuiCustom::PopupSearchBox("##KeyframeJsonSearch", sFilter, kWidth)) {
            // Enterで先頭の候補を決定する
            for (const auto &path : sCandidates) {
                if (!sFilter.PassFilter(path.c_str())) continue;
                if (path != jsonPath) {
                    jsonPath = path;
                    changed = true;
                }
                ImGui::CloseCurrentPopup();
                break;
            }
        }
        ImGui::Separator();

        std::vector<const std::string *> matches;
        for (const auto &path : sCandidates) {
            if (sFilter.PassFilter(path.c_str())) matches.push_back(&path);
        }
        ImGui::BeginChild("##KeyframeJsonList", ImVec2(kWidth, ImGuiCustom::PopupSearchListHeight(matches.size())));
        for (const auto *path : matches) {
            const bool selected = (*path == jsonPath);
            if (ImGui::Selectable(path->c_str(), selected)) {
                if (!selected) {
                    jsonPath = *path;
                    changed = true;
                }
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches.empty()) {
            ImGui::TextDisabled("%s", TranslationC("editor.imguicustom.search.noresults"));
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    return changed;
}
} // namespace

void KeyFrameAnimator::ShowEmbeddedKeyframesImGui(AnimationEntry &entry) {
    ImGui::Text("埋め込みキーフレーム（シーンに保存）: %d キー / %.2f 秒",
        static_cast<int>(entry.animation.GetKeyframeCount()), entry.animation.GetDuration());
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "イージングは「そのキーから次のキーへ」の補間に使われる（最後のキーのイージングは未使用）");
    }
    auto keys = entry.animation.GetKeyframes();
    bool changed = false;
    int removeKey = -1;
    if (ImGui::BeginTable("##embeddedKeys", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("時刻(秒)");
        ImGui::TableSetupColumn("値");
        ImGui::TableSetupColumn("次のキーへのイージング");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (int k = 0; k < static_cast<int>(keys.size()); ++k) {
            auto &key = keys[k];
            ImGui::PushID(k);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            // 変更後はキー列を作り直して時刻昇順へ並べ替える
            if (ImGui::DragFloat("##time", &key.time, 0.01f, 0.0f, 3600.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) changed = true;
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("##value", &key.value, 0.01f, -1.0e6f, 1.0e6f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) changed = true;
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool last = k + 1 == static_cast<int>(keys.size());
            if (ImGui::BeginCombo("##ease", last ? "（最後のキー：未使用）" : EaseTypeToString(key.easeType))) {
                for (int e = static_cast<int>(EaseType::Linear); e <= static_cast<int>(EaseType::EaseOutInBounce); ++e) {
                    const auto type = static_cast<EaseType>(e);
                    if (ImGui::Selectable(EaseTypeToString(type), type == key.easeType)) {
                        key.easeType = type;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("×")) removeKey = k;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::SmallButton("キーを追加")) {
        Keyframe key;
        key.time = entry.animation.GetDuration() + (keys.empty() ? 0.0f : 1.0f);
        key.value = keys.empty() ? 0.0f : keys.back().value;
        keys.push_back(key);
        changed = true;
    }
    if (removeKey >= 0) {
        keys.erase(keys.begin() + removeKey);
        changed = true;
    }
    if (changed) {
        KeyframeAnimation next;
        for (const auto &key : keys) next.AddKeyframe(key.time, key.value, key.easeType);
        entry.animation = std::move(next);
        entry.currentTime = std::min(entry.currentTime, entry.animation.GetDuration());
    }
}

void KeyFrameAnimator::ShowAnimationImGui(AnimationEntry &entry, const std::vector<ParameterBindingCandidate> &candidates) {
    ImGui::InputText(TranslationLabel("component.keyframeanimator.name"), &entry.name);
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "Play/Stop等で指定する識別名");
    }
    if (entry.shortTextManaged) {
        ImGuiCustom::TextDisabledWrapped("短文から生成: %s", entry.sourceText.c_str());
    }
    if (entry.embedded) {
        ShowEmbeddedKeyframesImGui(entry);
    } else {
    ImGui::InputText(TranslationLabel("component.keyframeanimator.json_path"), &entry.jsonPath);
    ImGui::SameLine();
    // 一覧から検索して選んだ場合は、Reloadと同様にすぐ読み込み直す
    if (ShowKeyframeJsonPicker(entry.jsonPath)) {
        entry.loaded = false;
        entry.loadFailed = false;
        entry.animation.Clear();
        EnsureLoaded(entry);
    }
    ImGui::SameLine();
    if (ImGui::Button(TranslationLabel("component.keyframeanimator.reload"))) {
        entry.loaded = false;
        entry.loadFailed = false;
        entry.animation.Clear();
        EnsureLoaded(entry);
    }
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "キーフレームjsonのパス（Keyframe Animation Editorで作成できる）。Reloadで読み込む");
    }
    if (!entry.loaded && !entry.loadFailed && !entry.jsonPath.empty()) {
        EnsureLoaded(entry);
    }
    if (entry.loaded) {
        ImGui::Text(TranslationC("component.keyframeanimator.keys_d_duration_2fs"), static_cast<int>(entry.animation.GetKeyframeCount()), entry.animation.GetDuration());
    } else if (entry.loadFailed) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", TranslationC("component.keyframeanimator.loadfailed"));
    } else {
        ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.keyframeanimator.desc_1"));
    }
    if (ImGui::SmallButton("キーをシーンへ埋め込む")) {
        // ファイルのキーをこのエントリへ複製し、以後はシーンに保存する（元ファイルは変更しない）
        KeyframeAnimation keys;
        if (entry.loaded) keys = entry.animation;
        entry.animation = std::move(keys);
        entry.embedded = true;
        entry.jsonPath.clear();
        entry.loaded = true;
        entry.loadFailed = false;
    }
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "読み込み済みのキーをこのエントリ内に保持し、シーンへ直接保存する（ファイル参照をやめる）");
    }
    }

    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.playback_speed"), &entry.playbackSpeed, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "再生速度の倍率（負の値で逆再生）");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.time_offset"), &entry.timeOffset, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "評価時刻へ加算する再生時オフセット（秒）。同じアニメを複数オブジェクトで位相をずらして再生する用途など");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.value_scale"), &entry.valueScale, 0.01f);
    if (std::abs(entry.valueScale - kDegreesToRadians) < 1e-6f) {
        ImGui::SameLine();
        ImGui::TextDisabled("(度→ラジアン)");
    }
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "評価値にかけるスケール。Value Offsetの加算より先に適用される（適用値 = 評価値 × Scale + Offset）");
    }
    ImGui::DragFloat(TranslationLabel("component.keyframeanimator.value_offset"), &entry.valueOffset, 0.01f);
    if (ImGui::IsItemHovered()) {
        ImGuiCustom::SetTooltipWrapped("%s", "評価値へ加算するデフォルト値オフセット。適用先ごとの基準値の違いを吸収する");
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
            StartEntry(entry);
        }
        if (ImGui::IsItemHovered()) {
            ImGuiCustom::SetTooltipWrapped("%s", "再生を開始する（値の適用はゲームループ実行中のみ行われる）");
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

JSON KeyFrameAnimator::SaveEntryToJson(const AnimationEntry &entry) {
    JSON entryJson = JSON::object();
    entryJson["name"] = entry.name;
    entryJson["jsonPath"] = entry.embedded ? std::string() : entry.jsonPath;
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
    // 追加項目は使用時だけ書き出す（ファイル参照の既存エントリの保存形式は変えない）
    if (entry.embedded) entryJson["embeddedAnimation"] = entry.animation.SaveToJson();
    if (entry.shortTextManaged || !entry.sourceText.empty()) {
        entryJson["shortTextManaged"] = entry.shortTextManaged;
        entryJson["sourceText"] = entry.sourceText;
    }
    return entryJson;
}

bool KeyFrameAnimator::LoadEntryFromJson(const JSON &json, AnimationEntry &out) {
    if (!json.is_object()) return false;
    AnimationEntry entry;
    auto text = [&](const char *key, std::string &value, size_t maxSize) {
        if (!json.contains(key)) return true;
        if (!json[key].is_string()) return false;
        value = json[key].get<std::string>();
        return value.size() <= maxSize;
    };
    auto number = [&](const char *key, float &value) {
        if (!json.contains(key)) return true;
        if (!json[key].is_number()) return false;
        const double n = json[key].get<double>();
        if (!std::isfinite(n) || std::abs(n) > KeyframeAnimation::kMaxAbsValue) return false;
        value = static_cast<float>(n);
        return true;
    };
    auto boolean = [&](const char *key, bool &value) {
        if (!json.contains(key)) return true;
        if (!json[key].is_boolean()) return false;
        value = json[key].get<bool>();
        return true;
    };
    if (!text("name", entry.name, 256) || !text("jsonPath", entry.jsonPath, 4096) ||
        !number("playbackSpeed", entry.playbackSpeed) || !number("timeOffset", entry.timeOffset) ||
        !number("valueScale", entry.valueScale) || !number("valueOffset", entry.valueOffset) ||
        !boolean("loop", entry.loop) || !boolean("playOnStart", entry.playOnStart) ||
        !boolean("shortTextManaged", entry.shortTextManaged) || !text("sourceText", entry.sourceText, 512)) {
        return false;
    }
    if (json.contains("bindings")) {
        if (!json["bindings"].is_array()) return false;
        for (const auto &bindingJson : json["bindings"]) {
            if (!bindingJson.is_object()) return false;
            const auto missing = [&](const char *key) { return !bindingJson.contains(key); };
            if ((!missing("componentType") && !bindingJson["componentType"].is_string()) ||
                (!missing("parameterName") && !bindingJson["parameterName"].is_string()) ||
                (!missing("componentIndex") && !bindingJson["componentIndex"].is_number_integer()) ||
                (!missing("channel") && !bindingJson["channel"].is_number_integer()) ||
                (!missing("isScriptVariable") && !bindingJson["isScriptVariable"].is_boolean())) {
                return false;
            }
            auto binding = LoadParameterBindingFromJson(bindingJson);
            if (binding.componentIndex < 0 || binding.componentIndex > 255 || binding.channel < -1 || binding.channel > 3) return false;
            entry.bindings.push_back(std::move(binding));
        }
    }
    if (json.contains("embeddedAnimation")) {
        if (!entry.animation.LoadFromJson(json["embeddedAnimation"])) return false;
        entry.embedded = true;
        entry.loaded = true;
        entry.jsonPath.clear();
    }
    out = std::move(entry);
    return true;
}

JSON KeyFrameAnimator::SaveToJson() const {
    JSON json = JSON::object();
    JSON animationsJson = JSON::array();
    for (const auto &entry : animations_) {
        animationsJson.push_back(SaveEntryToJson(entry));
    }
    json["animations"] = std::move(animationsJson);
    return json;
}

bool KeyFrameAnimator::LoadFromJson(const JSON &json) {
    if (!json.is_object() || !json.contains("animations")) {
        animations_.clear();
        return true;
    }
    if (!json["animations"].is_array()) return false;
    // 全エントリを検証してから差し替える（不正なデータの場合は以前の状態を保持する）
    std::vector<AnimationEntry> loaded;
    for (const auto &entryJson : json["animations"]) {
        AnimationEntry entry;
        if (!LoadEntryFromJson(entryJson, entry)) {
            Log(Translation("engine.keyframeanimator.load.failed") + entryJson.dump().substr(0, 200), LogSeverity::Warning);
            return false;
        }
        loaded.push_back(std::move(entry));
    }
    animations_ = std::move(loaded);
    return true;
}

} // namespace KashipanEngine
