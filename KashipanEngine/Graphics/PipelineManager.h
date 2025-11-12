#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/PipelineInfo.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/ComponentsPresetContainer.h"
#include "Graphics/Pipeline/System/PipelineCreator.h"

namespace KashipanEngine {

class DirectXCommon;

/// @brief パイプライン管理用クラス
class PipelineManager {
public:
    /// @brief コンストラクタ
    /// @param device D3D12 デバイス
    /// @param pipelineSettingsPath パイプライン設定ファイルパス
    PipelineManager(Passkey<DirectXCommon>, ID3D12Device *device, const std::string &pipelineSettingsPath = "Resources/PipelineSetting.json");
    ~PipelineManager() = default;

    /// @brief パイプラインの再読み込み
    void ReloadPipelines();

    /// @brief パイプライン情報の取得
    /// @param pipelineName パイプライン名
    /// @return パイプライン情報構造体への参照
    [[nodiscard]] const PipelineInfo &GetPipeline(const std::string &pipelineName) { return pipelineInfos_.at(pipelineName); }
    /// @brief パイプラインの存在確認
    /// @param pipelineName パイプライン名
    /// @return 存在する場合は true を返す
    [[nodiscard]] bool HasPipeline(const std::string &pipelineName) const { return pipelineInfos_.find(pipelineName) != pipelineInfos_.end(); }

    /// @brief コマンドリストにパイプラインを設定する
    /// @param commandList コマンドリスト
    /// @param pipelineName 
    void SetCommandListPipeline(ID3D12GraphicsCommandList *commandList, const std::string &pipelineName);
    void ResetCurrentPipeline() { currentPipelineName_.clear(); }

private:
    void LoadPreset();
    void LoadPipelines();
    void LoadRenderPipeline(const Json &json);
    void LoadComputePipeline(const Json &json);

    ID3D12Device *device_ = nullptr;
    ID3D12GraphicsCommandList *commandList_ = nullptr;

    std::unique_ptr<ShaderCompiler> shaderCompiler_;
    ComponentsPresetContainer components_{ Passkey<PipelineManager>{} };
    std::unique_ptr<PipelineCreator> pipelineCreator_;

    std::string pipelineSettingsPath_;
    std::string pipelineFolderPath_;
    std::unordered_map<std::string, std::string> presetFolderNames_;

    std::string currentPipelineName_;
    std::unordered_map<std::string, PipelineInfo> pipelineInfos_;
};

} // namespace KashipanEngine