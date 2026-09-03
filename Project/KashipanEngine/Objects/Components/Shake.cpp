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
#include "Utilities/Translation.h"

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
    ptr->positionAmplitudeStartMultiplier_ = positionAmplitudeStartMultiplier_;
    ptr->positionAmplitudeEndMultiplier_ = positionAmplitudeEndMultiplier_;
    ptr->positionSpeedStartMultiplier_ = positionSpeedStartMultiplier_;
    ptr->positionSpeedEndMultiplier_ = positionSpeedEndMultiplier_;
    ptr->positionEnvelopeEaseType_ = positionEnvelopeEaseType_;
    ptr->rotationEnableX_ = rotationEnableX_;
    ptr->rotationEnableY_ = rotationEnableY_;
    ptr->rotationEnableZ_ = rotationEnableZ_;
    ptr->rotationAmplitudeDeg_ = rotationAmplitudeDeg_;
    ptr->rotationSpeed_ = rotationSpeed_;
    ptr->rotationEaseType_ = rotationEaseType_;
    ptr->rotationAmplitudeStartMultiplier_ = rotationAmplitudeStartMultiplier_;
    ptr->rotationAmplitudeEndMultiplier_ = rotationAmplitudeEndMultiplier_;
    ptr->rotationSpeedStartMultiplier_ = rotationSpeedStartMultiplier_;
    ptr->rotationSpeedEndMultiplier_ = rotationSpeedEndMultiplier_;
    ptr->rotationEnvelopeEaseType_ = rotationEnvelopeEaseType_;
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
    const float progress = GetPlayProgress();

    // 振れ幅・スピードへ掛ける時間変化係数（start→endをイージング補間、負値にはならないようclamp）
    const float posAmplitudeMul = std::max(0.0f, Eased(positionAmplitudeStartMultiplier_, positionAmplitudeEndMultiplier_, progress, positionEnvelopeEaseType_));
    const float posSpeedMul = std::max(0.0f, Eased(positionSpeedStartMultiplier_, positionSpeedEndMultiplier_, progress, positionEnvelopeEaseType_));
    const float rotAmplitudeMul = std::max(0.0f, Eased(rotationAmplitudeStartMultiplier_, rotationAmplitudeEndMultiplier_, progress, rotationEnvelopeEaseType_));
    const float rotSpeedMul = std::max(0.0f, Eased(rotationSpeedStartMultiplier_, rotationSpeedEndMultiplier_, progress, rotationEnvelopeEaseType_));

    const float px = UpdateAxisRuntime(positionRuntime_[0], playing && positionEnableX_, positionAmplitude_.x * posAmplitudeMul, positionSpeed_.x * posSpeedMul, positionEaseType_, dt);
    const float py = UpdateAxisRuntime(positionRuntime_[1], playing && positionEnableY_, positionAmplitude_.y * posAmplitudeMul, positionSpeed_.y * posSpeedMul, positionEaseType_, dt);
    const float pz = UpdateAxisRuntime(positionRuntime_[2], playing && positionEnableZ_, positionAmplitude_.z * posAmplitudeMul, positionSpeed_.z * posSpeedMul, positionEaseType_, dt);
    currentPositionOffset_ = Vector3(px, py, pz);

    const float rx = UpdateAxisRuntime(rotationRuntime_[0], playing && rotationEnableX_, rotationAmplitudeDeg_.x * rotAmplitudeMul, rotationSpeed_.x * rotSpeedMul, rotationEaseType_, dt);
    const float ry = UpdateAxisRuntime(rotationRuntime_[1], playing && rotationEnableY_, rotationAmplitudeDeg_.y * rotAmplitudeMul, rotationSpeed_.y * rotSpeedMul, rotationEaseType_, dt);
    const float rz = UpdateAxisRuntime(rotationRuntime_[2], playing && rotationEnableZ_, rotationAmplitudeDeg_.z * rotAmplitudeMul, rotationSpeed_.z * rotSpeedMul, rotationEaseType_, dt);
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
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", TranslationC("component.shake.playing"));
        if (ImGui::Button(TranslationLabel("component.shake.stop"))) Stop();
    } else {
        ImGui::TextDisabled("%s", TranslationC("component.shake.stopped"));
        ImGui::SameLine();
        if (ImGui::Button(TranslationLabel("component.shake.play"))) Play(duration_);
    }

    ImGui::Checkbox(TranslationLabel("component.shake.auto_play"), &autoPlay_);
    ImGui::DragFloat(TranslationLabel("component.shake.duration_0_infinite"), &duration_, 0.01f, 0.0f, 60.0f);

    const char *kTimingLabels[] = { TranslationC("component.shake.timing.immediate"), TranslationC("component.shake.timing.deferred") };
    int timingIndex = static_cast<int>(processTiming_);
    if (ImGui::Combo(TranslationLabel("component.shake.process_timing"), &timingIndex, kTimingLabels, IM_ARRAYSIZE(kTimingLabels))) {
        processTiming_ = static_cast<ProcessTiming>(timingIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Immediate: 自身のUpdate内でその場処理 / Deferred: 全オブジェクト更新後にまとめて処理（他スクリプトと競合しない）");
    }

    const char *kApplyLabels[] = { TranslationC("component.shake.applytarget.transform"), TranslationC("component.shake.applytarget.renderonly") };
    int applyIndex = static_cast<int>(applyTarget_);
    if (ImGui::Combo(TranslationLabel("component.shake.apply_target"), &applyIndex, kApplyLabels, IM_ARRAYSIZE(kApplyLabels))) {
        applyTarget_ = static_cast<ApplyTarget>(applyIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "To Transform: Transform自体を書き換える（終了後は元に戻る） / Render Only: 描画にのみ適用しTransformは変更しない");
    }

    ImGui::Separator();
    ImGui::TextUnformatted(TranslationC("component.shake.position_shake"));
    ImGui::Checkbox(TranslationLabel("component.shake.pos_x"), &positionEnableX_); ImGui::SameLine();
    ImGui::Checkbox(TranslationLabel("component.shake.pos_y"), &positionEnableY_); ImGui::SameLine();
    ImGui::Checkbox(TranslationLabel("component.shake.pos_z"), &positionEnableZ_);
    ImGuiCustom::EditValue(TranslationLabel("component.shake.position_amplitude"), positionAmplitude_, { .vSpeed = 0.01f, .vMin = 0.0f });
    ImGuiCustom::EditValue(TranslationLabel("component.shake.position_speed"), positionSpeed_, { .vSpeed = 0.1f, .vMin = 0.0f });
    easeCombo("Position Ease", positionEaseType_);

    ImGui::TextUnformatted(TranslationC("component.shake.envelope_section"));
    ImGui::DragFloat(TranslationLabel("component.shake.position_envelope_amplitude_start"), &positionAmplitudeStartMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.position_envelope_amplitude_end"), &positionAmplitudeEndMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.position_envelope_speed_start"), &positionSpeedStartMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.position_envelope_speed_end"), &positionSpeedEndMultiplier_, 0.01f, 0.0f, 10.0f);
    easeCombo("Position Envelope Ease", positionEnvelopeEaseType_);

    ImGui::Separator();
    ImGui::TextUnformatted(TranslationC("component.shake.rotation_shake"));
    ImGui::Checkbox(TranslationLabel("component.shake.rot_x"), &rotationEnableX_); ImGui::SameLine();
    ImGui::Checkbox(TranslationLabel("component.shake.rot_y"), &rotationEnableY_); ImGui::SameLine();
    ImGui::Checkbox(TranslationLabel("component.shake.rot_z"), &rotationEnableZ_);
    ImGuiCustom::EditValue(TranslationLabel("component.shake.rotation_amplitude_deg"), rotationAmplitudeDeg_, { .vSpeed = 0.1f, .vMin = 0.0f });
    ImGuiCustom::EditValue(TranslationLabel("component.shake.rotation_speed"), rotationSpeed_, { .vSpeed = 0.1f, .vMin = 0.0f });
    easeCombo("Rotation Ease", rotationEaseType_);

    ImGui::TextUnformatted(TranslationC("component.shake.envelope_section"));
    ImGui::DragFloat(TranslationLabel("component.shake.rotation_envelope_amplitude_start"), &rotationAmplitudeStartMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.rotation_envelope_amplitude_end"), &rotationAmplitudeEndMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.rotation_envelope_speed_start"), &rotationSpeedStartMultiplier_, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(TranslationLabel("component.shake.rotation_envelope_speed_end"), &rotationSpeedEndMultiplier_, 0.01f, 0.0f, 10.0f);
    easeCombo("Rotation Envelope Ease", rotationEnvelopeEaseType_);
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
    json["positionAmplitudeStartMultiplier"] = positionAmplitudeStartMultiplier_;
    json["positionAmplitudeEndMultiplier"] = positionAmplitudeEndMultiplier_;
    json["positionSpeedStartMultiplier"] = positionSpeedStartMultiplier_;
    json["positionSpeedEndMultiplier"] = positionSpeedEndMultiplier_;
    json["positionEnvelopeEaseType"] = EaseTypeToString(positionEnvelopeEaseType_);
    json["rotationEnableX"] = rotationEnableX_;
    json["rotationEnableY"] = rotationEnableY_;
    json["rotationEnableZ"] = rotationEnableZ_;
    json["rotationAmplitudeDeg"] = ToJSON(rotationAmplitudeDeg_);
    json["rotationSpeed"] = ToJSON(rotationSpeed_);
    json["rotationEaseType"] = EaseTypeToString(rotationEaseType_);
    json["rotationAmplitudeStartMultiplier"] = rotationAmplitudeStartMultiplier_;
    json["rotationAmplitudeEndMultiplier"] = rotationAmplitudeEndMultiplier_;
    json["rotationSpeedStartMultiplier"] = rotationSpeedStartMultiplier_;
    json["rotationSpeedEndMultiplier"] = rotationSpeedEndMultiplier_;
    json["rotationEnvelopeEaseType"] = EaseTypeToString(rotationEnvelopeEaseType_);
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
    positionAmplitudeStartMultiplier_ = json.value("positionAmplitudeStartMultiplier", 1.0f);
    positionAmplitudeEndMultiplier_ = json.value("positionAmplitudeEndMultiplier", 1.0f);
    positionSpeedStartMultiplier_ = json.value("positionSpeedStartMultiplier", 1.0f);
    positionSpeedEndMultiplier_ = json.value("positionSpeedEndMultiplier", 1.0f);
    positionEnvelopeEaseType_ = StringToEaseType(json.value("positionEnvelopeEaseType", std::string("Linear")));

    rotationEnableX_ = json.value("rotationEnableX", false);
    rotationEnableY_ = json.value("rotationEnableY", false);
    rotationEnableZ_ = json.value("rotationEnableZ", false);
    if (json.contains("rotationAmplitudeDeg")) rotationAmplitudeDeg_ = FromJSON<Vector3>(json["rotationAmplitudeDeg"]);
    if (json.contains("rotationSpeed")) rotationSpeed_ = FromJSON<Vector3>(json["rotationSpeed"]);
    rotationEaseType_ = StringToEaseType(json.value("rotationEaseType", std::string("Linear")));
    rotationAmplitudeStartMultiplier_ = json.value("rotationAmplitudeStartMultiplier", 1.0f);
    rotationAmplitudeEndMultiplier_ = json.value("rotationAmplitudeEndMultiplier", 1.0f);
    rotationSpeedStartMultiplier_ = json.value("rotationSpeedStartMultiplier", 1.0f);
    rotationSpeedEndMultiplier_ = json.value("rotationSpeedEndMultiplier", 1.0f);
    rotationEnvelopeEaseType_ = StringToEaseType(json.value("rotationEnvelopeEaseType", std::string("Linear")));

    autoPlay_ = json.value("autoPlay", true);
    duration_ = json.value("duration", 0.0f);
    processTiming_ = static_cast<ProcessTiming>(json.value("processTiming", 1));
    applyTarget_ = static_cast<ApplyTarget>(json.value("applyTarget", 1));
    return true;
}

} // namespace KashipanEngine
