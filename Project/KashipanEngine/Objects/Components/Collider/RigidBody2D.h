#pragma once
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Math/Vector2.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class RigidBody2D final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(RigidBody2D, 1,
        ADD_MEMBER_VARIABLE(velocity_);
        ADD_MEMBER_VARIABLE(mass_);
        ADD_MEMBER_VARIABLE(useGravity_);
    )
    COMPONENT_CATEGORY("Collision")
    ~RigidBody2D() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RigidBody2D>();
        ptr->velocity_ = velocity_;
        ptr->mass_ = mass_;
        ptr->useGravity_ = useGravity_;
        ptr->selectedColliderTypeName_ = selectedColliderTypeName_;
        ptr->selectedColliderOccurrenceIndex_ = selectedColliderOccurrenceIndex_;
        return ptr;
    }

    //==================================================
    // 物理パラメータ
    //==================================================

    void SetVelocity(const Vector2 &velocity) { velocity_ = velocity; }
    const Vector2 &GetVelocity() const noexcept { return velocity_; }
    void SetMass(float mass) { mass_ = mass; }
    float GetMass() const noexcept { return mass_; }
    void SetUseGravity(bool enabled) { useGravity_ = enabled; }
    bool IsGravityEnabled() const noexcept { return useGravity_; }

    //==================================================
    // 使用するColliderコンポーネントの選択
    //==================================================

    /// @brief 同一オブジェクト上のICollider派生コンポーネントから使用する形状を選択する
    /// @param collider 選択するコライダー（nullptrの場合は未選択＝どのコライダーでも使用可）
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
    /// @brief 選択中のコライダーを取得（未選択・見つからない場合は nullptr）
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
    /// @brief 同一オブジェクト上の全ICollider派生コンポーネントを取得
    std::vector<ICollider *> GetOwnerColliders() const {
        std::vector<ICollider *> result;
        auto *ctx = GetOwnerObjectContext();
        if (!ctx) return result;
        for (const auto &pair : ctx->GetAllComponents()) {
            if (auto *collider = dynamic_cast<ICollider *>(pair.first.get())) {
                result.push_back(collider);
            }
        }
        return result;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat2(TranslationLabel("component.rigidbody2d.velocity"), &velocity_.x, 0.01f);
        ImGui::DragFloat(TranslationLabel("component.rigidbody2d.mass"), &mass_, 0.01f, 0.0f);
        ImGui::Checkbox(TranslationLabel("component.rigidbody2d.usegravity"), &useGravity_);

        // 使用する形状（Colliderコンポーネント）の選択
        const auto colliders = GetOwnerColliders();
        auto *current = GetSelectedCollider();
        const std::string preview = current ? current->GetComponentType() : "(Any)";
        if (ImGui::BeginCombo(TranslationLabel("component.rigidbody2d.collider_shape"), preview.c_str())) {
            if (ImGui::Selectable(TranslationLabel("component.rigidbody2d.any"), !current)) {
                SetSelectedCollider(nullptr);
            }
            for (auto *collider : colliders) {
                if (!collider->Is2D()) continue;
                ImGui::PushID(collider);
                const bool selected = (collider == current);
                if (ImGui::Selectable(collider->GetComponentType().c_str(), selected)) {
                    SetSelectedCollider(collider);
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
#endif
    JSON SaveToJson() const override {
        JSON json{ {"velocity", ToJSON(velocity_)}, {"mass", mass_}, {"useGravity", useGravity_} };
        json["selectedColliderTypeName"] = selectedColliderTypeName_;
        json["selectedColliderOccurrenceIndex"] = selectedColliderOccurrenceIndex_;
        return json;
    }
    bool LoadFromJson(const JSON &json) override {
        if (json.contains("velocity")) velocity_ = FromJSON<Vector2>(json["velocity"]);
        mass_ = json.value("mass", 1.0f);
        useGravity_ = json.value("useGravity", true);
        selectedColliderTypeName_ = json.value("selectedColliderTypeName", std::string{});
        selectedColliderOccurrenceIndex_ = json.value("selectedColliderOccurrenceIndex", 0);
        return true;
    }
private:
    Vector2 velocity_{ 0.0f, 0.0f };
    float mass_ = 1.0f;
    bool useGravity_ = true;

    /// @brief 使用するコライダーのコンポーネント型名（空の場合は未選択＝どのコライダーでも使用可）
    std::string selectedColliderTypeName_;
    /// @brief 同一型のコライダーが複数ある場合の何番目かを示すインデックス
    int selectedColliderOccurrenceIndex_ = 0;
};

REGISTER_COMPONENT_OBJECT(RigidBody2D)

} // namespace KashipanEngine
