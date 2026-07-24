#pragma once
#include <algorithm>
#include "Assets/TextureManager.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief ディゾルブポストエフェクト
class DissolveEffect final : public IPostProcessComponent {
public:
    struct Params {
        float maskThreshold = 0.5f;
        float edgeThickness = 0.1f;
        TextureManager::TextureHandle baseTexture = TextureManager::kInvalidHandle;
        TextureManager::TextureHandle maskTexture = TextureManager::kInvalidHandle;
        float baseTextureColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float edgeColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    DissolveEffect() : IPostProcessComponent("DissolveEffect", GetComponentTypeID<DissolveEffect>()) {
        ADD_MEMBER_VARIABLE(params_.maskThreshold);
        ADD_MEMBER_VARIABLE(params_.edgeThickness);
        AddMemberVariable("params_.baseTextureColor[0]", &params_.baseTextureColor[0]);
        AddMemberVariable("params_.baseTextureColor[1]", &params_.baseTextureColor[1]);
        AddMemberVariable("params_.baseTextureColor[2]", &params_.baseTextureColor[2]);
        AddMemberVariable("params_.baseTextureColor[3]", &params_.baseTextureColor[3]);
        AddMemberVariable("params_.edgeColor[0]", &params_.edgeColor[0]);
        AddMemberVariable("params_.edgeColor[1]", &params_.edgeColor[1]);
        AddMemberVariable("params_.edgeColor[2]", &params_.edgeColor[2]);
        AddMemberVariable("params_.edgeColor[3]", &params_.edgeColor[3]);
    }
    ~DissolveEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<DissolveEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Mask Threshold", &params_.maskThreshold, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Edge Thickness", &params_.edgeThickness, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::ColorEdit4("Base Texture Color", params_.baseTextureColor);
        ImGui::ColorEdit4("Edge Color", params_.edgeColor);
        // テクスチャは読み込み済みのものから選択する
        // ファイル名単体だと同名ファイルが複数フォルダにある場合にImGuiのID重複警告が出るため、
        // Assetsからの相対パスを表示・選択キーとして使う
        std::vector<std::string> textureNames;
        for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
            textureNames.push_back(entry.assetPath);
        }
        std::string baseTextureName = TextureManager::GetTextureAssetPath(params_.baseTexture);
        if (ImGuiCustom::SelectString("Base Texture", baseTextureName, textureNames, true)) {
            params_.baseTexture = baseTextureName.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(baseTextureName);
        }
        std::string maskTextureName = TextureManager::GetTextureAssetPath(params_.maskTexture);
        if (ImGuiCustom::SelectString("Mask Texture", maskTextureName, textureNames, true)) {
            params_.maskTexture = maskTextureName.empty() ? TextureManager::kInvalidHandle : TextureManager::GetTextureFromAssetPath(maskTextureName);
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["maskThreshold"] = params_.maskThreshold;
        json["edgeThickness"] = params_.edgeThickness;
        json["baseTexture"] = TextureManager::GetTextureAssetPath(params_.baseTexture);
        json["maskTexture"] = TextureManager::GetTextureAssetPath(params_.maskTexture);
        json["baseTextureColor"] = { params_.baseTextureColor[0], params_.baseTextureColor[1], params_.baseTextureColor[2], params_.baseTextureColor[3] };
        json["edgeColor"] = { params_.edgeColor[0], params_.edgeColor[1], params_.edgeColor[2], params_.edgeColor[3] };
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.maskThreshold = json.value("maskThreshold", 0.5f);
        params_.edgeThickness = json.value("edgeThickness", 0.1f);
        params_.baseTexture = TextureManager::GetTextureFromAssetPath(json.value("baseTexture", std::string{}));
        params_.maskTexture = TextureManager::GetTextureFromAssetPath(json.value("maskTexture", std::string{}));
        if (json.contains("baseTextureColor") && json["baseTextureColor"].is_array() && json["baseTextureColor"].size() >= 4) {
            for (int i = 0; i < 4; ++i) params_.baseTextureColor[i] = json["baseTextureColor"][i].get<float>();
        }
        if (json.contains("edgeColor") && json["edgeColor"].is_array() && json["edgeColor"].size() >= 4) {
            for (int i = 0; i < 4; ++i) params_.edgeColor[i] = json["edgeColor"][i].get<float>();
        }
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        cbData_.maskThreshold = std::clamp(params_.maskThreshold, 0.0f, 1.0f);
        cbData_.edgeThickness = std::max(0.0f, params_.edgeThickness);
        cbData_.useBaseTexture = (params_.baseTexture != TextureManager::kInvalidHandle) ? 1 : 0;
        cbData_.useMaskTexture = (params_.maskTexture != TextureManager::kInvalidHandle) ? 1 : 0;
        for (int i = 0; i < 4; ++i) {
            cbData_.baseTextureColor[i] = params_.baseTextureColor[i];
            cbData_.edgeColor[i] = params_.edgeColor[i];
        }
        PassInfo pass;
        pass.pipelineName = "PostEffect.Dissolve";
        pass.constantBufferRequirements = {
            { "Pixel:DissolveCB", sizeof(CBData), &cbData_ }
        };
        if (params_.baseTexture != TextureManager::kInvalidHandle) {
            const auto baseTexture = params_.baseTexture;
            pass.textureBindRequirements.push_back({ "Pixel:gBaseTexture",
                [baseTexture]() { return TextureManager::GetTextureView(baseTexture).GetSrvHandle(); } });
        }
        if (params_.maskTexture != TextureManager::kInvalidHandle) {
            const auto maskTexture = params_.maskTexture;
            pass.textureBindRequirements.push_back({ "Pixel:gMaskTexture",
                [maskTexture]() { return TextureManager::GetTextureView(maskTexture).GetSrvHandle(); } });
        }
        return { std::move(pass) };
    }

private:
    struct CBData {
        float maskThreshold = 0.0f;
        float edgeThickness = 0.0f;
        int useBaseTexture = 0;
        int useMaskTexture = 0;
        float baseTextureColor[4]{};
        float edgeColor[4]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(DissolveEffect)

} // namespace KashipanEngine
