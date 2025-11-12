#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace KashipanEngine {

class PipelineCreator;

/// @brief パイプラインタイプ列挙型
enum class PipelineType {
    Render,
    Compute
};

/// @brief パイプラインセット構造体
struct PipelineSet {
    ID3D12RootSignature *RootSignature() const { return rootSignature.Get(); }
    ID3D12PipelineState *PipelineState() const { return pipelineState.Get(); }
private:
    friend class PipelineCreator;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

/// @brief パイプライン情報構造体
struct PipelineInfo {
    const std::string &Name() const { return name; }
    PipelineType Type() const { return type; }
    D3D12_PRIMITIVE_TOPOLOGY TopologyType() const { return topologyType; }
    const PipelineSet &GetPipelineSet() const { return pipelineSet; }
private:
    friend class PipelineCreator;
    std::string name;
    PipelineType type;
    D3D12_PRIMITIVE_TOPOLOGY topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    PipelineSet pipelineSet;
};

} // namespace KashipanEngine