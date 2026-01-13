#pragma once
#include "Objects/IObjectComponent.h"
#include "Math/Vector4.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include "Graphics/IShaderTexture.h"
#include <memory>
#include <cstring>

namespace KashipanEngine {

/// @brief 3Dマテリアルコンポーネント
class Material3D : public IObjectComponent3D {
public:
    struct InstanceData {
        float enableLighting = 1.0f;
        float enableShadowMapProjection = 1.0f;
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Matrix4x4 uvTransform = Matrix4x4::Identity();
        float shininess = 32.0f;
        Vector4 specularColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct UVTransform {
        Vector3 translate{0.0f, 0.0f, 0.0f};
        Vector3 rotate{0.0f, 0.0f, 0.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
    };

    Material3D(const Vector4 &color = Vector4{1.0f, 1.0f, 1.0f, 1.0f},
        TextureManager::TextureHandle texture = TextureManager::kInvalidHandle,
        SamplerManager::SamplerHandle sampler = SamplerManager::kInvalidHandle)
        : IObjectComponent3D("Material3D", 1), textureHandle_(texture), samplerHandle_(sampler) {
        SetColor(color);
    }
    ~Material3D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material3D>(instanceData_.color, textureHandle_, samplerHandle_);
        ptr->instanceData_ = instanceData_;
        return ptr;
    }

    /// @brief マテリアルのバインド
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!shaderBinder) return false;

        bool ok = true;
        if (shaderBinder->GetNameMap().Contains("Pixel:gTexture")) {
            if (texture_ != nullptr) {
                ok = ok && TextureManager::BindTexture(shaderBinder, "Pixel:gTexture", *texture_);
            } else if (textureHandle_ != TextureManager::kInvalidHandle) {
                ok = ok && TextureManager::BindTexture(shaderBinder, "Pixel:gTexture", textureHandle_);
            }
        }
        if (shaderBinder->GetNameMap().Contains("Pixel:gSampler")) {
            if (samplerHandle_ != SamplerManager::kInvalidHandle) {
                ok = ok && SamplerManager::BindSampler(shaderBinder, "Pixel:gSampler", samplerHandle_);
            }
        }

        return ok;
    }

    std::optional<bool> BindInstancingResources(ShaderVariableBinder* binder, std::uint32_t instanceCount) override {
        (void)binder;
        (void)instanceCount;
        return std::nullopt;
    }

    std::optional<bool> SubmitInstance(void *instanceMap, std::uint32_t instanceIndex) override {
        if (!instanceMap) return false;

        auto *arr = static_cast<InstanceData*>(instanceMap);
        std::memcpy(&arr[instanceIndex], &instanceData_, sizeof(InstanceData));
        return true;
    }

    void SetTexture(TextureManager::TextureHandle texture) {
        texture_ = nullptr;
        textureHandle_ = texture;
    }
    void SetTexture(IShaderTexture* texture) {
        texture_ = texture;
        textureHandle_ = TextureManager::kInvalidHandle;
    }

    void SetSampler(SamplerManager::SamplerHandle sampler) { samplerHandle_ = sampler; }
    
    void SetEnableLighting(bool enable) {
        instanceData_.enableLighting = enable ? 1.0f : 0.0f;
        isBufferDirty_ = true;
    }
    void SetEnableShadowMapProjection(bool enable) {
        instanceData_.enableShadowMapProjection = enable ? 1.0f : 0.0f;
        isBufferDirty_ = true;
    }
    void SetColor(const Vector4 &color) {
        instanceData_.color = color;
        isBufferDirty_ = true;
    }
    void SetUVTransform(const UVTransform &uvTransform) {
        uvTransform_ = uvTransform;
        instanceData_.uvTransform.MakeAffine(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
        isBufferDirty_ = true;
    }
    void SetShininess(float shininess) {
        instanceData_.shininess = shininess;
        isBufferDirty_ = true;
    }
    void SetSpecularColor(const Vector4 &specularColor) {
        instanceData_.specularColor = specularColor;
        isBufferDirty_ = true;
    }

    TextureManager::TextureHandle GetTexture() const { return textureHandle_; }
    IShaderTexture* GetTexturePtr() const { return texture_; }
    SamplerManager::SamplerHandle GetSampler() const { return samplerHandle_; }
    const Vector4 &GetColor() const { return instanceData_.color; }
    const UVTransform &GetUVTransform() const { return uvTransform_; }
    Matrix4x4 GetUVTransformMatrix() const {
        return instanceData_.uvTransform;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.material3d").c_str());

        bool lightingEnabled = (instanceData_.enableLighting != 0.0f);
        if (ImGui::Checkbox(Translation("engine.imgui.material.enable_lighting").c_str(), &lightingEnabled)) {
            SetEnableLighting(lightingEnabled);
        }
        bool shadowEnabled = (instanceData_.enableShadowMapProjection != 0.0f);
        if (ImGui::Checkbox(Translation("engine.imgui.material.enable_shadow").c_str(), &shadowEnabled)) {
            SetEnableShadowMapProjection(shadowEnabled);
        }

        Vector4 c = instanceData_.color;
        if (ImGui::ColorEdit4(Translation("engine.imgui.material.color").c_str(), &c.x)) {
            SetColor(c);
        }

        UVTransform uv = uvTransform_;
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.translate").c_str(), &uv.translate.x, 0.01f);
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.rotate").c_str(), &uv.rotate.x, 0.01f, -3.14f, 3.14f);
        ImGui::DragFloat3(Translation("engine.imgui.material.uv.scale").c_str(), &uv.scale.x, 0.01f);
        SetUVTransform(uv);

        if (ImGui::DragFloat(Translation("engine.imgui.material.shininess").c_str(), &instanceData_.shininess, 0.1f, 0.0f, 256.0f)) {
            SetShininess(instanceData_.shininess);
        }
        Vector4 specColor = instanceData_.specularColor;
        if (ImGui::ColorEdit4(Translation("engine.imgui.material.specular_color").c_str(), &specColor.x)) {
            SetSpecularColor(specColor);
        }

        ImGui::TextUnformatted(Translation("engine.imgui.material.texture_handle").c_str());
        ImGui::SameLine();
        ImGui::Text("%u", static_cast<unsigned>(textureHandle_));

        ImGui::TextUnformatted(Translation("engine.imgui.material.sampler_handle").c_str());
        ImGui::SameLine();
        ImGui::Text("%u", static_cast<unsigned>(samplerHandle_));
    }
#endif

private:
    UVTransform uvTransform_;
    InstanceData instanceData_{};

    TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;
    IShaderTexture* texture_ = nullptr;
    SamplerManager::SamplerHandle samplerHandle_ = SamplerManager::kInvalidHandle;

    bool isBufferDirty_ = true;
};

} // namespace KashipanEngine