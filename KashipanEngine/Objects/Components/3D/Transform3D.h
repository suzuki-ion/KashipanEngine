#pragma once
#include "Objects/IObjectComponent.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include <memory>
#include <optional>
#include <cstring>

namespace KashipanEngine {

/// @brief 3Dトランスフォームコンポーネント
class Transform3D : public IObjectComponent3D {
public:
    static const std::string &GetStaticComponentType() {
        static const std::string type = "Transform3D";
        return type;
    }

    Transform3D() : IObjectComponent3D("Transform3D", 1) {
        // Instancing uses per-draw StructuredBuffer; keep no per-object CBV.
        isWorldMatrixCalculated_ = false;
        worldMatrix_ = Matrix4x4::Identity();
    }
    ~Transform3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Transform3D>();
        ptr->translate_ = translate_;
        ptr->rotate_ = rotate_;
        ptr->scale_ = scale_;
        ptr->isWorldMatrixCalculated_ = false;
        ptr->worldMatrix_ = Matrix4x4::Identity();
        return ptr;
    }

    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        (void)shaderBinder;
        return std::nullopt;
    }

    std::optional<bool> BindInstancingResources(ShaderVariableBinder* binder, std::uint32_t instanceCount) override {
        (void)binder;
        (void)instanceCount;
        return std::nullopt;
    }

    std::optional<bool> SubmitInstance(void *instanceMap, std::uint32_t instanceIndex) override {
        if (!instanceMap) return false;
        struct InstanceTransformLocal { Matrix4x4 world; };
        auto *arr = static_cast<InstanceTransformLocal*>(instanceMap);
        arr[instanceIndex].world = GetWorldMatrix();
        return true;
    }

    /// @brief 親トランスフォームの設定
    /// @param parent 親トランスフォーム
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool SetParentTransform(Transform3D *parent) {
        if (parent == this) {
            return false;
        }
        for (auto *p = parent; p != nullptr; p = p->parentTransform_) {
            if (p == this) return false;
        }
        parentTransform_ = parent;
        isWorldMatrixCalculated_ = false;
        return true;
    }

    Transform3D *GetParentTransform() const { return parentTransform_; }

    void SetTranslate(const Vector3 &translate) {
        if (translate_ == translate) return;
        translate_ = translate;
        isWorldMatrixCalculated_ = false;
    }

    void SetRotate(const Vector3 &rotate) {
        if (rotate_ == rotate) return;
        rotate_ = rotate;
        isWorldMatrixCalculated_ = false;
    }

    void SetScale(const Vector3 &scale) {
        if (scale_ == scale) return;
        scale_ = scale;
        isWorldMatrixCalculated_ = false;
    }

    const Vector3 &GetTranslate() const { return translate_; }
    const Vector3 &GetRotate() const { return rotate_; }
    const Vector3 &GetScale() const { return scale_; }

    const Matrix4x4 &GetWorldMatrix() {
        if (!isWorldMatrixCalculated_) {
            Matrix4x4 local = Matrix4x4::Identity();
            local.MakeAffine(scale_, rotate_, translate_);

            if (parentTransform_) {
                worldMatrix_ = local * parentTransform_->GetWorldMatrix();
            } else {
                worldMatrix_ = local;
            }
            isWorldMatrixCalculated_ = true;
        }
        return worldMatrix_;
    }
    bool IsWorldMatrixCalculated() const { return isWorldMatrixCalculated_; }
    bool IsWorldMatrixDirty() const { return !isWorldMatrixCalculated_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.transform3d").c_str());

        Vector3 t = translate_;
        Vector3 r = rotate_;
        Vector3 s = scale_;

        ImGui::DragFloat3(Translation("engine.imgui.transform.translate").c_str(), &t.x, 0.05f);
        ImGui::DragFloat3(Translation("engine.imgui.transform.rotate").c_str(), &r.x, 0.02f, -3.14f, 3.14f);
        ImGui::DragFloat3(Translation("engine.imgui.transform.scale").c_str(), &s.x, 0.05f);

        SetTranslate(t);
        SetRotate(r);
        SetScale(s);
    }
#endif

private:
    Vector3 translate_{0.0f, 0.0f, 0.0f};
    Vector3 rotate_{0.0f, 0.0f, 0.0f};
    Vector3 scale_{1.0f, 1.0f, 1.0f};

    Transform3D *parentTransform_ = nullptr;
    Matrix4x4 worldMatrix_ = Matrix4x4::Identity();
    bool isWorldMatrixCalculated_ = false;
};

} // namespace KashipanEngine
