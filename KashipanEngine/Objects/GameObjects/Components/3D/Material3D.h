#pragma once
#include "Objects/GameObjects/IGameObjectComponent.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector4.h"
#include <memory>

namespace KashipanEngine {

/// @brief 3Dマテリアルコンポーネントクラス
template<typename T = Vector4>
class Material3D : public IGameObjectComponent3D {
public:
    Material3D(const T &material = T()) : IGameObjectComponent3D("Material3D", 1), material_(material) {
        materialBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(T));
        SetMaterial(material_);
    }
    ~Material3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IGameObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material3D>();
        ptr->material_ = material_; // 状態を手動コピー（バッファは複製しない）
        return ptr;
    }

    /// @brief マテリアルのバインド
    /// @param shaderBinder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!materialBuffer_) return false;
        return shaderBinder && shaderBinder->Bind("Pixel:gMaterial", materialBuffer_.get());
    }

    /// @brief マテリアルの設定
    void SetMaterial(const T &material) {
        material_ = material;
        if (materialBuffer_) {
            void *mappedData = materialBuffer_->Map();
            if (mappedData) {
                std::memcpy(mappedData, &material_, sizeof(T));
                materialBuffer_->Unmap();
            }
        }
    }
    /// @brief マテリアルの取得
    T GetMaterial() const {
        return material_;
    }

private:
    T material_{};
    std::unique_ptr<ConstantBufferResource> materialBuffer_;
};

} // namespace KashipanEngine