#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class PipelineCreator;

enum class PipelineType {
    Render,
    Compute
};

struct PipelineSet {
    ID3D12RootSignature *RootSignature() const { return rootSignature.Get(); }
    ID3D12PipelineState *PipelineState() const { return pipelineState.Get(); }
private:
    friend class PipelineCreator;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

struct PipelineInfo {
    const std::string &Name() const { return name; }
    PipelineType Type() const { return type; }
    D3D_PRIMITIVE_TOPOLOGY TopologyType() const { return topologyType; }
    const PipelineSet &GetPipelineSet() const { return pipelineSet; }
    const std::vector<ShaderCompiler::ShaderCompiledInfo *> &Shaders() const { return shaders; }
    ShaderVariableBinder &GetVariableBinder() { return variableBinder; }
private:
    friend class PipelineCreator;
    std::string name;
    PipelineType type = PipelineType::Render;
    D3D_PRIMITIVE_TOPOLOGY topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    PipelineSet pipelineSet;
    std::vector<ShaderCompiler::ShaderCompiledInfo *> shaders;
    ShaderVariableBinder variableBinder{ Passkey<PipelineInfo>{} };
};

} // namespace KashipanEngine