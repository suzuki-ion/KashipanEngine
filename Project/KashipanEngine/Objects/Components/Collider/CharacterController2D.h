#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Transform.h"
#include "Objects/ObjectComponentHeader.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief Transformベースの2Dキャラクターを、衝突しない範囲だけ移動させるコンポーネント
/// @details Moveで受け取った移動要求は即座には適用せず、全Object更新後の
///          SceneObjectCollider更新時にまとめて解決する。これにより、そのフレームの全コライダーを
///          同期した後で X→Y 順のスイープを実行でき、衝突コールバック順に依存しない。
///
///          現在の移動形状は軸平行なBox2DColliderのみ。公開APIと結果型は形状非依存にしてあり、
///          Collider::MoveCharacter2D内部の形状アダプターを追加することで、将来Capsule2D/Circle2Dや
///          斜面追従へ拡張できる。
class CharacterController2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CharacterController2D, 1,
        ADD_MEMBER_VARIABLE(skinWidth_);
        ADD_MEMBER_VARIABLE(groundedThreshold_);
    )
    COMPONENT_CATEGORY("Collision")
    ~CharacterController2D() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CharacterController2D>();
        ptr->skinWidth_ = skinWidth_;
        ptr->groundedThreshold_ = groundedThreshold_;
        ptr->selectedColliderTypeName_ = selectedColliderTypeName_;
        ptr->selectedColliderOccurrenceIndex_ = selectedColliderOccurrenceIndex_;
        ptr->ignoredTags_ = ignoredTags_;
        return ptr;
    }

    /// @brief このフレームに行いたい移動を予約する（複数回呼んだ場合は加算）
    void Move(const Vector2 &displacement) {
        pendingDisplacement_ = pendingDisplacement_ + displacement;
        hasPendingMove_ = true;
    }

    void SetSkinWidth(float width) noexcept { skinWidth_ = std::max(0.0f, width); }
    float GetSkinWidth() const noexcept { return skinWidth_; }
    void SetGroundedThreshold(float threshold) noexcept { groundedThreshold_ = std::clamp(threshold, 0.0f, 1.0f); }
    float GetGroundedThreshold() const noexcept { return groundedThreshold_; }

    bool IsGrounded() const noexcept { return HasFlag(CharacterCollisionFlags2D::Below); }
    bool IsTouchingCeiling() const noexcept { return HasFlag(CharacterCollisionFlags2D::Above); }
    bool IsTouchingLeft() const noexcept { return HasFlag(CharacterCollisionFlags2D::Left); }
    bool IsTouchingRight() const noexcept { return HasFlag(CharacterCollisionFlags2D::Right); }
    bool IsTouchingWall() const noexcept { return IsTouchingLeft() || IsTouchingRight(); }
    bool WasLastMoveShapeSupported() const noexcept { return lastMoveResult_.shapeSupported; }
    Vector2 GetGroundNormal() const noexcept { return lastMoveResult_.groundNormal; }
    Vector2 GetRequestedDelta() const noexcept { return lastMoveResult_.requestedDelta; }
    Vector2 GetAppliedDelta() const noexcept { return lastMoveResult_.appliedDelta; }

    /// @brief 同一オブジェクト上の移動形状として使用する2Dコライダーを選択する
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
            if (auto *collider = dynamic_cast<ICollider *>(pair.first); collider && collider->Is2D()) {
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

        lastMoveResult_ = collider.MoveCharacter2D(
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
                transform->SetTranslate(Vector3{
                    current.x + lastMoveResult_.appliedDelta.x,
                    current.y + lastMoveResult_.appliedDelta.y,
                    current.z,
                });
            }
        }

        pendingDisplacement_ = Vector2{0.0f, 0.0f};
        hasPendingMove_ = false;
    }

protected:
    void Initialize() override { TryRegister(); }

    void Update() override { TryRegister(); }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneCollider = sceneContext ? sceneContext->GetComponent<SceneObjectCollider>() : nullptr;
        if (sceneCollider) sceneCollider->UnregisterCharacterController2D(this);
        registered_ = false;
        pendingDisplacement_ = Vector2{0.0f, 0.0f};
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
        const std::string preview = current ? current->GetComponentType() : "(First 2D Collider)";
        if (ImGui::BeginCombo("Collider Shape", preview.c_str())) {
            if (ImGui::Selectable("(First 2D Collider)", !current)) SetSelectedCollider(nullptr);
            for (auto *collider : colliders) {
                ImGui::PushID(collider);
                const bool selected = collider == current;
                if (ImGui::Selectable(collider->GetComponentType().c_str(), selected)) SetSelectedCollider(collider);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        if (current && current->GetShape() != ICollider::Shape::Box2D) {
            ImGui::TextDisabled("Current version supports axis-aligned Box2DCollider only.");
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
    bool HasFlag(CharacterCollisionFlags2D flag) const noexcept {
        return (lastMoveResult_.collisionFlags & static_cast<std::uint8_t>(flag)) != 0;
    }

    void TryRegister() {
        if (registered_) return;
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return;
        auto *sceneCollider = sceneContext->GetComponent<SceneObjectCollider>();
        if (!sceneCollider) sceneCollider = sceneContext->AddComponent<SceneObjectCollider>();
        if (!sceneCollider) return;
        sceneCollider->RegisterCharacterController2D(this);
        registered_ = true;
    }

    float skinWidth_ = 0.01f;
    float groundedThreshold_ = 0.5f;
    std::string selectedColliderTypeName_;
    int selectedColliderOccurrenceIndex_ = 0;
    std::vector<std::string> ignoredTags_;

    Vector2 pendingDisplacement_{0.0f, 0.0f};
    CharacterMoveResult2D lastMoveResult_{};
    bool hasPendingMove_ = false;
    bool registered_ = false;
};

REGISTER_COMPONENT_OBJECT(CharacterController2D)

} // namespace KashipanEngine
