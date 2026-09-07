#pragma once
#include <algorithm>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/CameraBoundsZone3D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"
#include "Utilities/MathUtils/Easings.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 同一オブジェクトの Camera3D を、複数の追従先オブジェクトへ滑らかに追従させるコンポーネント
/// @details 追従先オブジェクトは複数指定可能で、それぞれ移動(x,y,z)・回転(x,y,z)のどの軸を
///          追従に用いるかを個別に選択できる（複数の追従先が同じ軸を指定した場合は平均を取る）。
///          追従先オブジェクトが1つも指定されていない場合は、このコンポーネントが付与された
///          オブジェクト自身のTransformを追従先として扱う。
class CameraController final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraController, 1,
        ADD_MEMBER_VARIABLE(positionOffset_);
        ADD_MEMBER_VARIABLE(rotationOffset_);
        ADD_MEMBER_VARIABLE(targetFovY_);
        ADD_MEMBER_VARIABLE(fovLerpFactor_);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.all);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.perAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.all);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_.perAxis);
    )
    COMPONENT_CATEGORY("Render")
    ~CameraController() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraController>();
        ptr->followTargets_ = followTargets_;
        ptr->positionOffset_ = positionOffset_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->targetFovY_ = targetFovY_;
        ptr->moveLerpFactor_ = moveLerpFactor_;
        ptr->rotateLerpFactor_ = rotateLerpFactor_;
        ptr->fovLerpFactor_ = fovLerpFactor_;
        ptr->enableBoundsClamp_ = enableBoundsClamp_;
        ptr->boundsClampMode_ = boundsClampMode_;
        ptr->viewAreaDepth_ = viewAreaDepth_;
        ptr->boundsZoneObjectID_ = boundsZoneObjectID_;
        ptr->boundsTransitionDuration_ = boundsTransitionDuration_;
        ptr->boundsTransitionEaseType_ = boundsTransitionEaseType_;
        return ptr;
    }

    /// @brief 移動範囲制限（Camera Bounds Zone）のクランプ基準
    enum class BoundsClampMode {
        Position, ///< カメラ座標そのものをクランプする
        ViewArea, ///< カメラ前方ViewAreaDepth先での可視矩形（近似）がゾーンからはみ出さないようクランプする
    };

    /// @brief 追従先オブジェクト1件分の設定
    struct FollowTarget {
        UUID128 objectID{};
        bool followPositionX = true;
        bool followPositionY = true;
        bool followPositionZ = true;
        bool followRotationX = false;
        bool followRotationY = false;
        bool followRotationZ = false;
    };

    /// @brief 軸ごとの追従の強さ（all=全軸共通の値を使う、per-axis=軸ごとに個別の値を使う）
    struct AxisStrength {
        bool usePerAxis = false;
        float all = 0.1f;
        Vector3 perAxis{ 0.1f, 0.1f, 0.1f };

        float GetX() const noexcept { return usePerAxis ? perAxis.x : all; }
        float GetY() const noexcept { return usePerAxis ? perAxis.y : all; }
        float GetZ() const noexcept { return usePerAxis ? perAxis.z : all; }
    };

    /// @brief 同一オブジェクトから Camera3D コンポーネントが取得できるか（制御可能かどうか）
    bool IsControllable() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext && objectContext->GetComponent<Camera3D>() != nullptr;
    }

    //==================================================
    // 追従先オブジェクト
    //==================================================

    std::vector<FollowTarget> &GetFollowTargets() noexcept { return followTargets_; }
    const std::vector<FollowTarget> &GetFollowTargets() const noexcept { return followTargets_; }
    void AddFollowTarget(const UUID128 &objectID) {
        FollowTarget target;
        target.objectID = objectID;
        followTargets_.push_back(target);
    }
    void RemoveFollowTarget(size_t index) {
        if (index < followTargets_.size()) followTargets_.erase(followTargets_.begin() + index);
    }

    //==================================================
    // オフセット・目標値
    //==================================================

    void SetPositionOffset(const Vector3 &offset) { positionOffset_ = offset; }
    const Vector3 &GetPositionOffset() const noexcept { return positionOffset_; }
    /// @brief 回転オフセット（オイラー角、ラジアン）
    void SetRotationOffset(const Vector3 &offset) { rotationOffset_ = offset; }
    const Vector3 &GetRotationOffset() const noexcept { return rotationOffset_; }
    void SetTargetFovY(float fovY) { targetFovY_ = fovY; }
    float GetTargetFovY() const noexcept { return targetFovY_; }

    //==================================================
    // 追従の強さ
    //==================================================

    AxisStrength &GetMoveStrength() noexcept { return moveLerpFactor_; }
    const AxisStrength &GetMoveStrength() const noexcept { return moveLerpFactor_; }
    AxisStrength &GetRotateStrength() noexcept { return rotateLerpFactor_; }
    const AxisStrength &GetRotateStrength() const noexcept { return rotateLerpFactor_; }
    void SetFovLerpFactor(float factor) { fovLerpFactor_ = factor; }
    float GetFovLerpFactor() const noexcept { return fovLerpFactor_; }

    //==================================================
    // 移動範囲制限（Camera Bounds Zone）
    //==================================================

    void SetEnableBoundsClamp(bool enable) noexcept { enableBoundsClamp_ = enable; }
    bool GetEnableBoundsClamp() const noexcept { return enableBoundsClamp_; }
    void SetBoundsClampMode(BoundsClampMode mode) noexcept { boundsClampMode_ = mode; }
    BoundsClampMode GetBoundsClampMode() const noexcept { return boundsClampMode_; }
    /// @brief ViewArea基準クランプで使う、カメラ前方の奥行（この距離での可視矩形を基準にする）
    void SetViewAreaDepth(float depth) noexcept { viewAreaDepth_ = depth; }
    float GetViewAreaDepth() const noexcept { return viewAreaDepth_; }
    void SetBoundsZoneObjectID(const UUID128 &objectID) noexcept { boundsZoneObjectID_ = objectID; }
    const UUID128 &GetBoundsZoneObjectID() const noexcept { return boundsZoneObjectID_; }
    /// @brief アクティブなグループが切り替わった瞬間の、クランプ位置の不連続な変化を滑らかにする
    ///        遷移時間（秒）。0以下の場合は即座に切り替える（既定・従来動作）
    void SetBoundsTransitionDuration(float duration) noexcept { boundsTransitionDuration_ = duration; }
    float GetBoundsTransitionDuration() const noexcept { return boundsTransitionDuration_; }
    void SetBoundsTransitionEaseType(EaseType type) noexcept { boundsTransitionEaseType_ = type; }
    EaseType GetBoundsTransitionEaseType() const noexcept { return boundsTransitionEaseType_; }

protected:
    void Update() override {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *camera = objectContext->GetComponent<Camera3D>();
        if (!camera) return;
        auto *cameraTransform = objectContext->GetComponent<Transform>();
        if (!cameraTransform) return;

        auto *sceneContext = GetOwnerSceneContext();

        // 追従先が1つも指定されていない場合は自身のTransformを追従先として扱う
        std::vector<FollowTarget> fallbackTargets;
        const std::vector<FollowTarget> *targets = &followTargets_;
        if (followTargets_.empty()) {
            FollowTarget self;
            self.objectID = objectContext->GetObjectID();
            fallbackTargets.push_back(self);
            targets = &fallbackTargets;
        }

        Vector3 posSum{ 0.0f, 0.0f, 0.0f };
        int posCountX = 0, posCountY = 0, posCountZ = 0;
        Vector3 rotEulerSum{ 0.0f, 0.0f, 0.0f };
        int rotCountX = 0, rotCountY = 0, rotCountZ = 0;

        for (const auto &followTarget : *targets) {
            EmptyObject *targetObject = sceneContext ? sceneContext->GetSceneObject(followTarget.objectID) : nullptr;
            if (!targetObject) continue;
            auto *targetTransform = targetObject->GetComponent<Transform>();
            if (!targetTransform) continue;

            if (followTarget.followPositionX || followTarget.followPositionY || followTarget.followPositionZ) {
                const Matrix4x4 &worldMatrix = targetTransform->GetWorldMatrix();
                const Vector3 worldPosition(worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2]);
                if (followTarget.followPositionX) { posSum.x += worldPosition.x; ++posCountX; }
                if (followTarget.followPositionY) { posSum.y += worldPosition.y; ++posCountY; }
                if (followTarget.followPositionZ) { posSum.z += worldPosition.z; ++posCountZ; }
            }
            if (followTarget.followRotationX || followTarget.followRotationY || followTarget.followRotationZ) {
                Quaternion worldRotation = GetWorldRotationQuaternion(targetTransform);
                const Vector3 worldEuler = worldRotation.MakeEuler();
                if (followTarget.followRotationX) { rotEulerSum.x += worldEuler.x; ++rotCountX; }
                if (followTarget.followRotationY) { rotEulerSum.y += worldEuler.y; ++rotCountY; }
                if (followTarget.followRotationZ) { rotEulerSum.z += worldEuler.z; ++rotCountZ; }
            }
        }

        // 追従先が寄与しない軸は現在値を維持する（オフセットも寄与がある軸にのみ適用する）
        const Vector3 currentTranslate = cameraTransform->GetTranslate();
        Vector3 desiredTranslate = currentTranslate;
        // rawTargetPosition: positionOffset_を加算する前の、追従先そのものの座標（Bounds Zoneのグループ判定用）。
        // desiredTranslateは「カメラ自身の目標位置」であり、positionOffset_が加わっているため、そのままでは
        // ターゲットの実座標と大きくズレてしまい、境界矩形との位置関係を正しく判定できない
        Vector3 rawTargetPosition = currentTranslate;
        if (posCountX > 0) {
            const float avgX = posSum.x / static_cast<float>(posCountX);
            desiredTranslate.x = avgX + positionOffset_.x;
            rawTargetPosition.x = avgX;
        }
        if (posCountY > 0) {
            const float avgY = posSum.y / static_cast<float>(posCountY);
            desiredTranslate.y = avgY + positionOffset_.y;
            rawTargetPosition.y = avgY;
        }
        if (posCountZ > 0) {
            const float avgZ = posSum.z / static_cast<float>(posCountZ);
            desiredTranslate.z = avgZ + positionOffset_.z;
            rawTargetPosition.z = avgZ;
        }

        const Vector3 currentEuler = cameraTransform->GetRotate();
        Vector3 desiredEuler = currentEuler;
        if (rotCountX > 0) desiredEuler.x = rotEulerSum.x / static_cast<float>(rotCountX) + rotationOffset_.x;
        if (rotCountY > 0) desiredEuler.y = rotEulerSum.y / static_cast<float>(rotCountY) + rotationOffset_.y;
        if (rotCountZ > 0) desiredEuler.z = rotEulerSum.z / static_cast<float>(rotCountZ) + rotationOffset_.z;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        auto axisLerpT = [dt](float factor) { return std::clamp(factor * dt * 60.0f, 0.0f, 1.0f); };

        Vector3 newTranslate;
        newTranslate.x = currentTranslate.x + (desiredTranslate.x - currentTranslate.x) * axisLerpT(moveLerpFactor_.GetX());
        newTranslate.y = currentTranslate.y + (desiredTranslate.y - currentTranslate.y) * axisLerpT(moveLerpFactor_.GetY());
        newTranslate.z = currentTranslate.z + (desiredTranslate.z - currentTranslate.z) * axisLerpT(moveLerpFactor_.GetZ());

        // 移動範囲制限（Camera Bounds Zone）: ターゲットが今どのグループに属しているかで
        // アクティブなグループを切り替え、そのグループの矩形群に沿ってカメラ位置をクランプする
        if (enableBoundsClamp_) {
            EmptyObject *zoneObject = sceneContext ? sceneContext->GetSceneObject(boundsZoneObjectID_) : nullptr;
            auto *zone = zoneObject ? zoneObject->GetComponent<CameraBoundsZone3D>() : nullptr;
            auto *zoneTransform = zoneObject ? zoneObject->GetComponent<Transform>() : nullptr;
            if (zone && zoneTransform) {
                const Matrix4x4 invZoneWorld = zoneTransform->GetWorldMatrix().Inverse();
                // グループ判定はrawTargetPosition（追従先そのものの座標）で行う
                const Vector3 localTarget = rawTargetPosition.Transform(invZoneWorld);
                const int previousBoundsGroupIndex = activeBoundsGroupIndex_;
                if (activeBoundsGroupIndex_ < 0 || !zone->IsPointInGroup(static_cast<size_t>(activeBoundsGroupIndex_), localTarget)) {
                    // 矩形間に隙間があると、ターゲットがそこを通過する一瞬だけどのグループにも
                    // 属さなくなることがある。見つからない場合はactiveBoundsGroupIndex_を-1に
                    // 戻さず直前のグループを維持することで、その一瞬だけクランプが完全に外れて
                    // 位置が飛ぶ（せっかくの遷移処理も発動しない）事態を防ぐ
                    const int found = zone->FindGroupContaining(localTarget);
                    if (found >= 0) activeBoundsGroupIndex_ = found;
                }
                if (activeBoundsGroupIndex_ >= 0) {
                    Vector3 insetMin{ 0.0f, 0.0f, 0.0f };
                    Vector3 insetMax{ 0.0f, 0.0f, 0.0f };
                    if (boundsClampMode_ == BoundsClampMode::ViewArea) {
                        // 奥行viewAreaDepth_での可視矩形の半径をXZに適用する近似
                        // （カメラのYaw/Pitchに関わらず一律の半径を使う。Yはクランプ対象外）
                        const float halfHeight = viewAreaDepth_ * std::tan(camera->GetFovY() * 0.5f);
                        const float halfWidth = halfHeight * camera->GetAspectRatio();
                        insetMin = insetMax = Vector3(halfWidth, 0.0f, halfHeight);
                    }
                    // クランプはpositionOffset_を差し引いた基準点に対して行い、結果へオフセットを戻す
                    // （positionOffset_がどんな値でも、矩形は追従先が動き回るワールド空間の範囲として
                    // 一貫して扱えるようにするため。offsetが0ならreferencePointはnewTranslateと同じ）
                    const Vector3 referencePoint = newTranslate - positionOffset_;

                    // アクティブなグループが切り替わった瞬間はクランプ結果が不連続にジャンプしうるため、
                    // 指定の秒数・イージングで現在位置から新しいクランプ結果へ滑らかに遷移させる。
                    // 遷移の起点にはreferencePoint（今フレームの追従補間・moveLerpFactor_適用後の値）
                    // ではなく、前フレームで実際に表示されていた位置（currentTranslateから逆算）を使う。
                    // referencePointを使うと、切り替わり直前まで境界にピン留めされていた影響で
                    // 追従補間の目標（desiredTranslate）だけが大きく先行してしまっていた場合に、
                    // その追従補間分がそのまま遷移の起点に混入し、遷移アニメーションが始まる前に
                    // カメラが一瞬だけ意図しない位置へガクッと動いてしまう
                    if (activeBoundsGroupIndex_ != previousBoundsGroupIndex && boundsTransitionDuration_ > 0.0f) {
                        boundsTransitionElapsed_ = 0.0f;
                        boundsTransitionActive_ = true;
                        boundsTransitionStartValue_ = currentTranslate - positionOffset_;
                    }

                    const Vector3 localReference = referencePoint.Transform(invZoneWorld);
                    const Vector3 clampedLocal = zone->ClampToGroup(static_cast<size_t>(activeBoundsGroupIndex_), localReference, insetMin, insetMax);
                    Vector3 finalReferenceWorld = clampedLocal.Transform(zoneTransform->GetWorldMatrix());

                    if (boundsTransitionActive_) {
                        // 経過時間の加算より先にtを求めることで、遷移開始1フレーム目はt=0
                        // （完全に遷移前の位置）から始まるようにする
                        const float t = (boundsTransitionDuration_ > 0.0f)
                            ? std::clamp(boundsTransitionElapsed_ / boundsTransitionDuration_, 0.0f, 1.0f) : 1.0f;
                        finalReferenceWorld = Eased(boundsTransitionStartValue_, finalReferenceWorld, t, boundsTransitionEaseType_);
                        boundsTransitionElapsed_ += dt;
                        if (t >= 1.0f) boundsTransitionActive_ = false;
                    }

                    newTranslate = finalReferenceWorld + positionOffset_;
                }
            }
        }

        cameraTransform->SetTranslate(newTranslate);

        Vector3 newEuler;
        newEuler.x = LerpAngle(currentEuler.x, desiredEuler.x, axisLerpT(rotateLerpFactor_.GetX()));
        newEuler.y = LerpAngle(currentEuler.y, desiredEuler.y, axisLerpT(rotateLerpFactor_.GetY()));
        newEuler.z = LerpAngle(currentEuler.z, desiredEuler.z, axisLerpT(rotateLerpFactor_.GetZ()));
        cameraTransform->SetRotate(newEuler);

        const float currentFov = camera->GetFovY();
        camera->SetFovY(currentFov + (targetFovY_ - currentFov) * axisLerpT(fovLerpFactor_));
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextColored(IsControllable() ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            IsControllable() ? "Controllable (Camera3D found)" : "Not controllable (Camera3D component required)");
        ImGui::Separator();

        ImGui::TextUnformatted(TranslationC("component.cameracontroller.follow_targets"));
        int removeIndex = -1;
        for (size_t i = 0; i < followTargets_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &target = followTargets_[i];
            TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), target.objectID, true, false);
            ImGui::TextUnformatted(TranslationC("component.cameracontroller.follow_position"));
            ImGui::Checkbox("X##Pos", &target.followPositionX); ImGui::SameLine();
            ImGui::Checkbox("Y##Pos", &target.followPositionY); ImGui::SameLine();
            ImGui::Checkbox("Z##Pos", &target.followPositionZ);
            ImGui::TextUnformatted(TranslationC("component.cameracontroller.follow_rotation"));
            ImGui::Checkbox("X##Rot", &target.followRotationX); ImGui::SameLine();
            ImGui::Checkbox("Y##Rot", &target.followRotationY); ImGui::SameLine();
            ImGui::Checkbox("Z##Rot", &target.followRotationZ);
            if (ImGui::Button(TranslationLabel("component.cameracontroller.remove_target"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveFollowTarget(static_cast<size_t>(removeIndex));
        if (ImGui::Button(TranslationLabel("component.cameracontroller.add_follow_target"))) {
            followTargets_.push_back(FollowTarget{});
        }
        if (followTargets_.empty()) {
            ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.cameracontroller.no_target_following_own_transform"));
        }

        ImGui::Separator();
        ImGui::DragFloat3(TranslationLabel("component.cameracontroller.position_offset"), &positionOffset_.x, 0.01f);
        Vector3 rotationOffsetDeg(ToDegrees(rotationOffset_.x), ToDegrees(rotationOffset_.y), ToDegrees(rotationOffset_.z));
        if (ImGui::DragFloat3(TranslationLabel("component.cameracontroller.rotation_offset"), &rotationOffsetDeg.x, 0.1f)) {
            rotationOffset_ = Vector3(ToRadians(rotationOffsetDeg.x), ToRadians(rotationOffsetDeg.y), ToRadians(rotationOffsetDeg.z));
        }
        ImGui::DragFloat(TranslationLabel("component.cameracontroller.target_fovy"), &targetFovY_, 0.01f, 0.01f, GetPI<float>() - 0.01f);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.cameracontroller.move_follow_strength"));
        ImGui::Checkbox((std::string(TranslationC("component.cameracontroller.move_per_axis")) + "##Move").c_str(), &moveLerpFactor_.usePerAxis);
        if (moveLerpFactor_.usePerAxis) {
            ImGui::DragFloat3(TranslationLabel("component.cameracontroller.move_x_y_z"), &moveLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat(TranslationLabel("component.cameracontroller.move_all"), &moveLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }

        ImGui::TextUnformatted(TranslationC("component.cameracontroller.rotate_follow_strength"));
        ImGui::Checkbox((std::string(TranslationC("component.cameracontroller.rotate_per_axis")) + "##Rotate").c_str(), &rotateLerpFactor_.usePerAxis);
        if (rotateLerpFactor_.usePerAxis) {
            ImGui::DragFloat3(TranslationLabel("component.cameracontroller.rotate_x_y_z"), &rotateLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat(TranslationLabel("component.cameracontroller.rotate_all"), &rotateLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }

        ImGui::Separator();
        ImGui::DragFloat(TranslationLabel("component.cameracontroller.fov_transition_strength"), &fovLerpFactor_, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.cameracontroller.bounds_clamp"));
        ImGui::Checkbox(TranslationLabel("component.cameracontroller.bounds_clamp_enable"), &enableBoundsClamp_);
        if (enableBoundsClamp_) {
            TargetObjectSelector::ShowSelector(TranslationLabel("component.cameracontroller.bounds_zone_object"), GetOwnerSceneContext(), boundsZoneObjectID_, true, false);
            int clampModeInt = static_cast<int>(boundsClampMode_);
            if (ImGui::RadioButton(TranslationLabel("component.cameracontroller.bounds_clamp_mode_position"), clampModeInt == static_cast<int>(BoundsClampMode::Position))) {
                boundsClampMode_ = BoundsClampMode::Position;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("component.cameracontroller.bounds_clamp_mode_viewarea"), clampModeInt == static_cast<int>(BoundsClampMode::ViewArea))) {
                boundsClampMode_ = BoundsClampMode::ViewArea;
            }
            if (boundsClampMode_ == BoundsClampMode::ViewArea) {
                ImGui::DragFloat(TranslationLabel("component.cameracontroller.bounds_clamp_view_depth"), &viewAreaDepth_, 0.1f, 0.01f, 10000.0f);
            }
            ImGui::DragFloat(TranslationLabel("component.cameracontroller.bounds_transition_duration"), &boundsTransitionDuration_, 0.01f, 0.0f, 10.0f);
            ImGui::SetItemTooltip("%s", TranslationC("component.cameracontroller.bounds_transition_duration.tooltip"));
            static constexpr const char *kEaseTypeComboNames[] = {
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
            int easeIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(kEaseTypeComboNames); ++i) {
                if (EaseTypeToString(boundsTransitionEaseType_) == std::string(kEaseTypeComboNames[i])) { easeIndex = i; break; }
            }
            if (ImGui::Combo(TranslationLabel("component.cameracontroller.bounds_transition_ease"), &easeIndex, kEaseTypeComboNames, IM_ARRAYSIZE(kEaseTypeComboNames))) {
                boundsTransitionEaseType_ = StringToEaseType(kEaseTypeComboNames[easeIndex]);
            }
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();

        JSON targetsJson = JSON::array();
        for (const auto &target : followTargets_) {
            JSON t = JSON::object();
            t["objectID"] = ToJSON(target.objectID);
            t["followPositionX"] = target.followPositionX;
            t["followPositionY"] = target.followPositionY;
            t["followPositionZ"] = target.followPositionZ;
            t["followRotationX"] = target.followRotationX;
            t["followRotationY"] = target.followRotationY;
            t["followRotationZ"] = target.followRotationZ;
            targetsJson.push_back(t);
        }
        json["followTargets"] = targetsJson;

        json["positionOffset"] = ToJSON(positionOffset_);
        json["rotationOffset"] = ToJSON(rotationOffset_);
        json["targetFovY"] = targetFovY_;

        json["moveLerpUsePerAxis"] = moveLerpFactor_.usePerAxis;
        json["moveLerpAll"] = moveLerpFactor_.all;
        json["moveLerpPerAxis"] = ToJSON(moveLerpFactor_.perAxis);

        json["rotateLerpUsePerAxis"] = rotateLerpFactor_.usePerAxis;
        json["rotateLerpAll"] = rotateLerpFactor_.all;
        json["rotateLerpPerAxis"] = ToJSON(rotateLerpFactor_.perAxis);

        json["fovLerpFactor"] = fovLerpFactor_;

        json["enableBoundsClamp"] = enableBoundsClamp_;
        json["boundsClampMode"] = static_cast<int>(boundsClampMode_);
        json["viewAreaDepth"] = viewAreaDepth_;
        json["boundsZoneObjectID"] = ToJSON(boundsZoneObjectID_);
        json["boundsTransitionDuration"] = boundsTransitionDuration_;
        json["boundsTransitionEaseType"] = EaseTypeToString(boundsTransitionEaseType_);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        followTargets_.clear();
        if (json.contains("followTargets")) {
            for (const auto &t : json["followTargets"]) {
                FollowTarget target;
                if (t.contains("objectID")) target.objectID = FromJSON<UUID128>(t["objectID"]);
                target.followPositionX = t.value("followPositionX", true);
                target.followPositionY = t.value("followPositionY", true);
                target.followPositionZ = t.value("followPositionZ", true);
                target.followRotationX = t.value("followRotationX", false);
                target.followRotationY = t.value("followRotationY", false);
                target.followRotationZ = t.value("followRotationZ", false);
                followTargets_.push_back(target);
            }
        }

        if (json.contains("positionOffset")) positionOffset_ = FromJSON<Vector3>(json["positionOffset"]);
        if (json.contains("rotationOffset")) rotationOffset_ = FromJSON<Vector3>(json["rotationOffset"]);
        targetFovY_ = json.value("targetFovY", 0.45f);

        moveLerpFactor_.usePerAxis = json.value("moveLerpUsePerAxis", false);
        moveLerpFactor_.all = json.value("moveLerpAll", 0.1f);
        if (json.contains("moveLerpPerAxis")) moveLerpFactor_.perAxis = FromJSON<Vector3>(json["moveLerpPerAxis"]);

        rotateLerpFactor_.usePerAxis = json.value("rotateLerpUsePerAxis", false);
        rotateLerpFactor_.all = json.value("rotateLerpAll", 0.1f);
        if (json.contains("rotateLerpPerAxis")) rotateLerpFactor_.perAxis = FromJSON<Vector3>(json["rotateLerpPerAxis"]);

        fovLerpFactor_ = json.value("fovLerpFactor", 0.1f);

        enableBoundsClamp_ = json.value("enableBoundsClamp", false);
        boundsClampMode_ = static_cast<BoundsClampMode>(json.value("boundsClampMode", static_cast<int>(BoundsClampMode::Position)));
        viewAreaDepth_ = json.value("viewAreaDepth", 10.0f);
        if (json.contains("boundsZoneObjectID")) boundsZoneObjectID_ = FromJSON<UUID128>(json["boundsZoneObjectID"]);
        boundsTransitionDuration_ = json.value("boundsTransitionDuration", 0.0f);
        boundsTransitionEaseType_ = StringToEaseType(json.value("boundsTransitionEaseType", std::string("Linear")));
        return true;
    }

private:
    /// @brief Transformの親子関係をたどり、ワールド回転（クォータニオン）を合成する
    static Quaternion GetWorldRotationQuaternion(Transform *transform) {
        if (!transform) return Quaternion::Identity();
        const Quaternion local = transform->GetRotateQuaternion();
        EmptyObject *parentObject = transform->GetParentObject();
        Transform *parentTransform = parentObject ? parentObject->GetComponent<Transform>() : nullptr;
        if (parentTransform) {
            return GetWorldRotationQuaternion(parentTransform) * local;
        }
        return local;
    }

    /// @brief 角度（ラジアン）を最短経路で補間する
    static float LerpAngle(float current, float target, float t) {
        constexpr float kPi = GetPI<float>();
        float delta = target - current;
        while (delta > kPi) delta -= 2.0f * kPi;
        while (delta < -kPi) delta += 2.0f * kPi;
        return current + delta * t;
    }

    std::vector<FollowTarget> followTargets_;

    Vector3 positionOffset_{ 0.0f, 0.0f, 0.0f };
    /// @brief 回転オフセット（オイラー角、ラジアン）
    Vector3 rotationOffset_{ 0.0f, 0.0f, 0.0f };
    float targetFovY_ = 0.45f;

    AxisStrength moveLerpFactor_{};
    AxisStrength rotateLerpFactor_{};
    float fovLerpFactor_ = 0.1f;

    //==================================================
    // 移動範囲制限（Camera Bounds Zone）
    //==================================================

    bool enableBoundsClamp_ = false;
    BoundsClampMode boundsClampMode_ = BoundsClampMode::Position;
    /// @brief ViewArea基準クランプで使う、カメラ前方の奥行
    float viewAreaDepth_ = 10.0f;
    UUID128 boundsZoneObjectID_{};
    /// @brief アクティブなグループが切り替わった際の遷移時間（秒）。0以下なら即座に切り替える
    float boundsTransitionDuration_ = 0.0f;
    EaseType boundsTransitionEaseType_ = EaseType::Linear;
    /// @brief 現在アクティブなグループの添字（実行時のみ・非シリアライズ、未確定は-1）
    int activeBoundsGroupIndex_ = -1;
    /// @brief グループ切り替え遷移の実行時状態（非シリアライズ）
    bool boundsTransitionActive_ = false;
    float boundsTransitionElapsed_ = 0.0f;
    Vector3 boundsTransitionStartValue_{ 0.0f, 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(CameraController)

} // namespace KashipanEngine
