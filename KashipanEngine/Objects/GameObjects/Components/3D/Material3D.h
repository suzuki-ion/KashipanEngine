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
    Material3D() : IGameObjectComponent3D("Material3D", 1) {}
    ~Material3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IGameObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material3D>();
        ptr->material_ = material_; // 状態を手動コピー（バッファは複製しない）
        return ptr;
    }

    void Initialize(IGameObjectContext &context) override {
        materialBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(T));
        SetMaterial(material_);
    }
    void Finalize(IGameObjectContext &context) override {
        materialBuffer_.reset();
    }

    bool Bind(ShaderVariableBinder &shaderBinder, const std::string &variableName) {
        if (materialBuffer_) {
            return shaderBinder.Bind(variableName, materialBuffer_.get());
        }
        return false;
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