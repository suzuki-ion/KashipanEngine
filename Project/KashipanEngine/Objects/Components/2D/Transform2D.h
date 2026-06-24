#pragma once
#include "Objects/ObjectComponentHeader.h"
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
        auto *arr = static_cast<InstanceData *>(instanceMap);
        arr[instanceIndex].world = GetWorldMatrix();
        return true;
    }

    /// @brief 親オブジェクトの設定
    /// @param parent 親オブジェクトポインタ
    /// @return 設定に成功した場合はtrue、失敗した場合はfalseを返す
    bool SetParentObject(Object2DBase *parent) {
        if (!parent) {
            parentObject_ = nullptr;
            isWorldMatrixCalculated_ = false;
            cachedParentVersion_ = 0;
            return true;
        }
        auto *ctx = GetOwner2DContext();
        auto *ownerObject = ctx ? ctx->GetOwner() : nullptr;
        if (parent == ownerObject) return false;
        for (auto *p = parent; p != nullptr; p = p->GetComponent2D<Transform2D>()->parentObject_) {
            if (p == ownerObject) return false;
        }
        parentObject_ = parent;
        // 親が変わったのでキャッシュは無効
        isWorldMatrixCalculated_ = false;
        cachedParentVersion_ = 0;
        return true;
    }

    /// @brief 親オブジェクトの取得
    /// @return 親オブジェクトポインタ（親がいない場合はnullptr）
    Object2DBase *GetParentObject() const { return parentObject_; }

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

            auto *parentTransform = parentObject_ ? parentObject_->GetComponent2D<Transform2D>() : nullptr;
            if (parentTransform) {
                const Matrix4x4 &pw = parentTransform->GetWorldMatrix();
                worldMatrix_ = local * pw;
                cachedParentVersion_ = parentTransform->GetWorldMatrixVersion();
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
        auto *parentTransform = parentObject_ ? parentObject_->GetComponent2D<Transform2D>() : nullptr;
        if (!parentTransform) return true;
        if (!parentTransform->IsWorldMatrixCalculated()) return false;
        return (cachedParentVersion_ == parentTransform->GetWorldMatrixVersion());
    }
    bool IsWorldMatrixDirty() const { return !IsWorldMatrixCalculated(); }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.transform2d").c_str());

        Vector3 t = translate_;
        Vector3 r = rotate_;
        Vector3 s = scale_;
        Vector3 rDeg{r.x * 180.0f / 3.14159265f, r.y * 180.0f / 3.14159265f, r.z * 180.0f / 3.14159265f};

        ImGui::DragFloat3(Translation("engine.imgui.transform.translate").c_str(), &t.x, 0.05f);
        if (ImGui::DragFloat3(Translation("engine.imgui.transform.rotate").c_str(), &rDeg.x, 0.02f, -180.0f, 180.0f)) {
            r.x = rDeg.x * 3.14159265f / 180.0f;
            r.y = rDeg.y * 3.14159265f / 180.0f;
            r.z = rDeg.z * 3.14159265f / 180.0f;
        }
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

    Object2DBase *parentObject_ = nullptr;
    Matrix4x4 worldMatrix_ = Matrix4x4::Identity();
    bool isWorldMatrixCalculated_ = false;

    std::uint64_t worldMatrixVersion_ = 0;
    std::uint64_t cachedParentVersion_ = 0;
};

REGISTER_COMPONENT_OBJECT2D(Transform2D)

} // namespace KashipanEngine