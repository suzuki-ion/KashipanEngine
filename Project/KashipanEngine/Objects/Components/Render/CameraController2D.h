#pragma once
#include <algorithm>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/CameraBoundsZone2D.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
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

/// @brief 同一オブジェクトの Camera2D を、複数の追従先オブジェクトへ滑らかに追従させるコンポーネント
/// @details CameraController（3D用）の2D版。追従に用いるのは位置X/Y・回転Z（画面内の回転）のみで、
///          FovYの代わりにCamera2Dのwidth/height（表示範囲＝ズーム相当）を目標値へ遷移させる。
///          追従先オブジェクトは複数指定可能で、それぞれ移動(x,y)・回転(z)のどちらを追従に
///          用いるかを個別に選択できる（複数の追従先が同じ軸を指定した場合は平均を取る）。
///          追従先オブジェクトが1つも指定されていない場合は、このコンポーネントが付与された
///          オブジェクト自身のTransformを追従先として扱う。
class CameraController2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraController2D, 1,
        ADD_MEMBER_VARIABLE(positionOffset_);
        ADD_MEMBER_VARIABLE(rotationOffset_);
        ADD_MEMBER_VARIABLE(targetSize_);
        ADD_MEMBER_VARIABLE(sizeLerpFactor_);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.usePerAxis);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.all);
        ADD_MEMBER_VARIABLE(moveLerpFactor_.perAxis);
        ADD_MEMBER_VARIABLE(rotateLerpFactor_);
    )
    COMPONENT_CATEGORY("Render")
    ~CameraController2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraController2D>();
        ptr->followTargets_ = followTargets_;
        ptr->positionOffset_ = positionOffset_;
        ptr->rotationOffset_ = rotationOffset_;
        ptr->targetSize_ = targetSize_;
        ptr->moveLerpFactor_ = moveLerpFactor_;
        ptr->rotateLerpFactor_ = rotateLerpFactor_;
        ptr->sizeLerpFactor_ = sizeLerpFactor_;
        ptr->enableBoundsClamp_ = enableBoundsClamp_;
        ptr->boundsClampMode_ = boundsClampMode_;
        ptr->boundsZoneObjectID_ = boundsZoneObjectID_;
        ptr->boundsTransitionDuration_ = boundsTransitionDuration_;
        ptr->boundsTransitionEaseType_ = boundsTransitionEaseType_;
        return ptr;
    }

    /// @brief 移動範囲制限（Camera Bounds Zone）のクランプ基準
    enum class BoundsClampMode {
        Position, ///< カメラ座標そのものをクランプする
        ViewArea, ///< カメラが今映している範囲（表示矩形）がゾーンからはみ出さないようクランプする
    };

    /// @brief 追従先オブジェクト1件分の設定
    struct FollowTarget {
        UUID128 objectID{};
        bool followPositionX = true;
        bool followPositionY = true;
        bool followRotationZ = false;
    };

    /// @brief 軸ごとの移動追従の強さ（all=全軸共通の値を使う、per-axis=軸ごとに個別の値を使う）
    struct AxisStrength2 {
        bool usePerAxis = false;
        float all = 0.1f;
        Vector2 perAxis{ 0.1f, 0.1f };

        float GetX() const noexcept { return usePerAxis ? perAxis.x : all; }
        float GetY() const noexcept { return usePerAxis ? perAxis.y : all; }
    };

    /// @brief 同一オブジェクトから Camera2D コンポーネントが取得できるか（制御可能かどうか）
    bool IsControllable() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext && objectContext->GetComponent<Camera2D>() != nullptr;
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

    void SetPositionOffset(const Vector2 &offset) { positionOffset_ = offset; }
    const Vector2 &GetPositionOffset() const noexcept { return positionOffset_; }
    /// @brief 回転オフセット（ラジアン、画面内回転＝Z軸のみ）
    void SetRotationOffset(float offset) { rotationOffset_ = offset; }
    float GetRotationOffset() const noexcept { return rotationOffset_; }
    /// @brief Camera2Dの目標width/height（ズーム相当）。対象のCamera2DでAutoSyncSizeが
    ///        有効な場合は毎フレーム描画先解像度へ上書きされるため実質無効になる点に注意
    void SetTargetSize(const Vector2 &size) { targetSize_ = size; }
    const Vector2 &GetTargetSize() const noexcept { return targetSize_; }

    //==================================================
    // 追従の強さ
    //==================================================

    AxisStrength2 &GetMoveStrength() noexcept { return moveLerpFactor_; }
    const AxisStrength2 &GetMoveStrength() const noexcept { return moveLerpFactor_; }
    void SetRotateLerpFactor(float factor) { rotateLerpFactor_ = factor; }
    float GetRotateLerpFactor() const noexcept { return rotateLerpFactor_; }
    void SetSizeLerpFactor(float factor) { sizeLerpFactor_ = factor; }
    float GetSizeLerpFactor() const noexcept { return sizeLerpFactor_; }

    //==================================================
    // 移動範囲制限（Camera Bounds Zone）
    //==================================================

    void SetEnableBoundsClamp(bool enable) noexcept { enableBoundsClamp_ = enable; }
    bool GetEnableBoundsClamp() const noexcept { return enableBoundsClamp_; }
    void SetBoundsClampMode(BoundsClampMode mode) noexcept { boundsClampMode_ = mode; }
    BoundsClampMode GetBoundsClampMode() const noexcept { return boundsClampMode_; }
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
        auto *camera = objectContext->GetComponent<Camera2D>();
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

        Vector2 posSum{ 0.0f, 0.0f };
        int posCountX = 0, posCountY = 0;
        float rotSum = 0.0f;
        int rotCount = 0;

        for (const auto &followTarget : *targets) {
            EmptyObject *targetObject = sceneContext ? sceneContext->GetSceneObject(followTarget.objectID) : nullptr;
            if (!targetObject) continue;
            auto *targetTransform = targetObject->GetComponent<Transform>();
            if (!targetTransform) continue;

            if (followTarget.followPositionX || followTarget.followPositionY) {
                const Matrix4x4 &worldMatrix = targetTransform->GetWorldMatrix();
                if (followTarget.followPositionX) { posSum.x += worldMatrix.m[3][0]; ++posCountX; }
                if (followTarget.followPositionY) { posSum.y += worldMatrix.m[3][1]; ++posCountY; }
            }
            if (followTarget.followRotationZ) {
                Quaternion worldRotation = GetWorldRotationQuaternion(targetTransform);
                rotSum += worldRotation.MakeEuler().z;
                ++rotCount;
            }
        }

        // 追従先が寄与しない軸は現在値を維持する（オフセットも寄与がある軸にのみ適用する）
        const Vector3 currentTranslate = cameraTransform->GetTranslate();
        Vector3 desiredTranslate = currentTranslate;
        // rawTargetPosition: positionOffset_を加算する前の、追従先そのものの座標（Bounds Zoneのグループ判定用）。
        // desiredTranslateは「カメラ自身の目標位置」であり、positionOffset_（画面中央合わせ等で
        // -size/2のような値を設定するのが一般的）が加わっているため、そのままではターゲットの実座標と
        // 大きくズレてしまい、境界矩形との位置関係を正しく判定できない
        Vector2 rawTargetPosition(currentTranslate.x, currentTranslate.y);
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

        const Vector3 currentEuler = cameraTransform->GetRotate();
        float desiredRotZ = currentEuler.z;
        if (rotCount > 0) desiredRotZ = rotSum / static_cast<float>(rotCount) + rotationOffset_;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        auto axisLerpT = [dt](float factor) { return std::clamp(factor * dt * 60.0f, 0.0f, 1.0f); };

        Vector3 newTranslate = currentTranslate;
        newTranslate.x = currentTranslate.x + (desiredTranslate.x - currentTranslate.x) * axisLerpT(moveLerpFactor_.GetX());
        newTranslate.y = currentTranslate.y + (desiredTranslate.y - currentTranslate.y) * axisLerpT(moveLerpFactor_.GetY());

        // 移動範囲制限（Camera Bounds Zone）: ターゲットが今どのグループに属しているかで
        // アクティブなグループを切り替え、そのグループの矩形群に沿ってカメラ位置をクランプする
        if (enableBoundsClamp_) {
            EmptyObject *zoneObject = sceneContext ? sceneContext->GetSceneObject(boundsZoneObjectID_) : nullptr;
            auto *zone = zoneObject ? zoneObject->GetComponent<CameraBoundsZone2D>() : nullptr;
            auto *zoneTransform = zoneObject ? zoneObject->GetComponent<Transform>() : nullptr;
            if (zone && zoneTransform) {
                const Matrix4x4 invZoneWorld = zoneTransform->GetWorldMatrix().Inverse();
                // グループ判定はrawTargetPosition（追従先そのものの座標）で行う
                const Vector3 localTarget3 = Vector3(rawTargetPosition.x, rawTargetPosition.y, 0.0f).Transform(invZoneWorld);
                const Vector2 localTarget(localTarget3.x, localTarget3.y);
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
                    Vector2 insetMin{ 0.0f, 0.0f };
                    Vector2 insetMax{ 0.0f, 0.0f };
                    if (boundsClampMode_ == BoundsClampMode::ViewArea) {
                        // Camera2Dのtranslateは表示範囲の中心ではなく左上角を表すため、表示範囲の半径ぶんを
                        // 対称にinsetするのではなく、positionOffset_を差し引いた「基準点」（＝positionOffset_を
                        // size/2の負値に設定する一般的な中央追従構成では表示範囲の中心と一致する）に対して
                        // 対称にinsetする。これによりtranslateの左上角基準という実装の都合を吸収する
                        insetMin = insetMax = Vector2(camera->GetWidth() * 0.5f, camera->GetHeight() * 0.5f);
                    }
                    // クランプはpositionOffset_を差し引いた基準点に対して行い、結果へオフセットを戻す
                    // （positionOffset_がどんな値でも、矩形は追従先が動き回るワールド空間の範囲として
                    // 一貫して扱えるようにするため。offsetが0ならreferencePointはnewTranslateと同じ）
                    const Vector2 referencePoint(newTranslate.x - positionOffset_.x, newTranslate.y - positionOffset_.y);

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
                        boundsTransitionStartValue_ = Vector2(currentTranslate.x - positionOffset_.x, currentTranslate.y - positionOffset_.y);
                    }

                    const Vector3 localReference3 = Vector3(referencePoint.x, referencePoint.y, 0.0f).Transform(invZoneWorld);
                    const Vector2 clampedLocal = zone->ClampToGroup(static_cast<size_t>(activeBoundsGroupIndex_),
                        Vector2(localReference3.x, localReference3.y), insetMin, insetMax);
                    const Vector3 clampedReferenceWorld3 = Vector3(clampedLocal.x, clampedLocal.y, 0.0f).Transform(zoneTransform->GetWorldMatrix());
                    Vector2 finalReferenceWorld(clampedReferenceWorld3.x, clampedReferenceWorld3.y);

                    if (boundsTransitionActive_) {
                        // 経過時間の加算より先にtを求めることで、遷移開始1フレーム目はt=0
                        // （完全に遷移前の位置）から始まるようにする
                        const float t = (boundsTransitionDuration_ > 0.0f)
                            ? std::clamp(boundsTransitionElapsed_ / boundsTransitionDuration_, 0.0f, 1.0f) : 1.0f;
                        finalReferenceWorld = Eased(boundsTransitionStartValue_, finalReferenceWorld, t, boundsTransitionEaseType_);
                        boundsTransitionElapsed_ += dt;
                        if (t >= 1.0f) boundsTransitionActive_ = false;
                    }

                    newTranslate.x = finalReferenceWorld.x + positionOffset_.x;
                    newTranslate.y = finalReferenceWorld.y + positionOffset_.y;
                }
            }
        }

        cameraTransform->SetTranslate(newTranslate);

        Vector3 newEuler = currentEuler;
        newEuler.z = LerpAngle(currentEuler.z, desiredRotZ, axisLerpT(rotateLerpFactor_));
        cameraTransform->SetRotate(newEuler);

        const float currentWidth = camera->GetWidth();
        const float currentHeight = camera->GetHeight();
        const float sizeT = axisLerpT(sizeLerpFactor_);
        camera->SetSize(
            currentWidth + (targetSize_.x - currentWidth) * sizeT,
            currentHeight + (targetSize_.y - currentHeight) * sizeT
        );
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextColored(IsControllable() ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            IsControllable() ? "Controllable (Camera2D found)" : "Not controllable (Camera2D component required)");
        ImGui::Separator();

        ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.follow_targets"));
        int removeIndex = -1;
        for (size_t i = 0; i < followTargets_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto &target = followTargets_[i];
            TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), target.objectID, true, false);
            ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.follow_position"));
            ImGui::Checkbox("X##Pos", &target.followPositionX); ImGui::SameLine();
            ImGui::Checkbox("Y##Pos", &target.followPositionY);
            ImGui::Checkbox(TranslationLabel("component.cameracontroller2d.follow_rotation"), &target.followRotationZ);
            if (ImGui::Button(TranslationLabel("component.cameracontroller2d.remove_target"))) removeIndex = static_cast<int>(i);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveFollowTarget(static_cast<size_t>(removeIndex));
        if (ImGui::Button(TranslationLabel("component.cameracontroller2d.add_follow_target"))) {
            followTargets_.push_back(FollowTarget{});
        }
        if (followTargets_.empty()) {
            ImGuiCustom::TextDisabledWrapped("%s", TranslationC("component.cameracontroller2d.no_target_following_own_transform"));
        }

        ImGui::Separator();
        ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.position_offset"), &positionOffset_.x, 0.01f);
        float rotationOffsetDeg = ToDegrees(rotationOffset_);
        if (ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.rotation_offset"), &rotationOffsetDeg, 0.1f)) {
            rotationOffset_ = ToRadians(rotationOffsetDeg);
        }
        ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.target_size"), &targetSize_.x, 1.0f);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.move_follow_strength"));
        ImGui::Checkbox((std::string(TranslationC("component.cameracontroller2d.move_per_axis")) + "##Move").c_str(), &moveLerpFactor_.usePerAxis);
        if (moveLerpFactor_.usePerAxis) {
            ImGui::DragFloat2(TranslationLabel("component.cameracontroller2d.move_x_y"), &moveLerpFactor_.perAxis.x, 0.01f, 0.0f, 1.0f);
        } else {
            ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.move_all"), &moveLerpFactor_.all, 0.01f, 0.0f, 1.0f);
        }
        ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.rotate_follow_strength"), &rotateLerpFactor_, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.size_transition_strength"), &sizeLerpFactor_, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.cameracontroller2d.bounds_clamp"));
        ImGui::Checkbox(TranslationLabel("component.cameracontroller2d.bounds_clamp_enable"), &enableBoundsClamp_);
        if (enableBoundsClamp_) {
            TargetObjectSelector::ShowSelector(TranslationLabel("component.cameracontroller2d.bounds_zone_object"), GetOwnerSceneContext(), boundsZoneObjectID_, true, false);
            int clampModeInt = static_cast<int>(boundsClampMode_);
            if (ImGui::RadioButton(TranslationLabel("component.cameracontroller2d.bounds_clamp_mode_position"), clampModeInt == static_cast<int>(BoundsClampMode::Position))) {
                boundsClampMode_ = BoundsClampMode::Position;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("component.cameracontroller2d.bounds_clamp_mode_viewarea"), clampModeInt == static_cast<int>(BoundsClampMode::ViewArea))) {
                boundsClampMode_ = BoundsClampMode::ViewArea;
            }
            ImGui::DragFloat(TranslationLabel("component.cameracontroller2d.bounds_transition_duration"), &boundsTransitionDuration_, 0.01f, 0.0f, 10.0f);
            ImGui::SetItemTooltip("%s", TranslationC("component.cameracontroller2d.bounds_transition_duration.tooltip"));
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
            if (ImGui::Combo(TranslationLabel("component.cameracontroller2d.bounds_transition_ease"), &easeIndex, kEaseTypeComboNames, IM_ARRAYSIZE(kEaseTypeComboNames))) {
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
            t["followRotationZ"] = target.followRotationZ;
            targetsJson.push_back(t);
        }
        json["followTargets"] = targetsJson;

        json["positionOffset"] = ToJSON(positionOffset_);
        json["rotationOffset"] = rotationOffset_;
        json["targetSize"] = ToJSON(targetSize_);

        json["moveLerpUsePerAxis"] = moveLerpFactor_.usePerAxis;
        json["moveLerpAll"] = moveLerpFactor_.all;
        json["moveLerpPerAxis"] = ToJSON(moveLerpFactor_.perAxis);

        json["rotateLerpFactor"] = rotateLerpFactor_;
        json["sizeLerpFactor"] = sizeLerpFactor_;

        json["enableBoundsClamp"] = enableBoundsClamp_;
        json["boundsClampMode"] = static_cast<int>(boundsClampMode_);
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
                target.followRotationZ = t.value("followRotationZ", false);
                followTargets_.push_back(target);
            }
        }

        if (json.contains("positionOffset")) positionOffset_ = FromJSON<Vector2>(json["positionOffset"]);
        rotationOffset_ = json.value("rotationOffset", 0.0f);
        if (json.contains("targetSize")) {
            targetSize_ = FromJSON<Vector2>(json["targetSize"]);
        } else {
            targetSize_ = Vector2(1280.0f, 720.0f);
        }

        moveLerpFactor_.usePerAxis = json.value("moveLerpUsePerAxis", false);
        moveLerpFactor_.all = json.value("moveLerpAll", 0.1f);
        if (json.contains("moveLerpPerAxis")) moveLerpFactor_.perAxis = FromJSON<Vector2>(json["moveLerpPerAxis"]);

        rotateLerpFactor_ = json.value("rotateLerpFactor", 0.1f);
        sizeLerpFactor_ = json.value("sizeLerpFactor", 0.1f);

        enableBoundsClamp_ = json.value("enableBoundsClamp", false);
        boundsClampMode_ = static_cast<BoundsClampMode>(json.value("boundsClampMode", static_cast<int>(BoundsClampMode::Position)));
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

    Vector2 positionOffset_{ 0.0f, 0.0f };
    /// @brief 回転オフセット（ラジアン、画面内回転＝Z軸のみ）
    float rotationOffset_ = 0.0f;
    /// @brief 目標のwidth/height（ズーム相当）。既定はCamera2Dの既定値と同じ
    Vector2 targetSize_{ 1280.0f, 720.0f };

    AxisStrength2 moveLerpFactor_{};
    float rotateLerpFactor_ = 0.1f;
    float sizeLerpFactor_ = 0.1f;

    //==================================================
    // 移動範囲制限（Camera Bounds Zone）
    //==================================================

    bool enableBoundsClamp_ = false;
    BoundsClampMode boundsClampMode_ = BoundsClampMode::Position;
    UUID128 boundsZoneObjectID_{};
    /// @brief アクティブなグループが切り替わった際の遷移時間（秒）。0以下なら即座に切り替える
    float boundsTransitionDuration_ = 0.0f;
    EaseType boundsTransitionEaseType_ = EaseType::Linear;
    /// @brief 現在アクティブなグループの添字（実行時のみ・非シリアライズ、未確定は-1）
    int activeBoundsGroupIndex_ = -1;
    /// @brief グループ切り替え遷移の実行時状態（非シリアライズ）
    bool boundsTransitionActive_ = false;
    float boundsTransitionElapsed_ = 0.0f;
    Vector2 boundsTransitionStartValue_{ 0.0f, 0.0f };
};

REGISTER_COMPONENT_OBJECT(CameraController2D)

} // namespace KashipanEngine
