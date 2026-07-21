#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Scene/Components/Compute/SceneComputeProcessor.h"

namespace KashipanEngine {

/// @brief Computeシェーダーをオブジェクトのスクリプトのように扱うためのコンポーネント
/// @details 使用するComputeパイプライン名・ディスパッチするスレッドグループ数・
///          バインドしたいテクスチャ/定数バッファをImGui・JSONで設定するだけで、
///          エンジン専用のコマンドリスト（ComputeCommandProcessor）上で
///          毎フレームComputeシェーダーが実行される。
///          バインド用のGPUリソース（定数バッファ・UAVテクスチャ）はエンジン側（Renderer）が生成・管理する。
class ComputeShaderProcessing final : public IObjectComponent {
public:
    /// @brief 読み取り専用テクスチャのバインド要求（読み込み済みテクスチャから選択する）
    struct TextureBindRequirement {
        std::string variableName;
        std::string textureAssetPath;
    };
    /// @brief 読み書き可能なUAVテクスチャのバインド要求（サイズ指定でエンジンが生成・管理する）
    struct UAVTextureBindRequirement {
        std::string variableName;
        std::uint32_t width = 256;
        std::uint32_t height = 256;
        /// @brief フォーマット種別（0:RGBA8_UNORM 1:R32_FLOAT 2:RGBA32_FLOAT）
        int formatKind = 0;
    };
    /// @brief 定数バッファのバインド要求（float配列をそのまま定数バッファへ送る）
    struct ConstantBufferBindRequirement {
        std::string variableName;
        std::vector<float> values;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(ComputeShaderProcessing, 0xFF,
        SetUpdatePriority(100);
        ADD_MEMBER_VARIABLE(pipelineName_);
        ADD_MEMBER_VARIABLE(groupCountX_);
        ADD_MEMBER_VARIABLE(groupCountY_);
        ADD_MEMBER_VARIABLE(groupCountZ_);
    )
    COMPONENT_CATEGORY("Compute")
    ~ComputeShaderProcessing() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ComputeShaderProcessing>();
        ptr->pipelineName_ = pipelineName_;
        ptr->groupCountX_ = groupCountX_;
        ptr->groupCountY_ = groupCountY_;
        ptr->groupCountZ_ = groupCountZ_;
        ptr->textureBindRequirements_ = textureBindRequirements_;
        ptr->uavTextureBindRequirements_ = uavTextureBindRequirements_;
        ptr->constantBufferBindRequirements_ = constantBufferBindRequirements_;
        return ptr;
    }

    const std::string &GetPipelineName() const noexcept { return pipelineName_; }
    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }

    /// @brief ディスパッチするスレッドグループ数を取得
    void GetGroupCounts(std::uint32_t &x, std::uint32_t &y, std::uint32_t &z) const noexcept {
        x = groupCountX_; y = groupCountY_; z = groupCountZ_;
    }
    /// @brief ディスパッチするスレッドグループ数を設定
    void SetGroupCounts(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
        groupCountX_ = x; groupCountY_ = y; groupCountZ_ = z;
    }

    const std::vector<TextureBindRequirement> &GetTextureBindRequirements() const noexcept { return textureBindRequirements_; }
    const std::vector<UAVTextureBindRequirement> &GetUAVTextureBindRequirements() const noexcept { return uavTextureBindRequirements_; }
    const std::vector<ConstantBufferBindRequirement> &GetConstantBufferBindRequirements() const noexcept { return constantBufferBindRequirements_; }

protected:
    void Initialize() override {
        auto *sceneProcessor = GetOrAddSceneComputeProcessor();
        if (sceneProcessor) sceneProcessor->RegisterComputeShaderProcessing(this);
    }
    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneProcessor = sceneContext ? sceneContext->GetComponent<SceneComputeProcessor>() : nullptr;
        if (sceneProcessor) sceneProcessor->UnregisterComputeShaderProcessing(this);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

private:
    SceneComputeProcessor *GetOrAddSceneComputeProcessor() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneProcessor = sceneContext->GetComponent<SceneComputeProcessor>();
        if (!sceneProcessor) sceneProcessor = sceneContext->AddComponent<SceneComputeProcessor>();
        return sceneProcessor;
    }

    std::string pipelineName_;
    std::uint32_t groupCountX_ = 1;
    std::uint32_t groupCountY_ = 1;
    std::uint32_t groupCountZ_ = 1;

    std::vector<TextureBindRequirement> textureBindRequirements_;
    std::vector<UAVTextureBindRequirement> uavTextureBindRequirements_;
    std::vector<ConstantBufferBindRequirement> constantBufferBindRequirements_;
};

REGISTER_COMPONENT_OBJECT(ComputeShaderProcessing)

} // namespace KashipanEngine
