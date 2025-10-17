#include "PipelineElems.h"

namespace KashipanEngine {

namespace {
ID3D12Device *sD3D12Device = nullptr;
} // namespace

void PipelineElems::Initialize(ID3D12Device *d3d12device) {
    sD3D12Device = d3d12device;
    if (!sD3D12Device) {
        assert(false && "Failed to set D3D12 device");
    }
}

PipelineElems::PipelineElems() :
    rootSignature(std::make_unique<RootSignature>(sD3D12Device)),
    rootParameter(std::make_unique<RootParameter>()),
    descriptorRange(std::make_unique<DescriptorRange>()),
    rootConstants(std::make_unique<RootConstants>()),
    rootDescriptor(std::make_unique<RootDescriptor>()),
    sampler(std::make_unique<Sampler>()),
    inputLayout(std::make_unique<InputLayout>()),
    rasterizerState(std::make_unique<RasterizerState>()),
    blendState(std::make_unique<BlendState>()),
    shader(std::make_unique<Shader>()),
    depthStencilState(std::make_unique<DepthStencilState>()),
    graphicsPipelineState(std::make_unique<GraphicsPipelineState>()),
    computePipelineState(std::make_unique<ComputePipelineState>())
{}

void PipelineElems::Reset() {
    rootSignature->Clear();
    rootParameter->Clear();
    descriptorRange->Clear();
    rootConstants->Clear();
    rootDescriptor->Clear();
    sampler->Clear();
    inputLayout->Clear();
    rasterizerState->Clear();
    blendState->Clear();
    shader->Clear();
    depthStencilState->Clear();
    graphicsPipelineState->Clear();
    computePipelineState->Clear();
}

} // namespace KashipanEngine