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

/// @brief 2Dマテリアルコンポーネント
class Material2D : public IObjectComponent2D {
public:
    struct UVTransform {
        Vector3 translate{ 0.0f, 0.0f, 0.0f };
        Vector3 rotate{ 0.0f, 0.0f, 0.0f };
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
    };

    static const std::string &GetStaticComponentType() {
        static const std::string type = "Material2D";
        return type;
    }

    Material2D(const Vector4 &color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
        TextureManager::TextureHandle texture = TextureManager::kInvalidHandle,
        SamplerManager::SamplerHandle sampler = SamplerManager::kInvalidHandle)
        : IObjectComponent2D("Material2D", 1), textureHandle_(texture), samplerHandle_(sampler) {
        SetColor(color);
    }
    ~Material2D() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Material2D>(color_, textureHandle_, samplerHandle_);
        ptr->uvTransform_ = uvTransform_;
        return ptr;
    }

    /// @brief マテリアルのバインド
    std::optional<bool> BindShaderVariables(ShaderVariableBinder *shaderBinder) override {
        if (!shaderBinder) return false;

        bool ok = true;
        if (texture_ != nullptr) {
            ok = ok && TextureManager::BindTexture(shaderBinder, "Pixel:gTexture", *texture_);
        } else if (textureHandle_ != TextureManager::kInvalidHandle) {
            ok = ok && TextureManager::BindTexture(shaderBinder, "Pixel:gTexture", textureHandle_);
        }

        if (samplerHandle_ != SamplerManager::kInvalidHandle) {
            ok = ok && SamplerManager::BindSampler(shaderBinder, "Pixel:gSampler", samplerHandle_);
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

        struct InstanceMaterialLocal {
            Vector4 color;
            Matrix4x4 uvTransform;
        };

        auto *arr = static_cast<InstanceMaterialLocal *>(instanceMap);
        arr[instanceIndex].color = color_;
        arr[instanceIndex].uvTransform = GetUVTransformMatrix();
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

    void SetColor(const Vector4 &color) {
        color_ = color;
        isBufferDirty_ = true;
    }

    void SetUVTransform(const UVTransform &uvTransform) {
        uvTransform_ = uvTransform;
        isBufferDirty_ = true;
    }

    TextureManager::TextureHandle GetTexture() const { return textureHandle_; }
    IShaderTexture* GetTexturePtr() const { return texture_; }
    SamplerManager::SamplerHandle GetSampler() const { return samplerHandle_; }
    const Vector4 &GetColor() const { return color_; }
    const UVTransform &GetUVTransform() const { return uvTransform_; }

    Matrix4x4 GetUVTransformMatrix() const {
        Matrix4x4 m = Matrix4x4::Identity();
        m.MakeAffine(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
        return m;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.material2d").c_str());

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
    struct InstanceData {
        Vector4 color;
        Matrix4x4 uvTransform;
    };

    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    UVTransform uvTransform_{};

    TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;
    IShaderTexture* texture_ = nullptr;
    SamplerManager::SamplerHandle samplerHandle_ = SamplerManager::kInvalidHandle;

    bool isBufferDirty_ = true;
};

} // namespace KashipanEngine