#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class DissolveEffect final : public IPostEffectComponent {
public:
    struct Params {
        float maskThreshold = 0.5f;
        float edgeThickness = 0.1f;
        TextureManager::TextureHandle baseTexture = TextureManager::kInvalidHandle;
        TextureManager::TextureHandle maskTexture = TextureManager::kInvalidHandle;
        float baseTextureColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float edgeColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };
    explicit DissolveEffect(Params p = {})
        : IPostEffectComponent("DissolveEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<DissolveEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Mask Threshold", &params_.maskThreshold, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Edge Thickness", &params_.edgeThickness, 0.01f, 0.0f, 1.0f, "%.3f");
        const auto &textureEntries = TextureManager::GetLoadedTextureListEntries();
        // テクスチャのファイル名を選択肢として表示
        static std::vector<const char *> textureNames;
        static std::vector<TextureManager::TextureHandle> textureHandles;
        textureNames.clear();
        textureHandles.clear();
        textureNames.push_back("(None)");
        textureHandles.push_back(TextureManager::kInvalidHandle);
        for (const auto &entry : textureEntries) {
            textureNames.push_back(entry.fileName.c_str());
            textureHandles.push_back(entry.handle);
        }
        static int selectedBaseTextureIndex = -1;
        if (ImGui::Combo("Base Texture", &selectedBaseTextureIndex, textureNames.data(), static_cast<int>(textureNames.size()))) {
            if (selectedBaseTextureIndex >= 0 && static_cast<size_t>(selectedBaseTextureIndex) < textureHandles.size()) {
                params_.baseTexture = textureHandles[selectedBaseTextureIndex];
            } else {
                params_.baseTexture = TextureManager::kInvalidHandle;
            }
        }
        static int selectedMaskTextureIndex = -1;
        if (ImGui::Combo("Mask Texture", &selectedMaskTextureIndex, textureNames.data(), static_cast<int>(textureNames.size()))) {
            if (selectedMaskTextureIndex >= 0 && static_cast<size_t>(selectedMaskTextureIndex) < textureHandles.size()) {
                params_.maskTexture = textureHandles[selectedMaskTextureIndex];
            } else {
                params_.maskTexture = TextureManager::kInvalidHandle;
            }
        }
        ImGui::ColorEdit4("Base Texture Color", params_.baseTextureColor);
        ImGui::ColorEdit4("Edge Color", params_.edgeColor);
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.Dissolve";
        pass.passName = "Dissolve";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:DissolveCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            auto *owner = GetOwnerBuffer();
            if (!owner) return false;

            cb->maskThreshold = std::clamp(params_.maskThreshold, 0.0f, 1.0f);
            cb->edgeThickness = std::clamp(params_.edgeThickness, 0.0f, 1.0f);
            cb->useBaseTexture = params_.baseTexture != TextureManager::kInvalidHandle;
            cb->useMaskTexture = params_.maskTexture != TextureManager::kInvalidHandle;
            cb->baseTextureColor[0] = std::clamp(params_.baseTextureColor[0], 0.0f, 1.0f);
            cb->baseTextureColor[1] = std::clamp(params_.baseTextureColor[1], 0.0f, 1.0f);
            cb->baseTextureColor[2] = std::clamp(params_.baseTextureColor[2], 0.0f, 1.0f);
            cb->baseTextureColor[3] = std::clamp(params_.baseTextureColor[3], 0.0f, 1.0f);
            cb->edgeColor[0] = std::clamp(params_.edgeColor[0], 0.0f, 1.0f);
            cb->edgeColor[1] = std::clamp(params_.edgeColor[1], 0.0f, 1.0f);
            cb->edgeColor[2] = std::clamp(params_.edgeColor[2], 0.0f, 1.0f);
            cb->edgeColor[3] = std::clamp(params_.edgeColor[3], 0.0f, 1.0f);
            return true;
            };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            if (!binder.Bind("Pixel:gTexture", owner->GetSrvHandle())) return false;
            if (params_.baseTexture != TextureManager::kInvalidHandle) {
                if (!TextureManager::BindTexture(&binder, "Pixel:gBaseTexture", params_.baseTexture)) return false;
            }
            if (params_.maskTexture != TextureManager::kInvalidHandle) {
                if (!TextureManager::BindTexture(&binder, "Pixel:gMaskTexture", params_.maskTexture)) return false;
            }
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
            return true;
            };

        pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
            };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float maskThreshold;
        float edgeThickness;
        int useBaseTexture;
        int useMaskTexture;
        float baseTextureColor[4];
        float edgeColor[4];
    };

    Params params_{};
};

} // namespace KashipanEngine