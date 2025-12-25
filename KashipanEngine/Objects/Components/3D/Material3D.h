#pragma once
#include "Objects/IObjectComponent.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include <memory>
#include <cstring>

namespace KashipanEngine {

/// @brief 3Dマテリアルコンポーネント
class Material3D : public IObjectComponent3D {
public:
    struct UVTransform {
        Vector3 translate{0.0f, 0.0f, 0.0f};
        Vector3 rotate{0.0f, 0.0f, 0.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    static const std::string &GetStaticComponentType() {
        static const std::string type = "Material3D";
        return type;
    }

    Material3D(const Vector4 &color = Vector4{1.0f, 1.0f, 1.0f, 1.0f},
        TextureManager::TextureHandle texture = TextureManager::kInvalidHandle,
        SamplerManager::SamplerHandle sampler = SamplerManager::kInvalidHandle)
        : IObjectComponent3D("Material3D", 1), textureHandle_(texture), samplerHandle_(sampler) {
        materialBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(MaterialBuffer));
        SetColor(color);
        UpdateMaterialBuffer();
    }
    ~Material3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material3D>(color_, textureHandle_, samplerHandle_);
        ptr->uvTransform_ = uvTransform_;
        ptr->UpdateMaterialBuffer();
        return ptr;
    }

    /// @brief マテリアルのバインド
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!materialBuffer_) return false;
        if (!shaderBinder) return false;

        UpdateMaterialBuffer();

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
    void SetSampler(SamplerManager::SamplerHandle sampler) { samplerHandle_ = sampler; }
    void SetColor(const Vector4 &color) {
        color_ = color;
        isBufferDirty_ = true;
    }
    void SetUVTransform(const UVTransform &uvTransform) {
        uvTransform_ = uvTransform;
        isBufferDirty_ = true;
    }
    TextureManager::TextureHandle GetTexture() const { return textureHandle_; }
    SamplerManager::SamplerHandle GetSampler() const { return samplerHandle_; }
    const Vector4 &GetColor() const { return color_; }
    const UVTransform &GetUVTransform() const { return uvTransform_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.material3d").c_str());

        Vector4 c = color_;
        if (ImGui::ColorEdit4(Translation("engine.imgui.material.color").c_str(), &c.x)) {
            SetColor(c);
        }

        UVTransform uv = uvTransform_;
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.translate").c_str(), &uv.translate.x, 0.01f);
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.rotate").c_str(), &uv.rotate.x, 0.01f, -3.14f, 3.14f);
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.scale").c_str(), &uv.scale.x, 0.01f);
        SetUVTransform(uv);

        ImGui::TextUnformatted(Translation("engine.imgui.material.texture_handle").c_str());
        ImGui::SameLine();
        ImGui::Text("%u", static_cast<unsigned>(textureHandle_));

        ImGui::TextUnformatted(Translation("engine.imgui.material.sampler_handle").c_str());
        ImGui::SameLine();
        ImGui::Text("%u", static_cast<unsigned>(samplerHandle_));
    }
#endif

private:
    struct MaterialBuffer {
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Matrix4x4 uvTransform = Matrix4x4::Identity();
    };

    void UpdateMaterialBuffer() {
        if (!materialBuffer_ || !isBufferDirty_) return;

        MaterialBuffer buf{};
        buf.color = color_;
        buf.uvTransform = Matrix4x4::Identity();
        buf.uvTransform.MakeAffine(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);

        void *mappedData = materialBuffer_->Map();
        if (mappedData) {
            std::memcpy(mappedData, &buf, sizeof(MaterialBuffer));
            materialBuffer_->Unmap();
            isBufferDirty_ = false;
        }
    }

private:
    Vector4 color_{1.0f, 1.0f, 1.0f, 1.0f};
    UVTransform uvTransform_{};

    TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;
    SamplerManager::SamplerHandle samplerHandle_ = SamplerManager::kInvalidHandle;

    bool isBufferDirty_ = true;
    std::unique_ptr<ConstantBufferResource> materialBuffer_;
};

} // namespace KashipanEngine