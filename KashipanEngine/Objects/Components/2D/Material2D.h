#pragma once
#include "Objects/IObjectComponent.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector4.h"
#include <memory>

namespace KashipanEngine {

/// @brief 2Dマテリアルコンポーネント
class Material2D : public IObjectComponent2D {
public:
    struct Data {
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    static const std::string &GetStaticComponentType() {
        static const std::string type = "Material2D";
        return type;
    }

    Material2D(const Data &data = Data{}) : IObjectComponent2D("Material2D", 1) {
        materialBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(Data));
        SetData(data);
    }
    ~Material2D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material2D>(data_);
        return ptr;
    }

    /// @brief マテリアルのバインド
    /// @param shaderBinder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!materialBuffer_) return false;
        return shaderBinder && shaderBinder->Bind("Pixel:gMaterial", materialBuffer_.get());
    }

    /// @brief カラーの設定
    void SetColor(const Vector4 &color) {
        data_.color = color;
        if (materialBuffer_) {
            void *mappedData = materialBuffer_->Map();
            if (mappedData) {
                std::memcpy(mappedData, &data_, sizeof(Data));
                materialBuffer_->Unmap();
            }
        }
    }
    /// @brief カラーの取得
    const Vector4 &GetColor() const { return data_.color; }

    /// @brief データの設定
    void SetData(const Data &d) {
        data_ = d;
        if (materialBuffer_) {
            void *mappedData = materialBuffer_->Map();
            if (mappedData) {
                std::memcpy(mappedData, &data_, sizeof(Data));
                materialBuffer_->Unmap();
            }
        }
    }
    /// @brief データの取得
    const Data &GetData() const { return data_; }

private:
    Data data_{};
    std::unique_ptr<ConstantBufferResource> materialBuffer_;
};

} // namespace KashipanEngine