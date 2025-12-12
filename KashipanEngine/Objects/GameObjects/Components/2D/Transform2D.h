#pragma once
#include "Objects/GameObjects/IGameObjectComponent.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Matrix4x4.h"
#include <memory>

namespace KashipanEngine {

/// @brief 2Dトランスフォームコンポーネント
template<typename T = Matrix4x4>
class Transform2D : public IGameObjectComponent2D {
public:
    Transform2D(const T &transform = T()) : IGameObjectComponent2D("Transform2D", 1), transform_(transform) {
        transformBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(T));
        SetTransform(transform_);
    }
    ~Transform2D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IGameObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Transform2D<T>>(transform_);
        return ptr;
    }

    /// @brief トランスフォームのバインド
    /// @param shaderBinder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!transformBuffer_) return false;
        return shaderBinder && shaderBinder->Bind("Vertex:gTransformationMatrix", transformBuffer_.get());
    }

    /// @brief トランスフォームの設定
    void SetTransform(const T &mat) {
        transform_ = mat;
        if (transformBuffer_) {
            void *mapped = transformBuffer_->Map();
            if (mapped) {
                std::memcpy(mapped, &transform_, sizeof(T));
                transformBuffer_->Unmap();
            }
        }
    }
    /// @brief トランスフォームの取得
    const T &GetTransform() const { return transform_; }

private:
    T transform_{};
    std::unique_ptr<ConstantBufferResource> transformBuffer_;
};

} // namespace KashipanEngine
