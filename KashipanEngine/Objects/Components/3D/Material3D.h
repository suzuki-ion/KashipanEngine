#pragma once
#include "Objects/IObjectComponent.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector4.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include <memory>

namespace KashipanEngine {

/// @brief 3Dマテリアルコンポーネント
class Material3D : public IObjectComponent3D {
public:
    struct Data {
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    static const std::string &GetStaticComponentType() {
        static const std::string type = "Material3D";
        return type;
    }

    Material3D(const Data &data = Data{},
        TextureManager::TextureHandle texture = TextureManager::kInvalidHandle,
        SamplerManager::SamplerHandle sampler = SamplerManager::kInvalidHandle)
        : IObjectComponent3D("Material3D", 1), textureHandle_(texture), samplerHandle_(sampler) {
        materialBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(Data));
        SetData(data);
    }
    ~Material3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material3D>(data_, textureHandle_, samplerHandle_);
        return ptr;
    }

    /// @brief マテリアルのバインド
    /// @param shaderBinder シェーダー変数バインダー
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!materialBuffer_) return false;
        if (!shaderBinder) return false;

        bool ok = shaderBinder->Bind("Pixel:gMaterial", materialBuffer_.get());
        if (textureHandle_ != TextureManager::kInvalidHandle) {
            ok = ok && TextureManager::BindTexture(shaderBinder, "Pixel:gTexture", textureHandle_);
        }
        if (samplerHandle_ != SamplerManager::kInvalidHandle) {
            ok = ok && SamplerManager::BindSampler(shaderBinder, "Pixel:gSampler", samplerHandle_);
        }
        return ok;
    }

    void SetTexture(TextureManager::TextureHandle texture) { textureHandle_ = texture; }
    TextureManager::TextureHandle GetTexture() const { return textureHandle_; }

    void SetSampler(SamplerManager::SamplerHandle sampler) { samplerHandle_ = sampler; }
    SamplerManager::SamplerHandle GetSampler() const { return samplerHandle_; }

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
    TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;
    SamplerManager::SamplerHandle samplerHandle_ = SamplerManager::kInvalidHandle;
    std::unique_ptr<ConstantBufferResource> materialBuffer_;
};

} // namespace KashipanEngine