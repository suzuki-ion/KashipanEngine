#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/PipelineInfo.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"
#include "Graphics/Pipeline/System/PipelineCreator.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

namespace KashipanEngine {

class GraphicsEngine;
class Renderer;

/// @brief パイプライン管理用クラス
class PipelineManager {
public:
    /// @brief コンストラクタ（GraphicsEngine からのみ生成可能）
    /// @param device D3D12 デバイス
    /// @param pipelineSettingsPath パイプライン設定ファイルパス
    PipelineManager(Passkey<GraphicsEngine>, ID3D12Device *device, const std::string &pipelineSettingsPath = "Resources/PipelineSetting.json");
    ~PipelineManager() = default;

    /// @brief パイプラインの再読み込み
    void ReloadPipelines();

    /// @brief パイプライン情報の取得
    const PipelineInfo &GetPipeline(const std::string &pipelineName) { return pipelineInfos_.at(pipelineName); }
    /// @brief パイプラインの存在確認
    bool HasPipeline(const std::string &pipelineName) const { return pipelineInfos_.find(pipelineName) != pipelineInfos_.end(); }

    /// @brief 指定のコマンドリストにパイプラインをセット（差分管理は PipelineBinder 側で行う想定）
    void ApplyPipeline(ID3D12GraphicsCommandList* commandList, const std::string &pipelineName);
    /// @brief シェーダーの変数バインダーを取得（Rendererから呼ばれる想定）
    ShaderVariableBinder &GetShaderVariableBinder(Passkey<Renderer>, const std::string &pipelineName) {
        auto it = pipelineInfos_.find(pipelineName);
        if (it == pipelineInfos_.end()) {
            throw std::runtime_error("PipelineManager::GetShaderVariableBinder: Pipeline not found: " + pipelineName);
        }
        return it->second.GetVariableBinder();
    }

private:
    void LoadPreset();
    void LoadPipelines();

    ID3D12Device *device_ = nullptr;

    std::unique_ptr<ShaderCompiler> shaderCompiler_;
    ComponentsPresetContainer components_{ Passkey<PipelineManager>{} };
    std::unique_ptr<PipelineCreator> pipelineCreator_;

    std::string pipelineSettingsPath_;
    std::string pipelineFolderPath_;
    std::unordered_map<std::string, std::string> presetFolderNames_;

    std::unordered_map<std::string, PipelineInfo> pipelineInfos_;
};

} // namespace KashipanEngine