#include "Objects/Components/Shake.h"

#include <algorithm>
#include <cmath>

#include "Math/Quaternion.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Objects/ObjectContext.h"
#include "Scene/Components/SceneShakeApplier.h"
#include "Scene/SceneContext.h"
#include "Utilities/RandomValue.h"
#include "Utilities/TimeUtils.h"

namespace KashipanEngine {

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

/// @brief ShowImGuiのイージング選択コンボ用の名前一覧（Easings.cppのkEaseTypeNamesと同じ並び）
constexpr const char *kEaseTypeComboNames[] = {
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
    "EaseInBounce", "EaseOutBounce", "EaseInOutBounce", "EaseOutInBounce",
};
} // namespace

std::unique_ptr<IObjectComponent> Shake::Clone() const {
    auto ptr = std::make_unique<Shake>();
    ptr->positionEnableX_ = positionEnableX_;
    ptr->positionEnableY_ = positionEnableY_;
    ptr->positionEnableZ_ = positionEnableZ_;
    ptr->positionAmplitude_ = positionAmplitude_;
    ptr->positionSpeed_ = positionSpeed_;
    ptr->positionEaseType_ = positionEaseType_;
    ptr->rotationEnableX_ = rotationEnableX_;
    ptr->rotationEnableY_ = rotationEnableY_;
    ptr->rotationEnableZ_ = rotationEnableZ_;
    ptr->rotationAmplitudeDeg_ = rotationAmplitudeDeg_;
    ptr->rotationSpeed_ = rotationSpeed_;
    ptr->rotationEaseType_ = rotationEaseType_;
    ptr->autoPlay_ = autoPlay_;
    ptr->duration_ = duration_;
    ptr->processTiming_ = processTiming_;
    ptr->applyTarget_ = applyTarget_;
    return ptr;
}

void Shake::Initialize() {
    if (autoPlay_) {
        Play(duration_);
    }
    auto *applier = GetOrAddSceneShakeApplier();
    if (applier) applier->RegisterShake(this);
}

void Shake::Finalize() {
    isPlaying_ = false;
    // Transformへ加算済みのオフセットが残っていれば、破棄前にきちんと差し引いて元へ戻す
    if (appliedToTransformLastFrame_) {
        ApplyToTransform();
    }
    auto *sceneContext = GetOwnerSceneContext();
    auto *applier = sceneContext ? sceneContext->GetComponent<SceneShakeApplier>() : nullptr;
    if (applier) applier->UnregisterShake(this);
}

void Shake::Update() {
    const float dt = GetDeltaTime();

    if (isPlaying_ && playDuration_ > 0.0f) {
        elapsedPlayTime_ += dt;
        if (elapsedPlayTime_ >= playDuration_) {
            isPlaying_ = false;
        }
    }

    ComputeOffsets(dt);

    // DeferredEndの場合はSceneShakeApplierが後でまとめて呼ぶため、ここでは適用しない
    if (processTiming_ == ProcessTiming::Immediate) {
        ApplyToTransform();
    }
}

float Shake::UpdateAxisRuntime(AxisRuntime &state, bool enabled, float amplitude, float speed, EaseType easeType, float dt) {
    if (!enabled || amplitude == 0.0f || speed <= 0.0f) {
        // 無効化された軸は、目標値追従をリセットしつつ現在値を滑らかに0へ戻す
        state.previousValue = 0.0f;
        state.targetValue = 0.0f;
        state.timer = 0.0f;
        state.currentValue = Lerp(state.currentValue, 0.0f, 0.2f);
        if (std::abs(state.currentValue) < 0.0001f) state.currentValue = 0.0f;
        return state.currentValue;
    }

    const float stepDuration = 1.0f / speed;
    state.timer += dt;
    if (state.timer >= stepDuration) {
        state.timer = std::fmod(state.timer, stepDuration);
        state.previousValue = state.targetValue;
        state.targetValue = GetRandomFloat(-amplitude, amplitude);
    }
    const float t = std::clamp(state.timer / stepDuration, 0.0f, 1.0f);
    state.currentValue = Eased(state.previousValue, state.targetValue, t, easeType);
    return state.currentValue;
}

void Shake::ComputeOffsets(float dt) {
    const bool playing = isPlaying_;
    const float px = UpdateAxisRuntime(positionRuntime_[0], playing && positionEnableX_, positionAmplitude_.x, positionSpeed_.x, positionEaseType_, dt);
    const float py = UpdateAxisRuntime(positionRuntime_[1], playing && positionEnableY_, positionAmplitude_.y, positionSpeed_.y, positionEaseType_, dt);
    const float pz = UpdateAxisRuntime(positionRuntime_[2], playing && positionEnableZ_, positionAmplitude_.z, positionSpeed_.z, positionEaseType_, dt);
    currentPositionOffset_ = Vector3(px, py, pz);

    const float rx = UpdateAxisRuntime(rotationRuntime_[0], playing && rotationEnableX_, rotationAmplitudeDeg_.x, rotationSpeed_.x, rotationEaseType_, dt);
    const float ry = UpdateAxisRuntime(rotationRuntime_[1], playing && rotationEnableY_, rotationAmplitudeDeg_.y, rotationSpeed_.y, rotationEaseType_, dt);
    const float rz = UpdateAxisRuntime(rotationRuntime_[2], playing && rotationEnableZ_, rotationAmplitudeDeg_.z, rotationSpeed_.z, rotationEaseType_, dt);
    currentRotationOffset_ = Vector3(rx * kDegToRad, ry * kDegToRad, rz * kDegToRad);
}

Matrix4x4 Shake::ApplyRenderOnlyOffsets(const EmptyObject *owner, const Matrix4x4 &worldMatrix) {
    if (!owner) return worldMatrix;

    Matrix4x4 result = worldMatrix;
    for (const Shake *shake : owner->GetComponents<Shake>()) {
        if (!shake || !shake->IsActive() || !shake->IsPlaying()) continue;
        if (shake->GetApplyTarget() != ApplyTarget::RenderOnly) continue;

        const Vector3 &posOffset = shake->GetCurrentPositionOffset();
        const Vector3 &rotOffset = shake->GetCurrentRotationOffset();
        if (posOffset.LengthSquared() < 1e-8f && rotOffset.LengthSquared() < 1e-8f) continue;

        // 行ベクトル行列の規約に合わせ、現在のワールドピボットへ一度戻してから
        // 回転とワールド空間の平行移動を適用する
        const Vector3 pivot(result.m[3][0], result.m[3][1], result.m[3][2]);
        Matrix4x4 toOrigin;
        toOrigin.MakeTranslate(Vector3(-pivot.x, -pivot.y, -pivot.z));
        Matrix4x4 rotate = Quaternion::MakeRotateEuler(rotOffset).MakeRotateMatrix();
        Matrix4x4 backToPivot;
        backToPivot.MakeTranslate(pivot);
        Matrix4x4 translateOffset;
        translateOffset.MakeTranslate(posOffset);
        result = result * toOrigin * rotate * backToPivot * translateOffset;
    }
    return result;
}

void Shake::ApplyToTransform() {
    auto *objectContext = GetOwnerObjectContext();
    auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
    if (!transform) return;

    // 前回加算したオフセットを必ず先に差し引く（適用方法・再生状態が切り替わっても
    // Transformが揺れたまま残らないようにする）
    if (appliedToTransformLastFrame_) {
        transform->SetTranslate(transform->GetTranslate() - lastAppliedPositionOffset_);
        transform->SetRotate(transform->GetRotate() - lastAppliedRotationOffset_);
        appliedToTransformLastFrame_ = false;
    }

    if (applyTarget_ == ApplyTarget::ToTransform && isPlaying_) {
        transform->SetTranslate(transform->GetTranslate() + currentPositionOffset_);
        transform->SetRotate(transform->GetRotate() + currentRotationOffset_);
        lastAppliedPositionOffset_ = currentPositionOffset_;
        lastAppliedRotationOffset_ = currentRotationOffset_;
        appliedToTransformLastFrame_ = true;
    }
}

SceneShakeApplier *Shake::GetOrAddSceneShakeApplier() const {
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return nullptr;
    auto *applier = sceneContext->GetComponent<SceneShakeApplier>();
    if (!applier) {
        applier = sceneContext->AddComponent<SceneShakeApplier>();
    }
    return applier;
}

#if defined(USE_IMGUI)

void Shake::ShowImGui() {
    auto easeCombo = [](const char *label, EaseType &type) {
        int index = 0;
        for (int i = 0; i < IM_ARRAYSIZE(kEaseTypeComboNames); ++i) {
            if (EaseTypeToString(type) == std::string(kEaseTypeComboNames[i])) { index = i; break; }
        }
        if (ImGui::Combo(label, &index, kEaseTypeComboNames, IM_ARRAYSIZE(kEaseTypeComboNames))) {
            type = StringToEaseType(kEaseTypeComboNames[index]);
        }
    };

    if (isPlaying_) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Playing");
        if (ImGui::Button("Stop")) Stop();
    } else {
        ImGui::TextDisabled("Stopped");
        ImGui::SameLine();
        if (ImGui::Button("Play")) Play(duration_);
    }

    ImGui::Checkbox("Auto Play", &autoPlay_);
    ImGui::DragFloat("Duration (0=Infinite)", &duration_, 0.01f, 0.0f, 60.0f);

    static const char *kTimingLabels[] = { "Immediate", "Deferred (End of Frame)" };
    int timingIndex = static_cast<int>(processTiming_);
    if (ImGui::Combo("Process Timing", &timingIndex, kTimingLabels, IM_ARRAYSIZE(kTimingLabels))) {
        processTiming_ = static_cast<ProcessTiming>(timingIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Immediate: 自身のUpdate内でその場処理 / Deferred: 全オブジェクト更新後にまとめて処理（他スクリプトと競合しない）");
    }

    static const char *kApplyLabels[] = { "To Transform", "Render Only" };
    int applyIndex = static_cast<int>(applyTarget_);
    if (ImGui::Combo("Apply Target", &applyIndex, kApplyLabels, IM_ARRAYSIZE(kApplyLabels))) {
        applyTarget_ = static_cast<ApplyTarget>(applyIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "To Transform: Transform自体を書き換える（終了後は元に戻る） / Render Only: 描画にのみ適用しTransformは変更しない");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Position Shake");
    ImGui::Checkbox("Pos X", &positionEnableX_); ImGui::SameLine();
    ImGui::Checkbox("Pos Y", &positionEnableY_); ImGui::SameLine();
    ImGui::Checkbox("Pos Z", &positionEnableZ_);
    ImGuiCustom::EditValue("Position Amplitude", positionAmplitude_, { .vSpeed = 0.01f, .vMin = 0.0f });
    ImGuiCustom::EditValue("Position Speed", positionSpeed_, { .vSpeed = 0.1f, .vMin = 0.0f });
    easeCombo("Position Ease", positionEaseType_);

    ImGui::Separator();
    ImGui::TextUnformatted("Rotation Shake");
    ImGui::Checkbox("Rot X", &rotationEnableX_); ImGui::SameLine();
    ImGui::Checkbox("Rot Y", &rotationEnableY_); ImGui::SameLine();
    ImGui::Checkbox("Rot Z", &rotationEnableZ_);
    ImGuiCustom::EditValue("Rotation Amplitude (deg)", rotationAmplitudeDeg_, { .vSpeed = 0.1f, .vMin = 0.0f });
    ImGuiCustom::EditValue("Rotation Speed", rotationSpeed_, { .vSpeed = 0.1f, .vMin = 0.0f });
    easeCombo("Rotation Ease", rotationEaseType_);
}

#endif // USE_IMGUI

JSON Shake::SaveToJson() const {
    JSON json;
    json["positionEnableX"] = positionEnableX_;
    json["positionEnableY"] = positionEnableY_;
    json["positionEnableZ"] = positionEnableZ_;
    json["positionAmplitude"] = ToJSON(positionAmplitude_);
    json["positionSpeed"] = ToJSON(positionSpeed_);
    json["positionEaseType"] = EaseTypeToString(positionEaseType_);
    json["rotationEnableX"] = rotationEnableX_;
    json["rotationEnableY"] = rotationEnableY_;
    json["rotationEnableZ"] = rotationEnableZ_;
    json["rotationAmplitudeDeg"] = ToJSON(rotationAmplitudeDeg_);
    json["rotationSpeed"] = ToJSON(rotationSpeed_);
    json["rotationEaseType"] = EaseTypeToString(rotationEaseType_);
    json["autoPlay"] = autoPlay_;
    json["duration"] = duration_;
    json["processTiming"] = static_cast<int>(processTiming_);
    json["applyTarget"] = static_cast<int>(applyTarget_);
    return json;
}

bool Shake::LoadFromJson(const JSON &json) {
    positionEnableX_ = json.value("positionEnableX", false);
    positionEnableY_ = json.value("positionEnableY", false);
    positionEnableZ_ = json.value("positionEnableZ", false);
    if (json.contains("positionAmplitude")) positionAmplitude_ = FromJSON<Vector3>(json["positionAmplitude"]);
    if (json.contains("positionSpeed")) positionSpeed_ = FromJSON<Vector3>(json["positionSpeed"]);
    positionEaseType_ = StringToEaseType(json.value("positionEaseType", std::string("Linear")));

    rotationEnableX_ = json.value("rotationEnableX", false);
    rotationEnableY_ = json.value("rotationEnableY", false);
    rotationEnableZ_ = json.value("rotationEnableZ", false);
    if (json.contains("rotationAmplitudeDeg")) rotationAmplitudeDeg_ = FromJSON<Vector3>(json["rotationAmplitudeDeg"]);
    if (json.contains("rotationSpeed")) rotationSpeed_ = FromJSON<Vector3>(json["rotationSpeed"]);
    rotationEaseType_ = StringToEaseType(json.value("rotationEaseType", std::string("Linear")));

    autoPlay_ = json.value("autoPlay", true);
    duration_ = json.value("duration", 0.0f);
    processTiming_ = static_cast<ProcessTiming>(json.value("processTiming", 1));
    applyTarget_ = static_cast<ApplyTarget>(json.value("applyTarget", 1));
    return true;
}

} // namespace KashipanEngine
