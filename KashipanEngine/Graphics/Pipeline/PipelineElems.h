#pragma once
#include <memory>
#include <vector>
#include <d3d12.h>

#include "Graphics/Pipeline/RootSignature.h"
#include "Graphics/Pipeline/RootParameter.h"
#include "Graphics/Pipeline/DescriptorRange.h"
#include "Graphics/Pipeline/RootConstants.h"
#include "Graphics/Pipeline/RootDescriptor.h"
#include "Graphics/Pipeline/Sampler.h"
#include "Graphics/Pipeline/InputLayout.h"
#include "Graphics/Pipeline/RasterizerState.h"
#include "Graphics/Pipeline/BlendState.h"
#include "Graphics/Pipeline/Shader.h"
#include "Graphics/Pipeline/DepthStencilState.h"
#include "Graphics/Pipeline/GraphicsPipelineState.h"
#include "Graphics/Pipeline/ComputePipelineState.h"

namespace KashipanEngine {

struct PipelineElems {
    static void Initialize(ID3D12Device *d3d12device);
    PipelineElems();
    ~PipelineElems() = default;
    void Reset();
    const std::unique_ptr<RootSignature> rootSignature;
    const std::unique_ptr<RootParameter> rootParameter;
    const std::unique_ptr<DescriptorRange> descriptorRange;
    const std::unique_ptr<RootConstants> rootConstants;
    const std::unique_ptr<RootDescriptor> rootDescriptor;
    const std::unique_ptr<Sampler> sampler;
    const std::unique_ptr<InputLayout> inputLayout;
    const std::unique_ptr<RasterizerState> rasterizerState;
    const std::unique_ptr<BlendState> blendState;
    const std::unique_ptr<Shader> shader;
    const std::unique_ptr<DepthStencilState> depthStencilState;
    const std::unique_ptr<GraphicsPipelineState> graphicsPipelineState;
    const std::unique_ptr<ComputePipelineState> computePipelineState;
};

} // namespace KashipanEngine