#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/ComponentsPresetContainer.h"

namespace KashipanEngine {

struct PipelineSet {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

struct PipelineInfo {
    std::string name;
    std::string type; // "Render" or "Compute"
    D3D12_PRIMITIVE_TOPOLOGY topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    PipelineSet pipelineSet;
};

class PipelineManager {
public:
    PipelineManager() = delete;
    PipelineManager(ID3D12Device *device, const std::string &pipelineSettingsPath = "Resources/PipelineSetting.json");
    ~PipelineManager() = default;

    void ReloadPipelines();

    [[nodiscard]] PipelineInfo &GetPipeline(const std::string &pipelineName) { return pipelineInfos_.at(pipelineName); }
    [[nodiscard]] bool HasPipeline(const std::string &pipelineName) const { return pipelineInfos_.find(pipelineName) != pipelineInfos_.end(); }

    void SetCommandListPipeline(ID3D12GraphicsCommandList *commandList, const std::string &pipelineName);
    void ResetCurrentPipeline() { currentPipelineName_.clear(); }

private:
    void LoadPreset();
    void LoadPipelines();
    void LoadRenderPipeline(const Json &json);
    void LoadComputePipeline(const Json &json);

    ID3D12Device *device_ = nullptr;
    std::unique_ptr<ShaderCompiler> shaderCompiler_;
    ComponentsPresetContainer components_{ Passkey<PipelineManager>{} };

    std::string pipelineSettingsPath_;
    std::string pipelineFolderPath_;
    std::unordered_map<std::string, std::string> presetFolderNames_;

    std::string currentPipelineName_;
    std::unordered_map<std::string, PipelineInfo> pipelineInfos_;
};

} // namespace KashipanEngine