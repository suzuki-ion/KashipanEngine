#pragma once
#include "Objects/IObjectComponent.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include <memory>
#include <optional>
#include <cstring>
#include <cstdint>

namespace KashipanEngine {

/// @brief 2Dトランスフォームコンポーネント
class Transform2D : public IObjectComponent2D {
public:
    struct InstanceData {
        Matrix4x4 world;
    };

    Transform2D() : IObjectComponent2D("Transform2D", 1) {
        isWorldMatrixCalculated_ = false;
        worldMatrix_ = Matrix4x4::Identity();
        worldMatrixVersion_ = 0;
        cachedParentVersion_ = 0;
    }
    ~Transform2D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Transform2D>();
        ptr->translate_ = translate_;
        ptr->rotate_ = rotate_;
        ptr->scale_ = scale_;
        ptr->isWorldMatrixCalculated_ = false;
        ptr->worldMatrix_ = Matrix4x4::Identity();
        ptr->worldMatrixVersion_ = 0;
        ptr->cachedParentVersion_ = 0;
        return ptr;
    }

    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        (void)shaderBinder;
        return std::nullopt;
    }

    std::optional<bool> BindInstancingResources(ShaderVariableBinder *binder, std::uint32_t instanceCount) override {
        (void)binder;
        (void)instanceCount;
        return std::nullopt;
    }

    std::optional<bool> SubmitInstance(void *instanceMap, std::uint32_t instanceIndex) override {
        if (!instanceMap) return false;
        struct InstanceTransformLocal { Matrix4x4 world; };
        auto *arr = static_cast<InstanceTransformLocal *>(instanceMap);
        arr[instanceIndex].world = GetWorldMatrix();
        return true;
    }

    /// @brief 親トランスフォームの設定
    /// @param parent 親トランスフォームポインタ
    /// @return 設定に成功した場合はtrue、失敗した場合はfalseを返す
    bool SetParentTransform(Transform2D *parent) {
        if (parent == this) {
            return false;
        }
        for (auto *p = parent; p != nullptr; p = p->parentTransform_) {
            if (p == this) return false;
        }
        parentTransform_ = parent;
        isWorldMatrixCalculated_ = false;
        cachedParentVersion_ = 0;
        return true;
    }

    /// @brief 親トランスフォームの取得
    /// @return 親トランスフォームポインタ
    Transform2D *GetParentTransform() const { return parentTransform_; }

    /// @brief 平行移動の設定
    /// @param translate 平行移動ベクトル
    void SetTranslate(const Vector3 &translate) {
        if (translate_ == translate) return;
        translate_ = translate;
        isWorldMatrixCalculated_ = false;
    }

    /// @brief 回転の設定
    /// @param rotate 回転ベクトル
    void SetRotate(const Vector3 &rotate) {
        if (rotate_ == rotate) return;
        rotate_ = rotate;
        isWorldMatrixCalculated_ = false;
    }

    /// @brief スケーリングの設定
    /// @param scale スケーリングベクトル
    void SetScale(const Vector3 &scale) {
        if (scale_ == scale) return;
        scale_ = scale;
        isWorldMatrixCalculated_ = false;
    }

    /// @brief 現在の平行移動ベクトルを取得
    /// @return 平行移動ベクトル
    const Vector3 &GetTranslate() const { return translate_; }
    /// @brief 現在の回転ベクトルを取得
    /// @return 回転ベクトル
    const Vector3 &GetRotate() const { return rotate_; }
    /// @brief 現在のスケーリングベクトルを取得
    /// @return スケーリングベクトル
    const Vector3 &GetScale() const { return scale_; }

    const Matrix4x4 &GetWorldMatrix() {
        if (IsWorldMatrixDirty()) {
            Matrix4x4 local = Matrix4x4::Identity();
            local.MakeAffine(scale_, rotate_, translate_);

            if (parentTransform_) {
                const Matrix4x4 &pw = parentTransform_->GetWorldMatrix();
                worldMatrix_ = local * pw;
                cachedParentVersion_ = parentTransform_->GetWorldMatrixVersion();
            } else {
                worldMatrix_ = local;
                cachedParentVersion_ = 0;
            }
            isWorldMatrixCalculated_ = true;
            ++worldMatrixVersion_;
        }
        return worldMatrix_;
    }

    std::uint64_t GetWorldMatrixVersion() const { return worldMatrixVersion_; }

    bool IsWorldMatrixCalculated() const {
        if (!isWorldMatrixCalculated_) return false;
        if (!parentTransform_) return true;
        if (!parentTransform_->IsWorldMatrixCalculated()) return false;
        return (cachedParentVersion_ == parentTransform_->GetWorldMatrixVersion());
    }
    bool IsWorldMatrixDirty() const { return !IsWorldMatrixCalculated(); }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.transform2d").c_str());

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
    Vector3 translate_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotate_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    Transform2D *parentTransform_ = nullptr;
    Matrix4x4 worldMatrix_ = Matrix4x4::Identity();
    bool isWorldMatrixCalculated_ = false;

    std::uint64_t worldMatrixVersion_ = 0;
    std::uint64_t cachedParentVersion_ = 0;
};

} // namespace KashipanEngine