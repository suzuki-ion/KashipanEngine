#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Math/Vector3.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Transform.h"
#include "Objects/EmptyObject.h"
#include "Objects/ObjectComponentHeader.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief Transformベースの3Dキャラクターを、衝突しない範囲だけ移動させるコンポーネント
/// @details CharacterController2Dの3D版。Moveで受け取った移動要求は即座には適用せず、
///          全Object更新後のSceneObjectCollider更新時にまとめて解決する。これにより、
///          そのフレームの全コライダーを同期した後で反復的なcollide-and-slide解決を実行でき、
///          衝突コールバック順に依存しない。
///
///          移動形状はBox/Sphere/Capsuleに対応する（Mesh/ConvexMesh/HeightFieldは非対応）。
///          Boxは回転（斜面ランプ等）も含めて実際の向きのOBBとして厳密に判定される
///          （Collider::MoveCharacter3DがCollisionAlgorithms3DのSAT/最近接点ベース判定を使用）。
///          Capsuleのみ、厳密形状での判定手段が無いため回転を反映したバウンディングOBBで近似する。
///          公開APIと結果型は形状非依存にしてある。
class CharacterController3D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CharacterController3D, 1,
        ADD_MEMBER_VARIABLE(skinWidth_);
        ADD_MEMBER_VARIABLE(groundedThreshold_);
    )
    COMPONENT_CATEGORY("Collision")
    ~CharacterController3D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CharacterController3D>();
        ptr->skinWidth_ = skinWidth_;
        ptr->groundedThreshold_ = groundedThreshold_;
        ptr->selectedColliderTypeName_ = selectedColliderTypeName_;
        ptr->selectedColliderOccurrenceIndex_ = selectedColliderOccurrenceIndex_;
        ptr->ignoredTags_ = ignoredTags_;
        return ptr;
    }

    /// @brief このフレームに行いたい移動を予約する（複数回呼んだ場合は加算）
    void Move(const Vector3 &displacement) {
        pendingDisplacement_ = pendingDisplacement_ + displacement;
        hasPendingMove_ = true;
    }

    void SetSkinWidth(float width) noexcept { skinWidth_ = std::max(0.0f, width); }
    float GetSkinWidth() const noexcept { return skinWidth_; }
    void SetGroundedThreshold(float threshold) noexcept { groundedThreshold_ = std::clamp(threshold, 0.0f, 1.0f); }
    float GetGroundedThreshold() const noexcept { return groundedThreshold_; }

    bool IsGrounded() const noexcept { return HasFlag(CharacterCollisionFlags3D::Below); }
    bool IsTouchingCeiling() const noexcept { return HasFlag(CharacterCollisionFlags3D::Above); }
    bool IsTouchingPosX() const noexcept { return HasFlag(CharacterCollisionFlags3D::PosX); }
    bool IsTouchingNegX() const noexcept { return HasFlag(CharacterCollisionFlags3D::NegX); }
    bool IsTouchingPosZ() const noexcept { return HasFlag(CharacterCollisionFlags3D::PosZ); }
    bool IsTouchingNegZ() const noexcept { return HasFlag(CharacterCollisionFlags3D::NegZ); }
    bool IsTouchingWall() const noexcept {
        return IsTouchingPosX() || IsTouchingNegX() || IsTouchingPosZ() || IsTouchingNegZ();
    }
    bool WasLastMoveShapeSupported() const noexcept { return lastMoveResult_.shapeSupported; }
    Vector3 GetGroundNormal() const noexcept { return lastMoveResult_.groundNormal; }
    Vector3 GetRequestedDelta() const noexcept { return lastMoveResult_.requestedDelta; }
    Vector3 GetAppliedDelta() const noexcept { return lastMoveResult_.appliedDelta; }
    /// @brief 直近の移動解決で接地(Below)と判定された障害物の所有オブジェクトを取得する
    /// @details 動く床への追従など、「今何に乗っているか」が必要なスクリプト側で使う。
    ///          接地していない場合はnullptr
    EmptyObject *GetGroundObject() const noexcept { return lastMoveResult_.groundObject; }
    /// @brief 直近の移動解決で接地(Below)と判定された障害物のコライダーを取得する（無ければnullptr）
    ICollider *GetGroundCollider() const noexcept { return lastMoveResult_.groundCollider; }

    /// @brief 同一オブジェクト上の移動形状として使用する3Dコライダーを選択する
    void SetSelectedCollider(ICollider *collider) {
        if (!collider) {
            selectedColliderTypeName_.clear();
            selectedColliderOccurrenceIndex_ = 0;
            return;
        }
        int occurrence = 0;
        for (auto *candidate : GetOwnerColliders()) {
            if (candidate == collider) {
                selectedColliderTypeName_ = candidate->GetComponentType();
                selectedColliderOccurrenceIndex_ = occurrence;
                return;
            }
            if (candidate->GetComponentType() == collider->GetComponentType()) ++occurrence;
        }
    }

    ICollider *GetSelectedCollider() const {
        if (selectedColliderTypeName_.empty()) return nullptr;
        int occurrence = 0;
        for (auto *candidate : GetOwnerColliders()) {
            if (candidate->GetComponentType() != selectedColliderTypeName_) continue;
            if (occurrence == selectedColliderOccurrenceIndex_) return candidate;
            ++occurrence;
        }
        return nullptr;
    }

    std::vector<ICollider *> GetOwnerColliders() const {
        std::vector<ICollider *> result;
        auto *context = GetOwnerObjectContext();
        if (!context) return result;
        for (const auto &pair : context->GetAllComponents()) {
            if (auto *collider = dynamic_cast<ICollider *>(pair.first); collider && !collider->Is2D()) {
                result.push_back(collider);
            }
        }
        return result;
    }

    void AddIgnoredTag(const std::string &tag) {
        if (tag.empty() || std::find(ignoredTags_.begin(), ignoredTags_.end(), tag) != ignoredTags_.end()) return;
        ignoredTags_.push_back(tag);
    }
    void RemoveIgnoredTag(const std::string &tag) {
        ignoredTags_.erase(std::remove(ignoredTags_.begin(), ignoredTags_.end(), tag), ignoredTags_.end());
    }
    void ClearIgnoredTags() { ignoredTags_.clear(); }
    bool IsTagIgnored(const std::string &tag) const {
        return std::find(ignoredTags_.begin(), ignoredTags_.end(), tag) != ignoredTags_.end();
    }

    /// @brief SceneObjectColliderから呼ばれ、保留中の移動を同フレーム内で確定する
    void ResolvePendingMove(Collider &collider) {
        if (!hasPendingMove_) return;

        ICollider *shape = GetSelectedCollider();
        if (!shape) {
            const auto colliders = GetOwnerColliders();
            if (!colliders.empty()) shape = colliders.front();
        }

        lastMoveResult_ = collider.MoveCharacter3D(
            shape,
            pendingDisplacement_,
            skinWidth_,
            groundedThreshold_,
            ignoredTags_);

        if (lastMoveResult_.shapeSupported) {
            auto *context = GetOwnerObjectContext();
            auto *transform = context ? context->GetComponent<Transform>() : nullptr;
            if (transform) {
                const Vector3 current = transform->GetTranslate();
                transform->SetTranslate(current + lastMoveResult_.appliedDelta);
            }
        }

        pendingDisplacement_ = Vector3{0.0f, 0.0f, 0.0f};
        hasPendingMove_ = false;
    }

protected:
    void Initialize() override { TryRegister(); }

    void Update() override { TryRegister(); }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
        if (sceneCollider) sceneCollider->UnregisterCharacterController3D(this);
        registered_ = false;
        pendingDisplacement_ = Vector3{0.0f, 0.0f, 0.0f};
        hasPendingMove_ = false;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGui::DragFloat("Skin Width", &skinWidth_, 0.001f, 0.0f, 10.0f)) {
            SetSkinWidth(skinWidth_);
        }
        if (ImGui::DragFloat("Grounded Threshold", &groundedThreshold_, 0.01f, 0.0f, 1.0f)) {
            SetGroundedThreshold(groundedThreshold_);
        }

        const auto colliders = GetOwnerColliders();
        auto *current = GetSelectedCollider();
        const std::string preview = current ? current->GetComponentType() : "(First 3D Collider)";
        if (ImGui::BeginCombo("Collider Shape", preview.c_str())) {
            if (ImGui::Selectable("(First 3D Collider)", !current)) SetSelectedCollider(nullptr);
            for (auto *collider : colliders) {
                ImGui::PushID(collider);
                const bool selected = collider == current;
                if (ImGui::Selectable(collider->GetComponentType().c_str(), selected)) SetSelectedCollider(collider);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        if (current && current->GetShape() != ICollider::Shape::Box &&
            current->GetShape() != ICollider::Shape::Sphere &&
            current->GetShape() != ICollider::Shape::Capsule) {
            ImGuiCustom::TextDisabledWrapped("Current version supports Box/Sphere/Capsule colliders only.");
        }
    }
#endif

    JSON SaveToJson() const override {
        return JSON{
            {"skinWidth", skinWidth_},
            {"groundedThreshold", groundedThreshold_},
            {"selectedColliderTypeName", selectedColliderTypeName_},
            {"selectedColliderOccurrenceIndex", selectedColliderOccurrenceIndex_},
            {"ignoredTags", ignoredTags_},
        };
    }

    bool LoadFromJson(const JSON &json) override {
        SetSkinWidth(json.value("skinWidth", 0.01f));
        SetGroundedThreshold(json.value("groundedThreshold", 0.5f));
        selectedColliderTypeName_ = json.value("selectedColliderTypeName", std::string{});
        selectedColliderOccurrenceIndex_ = json.value("selectedColliderOccurrenceIndex", 0);
        ignoredTags_ = json.value("ignoredTags", std::vector<std::string>{});
        return true;
    }

private:
    bool HasFlag(CharacterCollisionFlags3D flag) const noexcept {
        return (lastMoveResult_.collisionFlags & static_cast<std::uint8_t>(flag)) != 0;
    }

    void TryRegister() {
        if (registered_) return;
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return;
        auto *sceneCollider = sceneContext->GetComponent<SceneObjectCollider>();
        if (!sceneCollider) sceneCollider = sceneContext->AddComponent<SceneObjectCollider>();
        if (!sceneCollider) return;
        sceneCollider->RegisterCharacterController3D(this);
        registered_ = true;
    }

    float skinWidth_ = 0.01f;
    float groundedThreshold_ = 0.5f;
    std::string selectedColliderTypeName_;
    int selectedColliderOccurrenceIndex_ = 0;
    std::vector<std::string> ignoredTags_;

    Vector3 pendingDisplacement_{0.0f, 0.0f, 0.0f};
    CharacterMoveResult3D lastMoveResult_{};
    bool hasPendingMove_ = false;
    bool registered_ = false;
};

REGISTER_COMPONENT_OBJECT(CharacterController3D)

} // namespace KashipanEngine
