#include "Graphics/Pipeline/System/PipelineStateCache.h"
#include <d3dcompiler.h>
#include <cassert>
#include <iostream>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
namespace {
std::string projectRoot;
std::vector<std::string> messages;
using Microsoft::WRL::ComPtr;
using KashipanEngine::PipelineStateCache;
using KashipanEngine::ContentCache::Path;
ComPtr<ID3DBlob> Compile(const char *source, const char *target) {
    ComPtr<ID3DBlob> code, errors;
    const auto result = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, "main", target, 0, 0, &code, &errors);
    if (FAILED(result) && errors) std::cerr << static_cast<const char *>(errors->GetBufferPointer());
    assert(SUCCEEDED(result));
    return code;
}
void Expect(PipelineStateCache &cache, const std::string &counts) {
    cache.LogStatistics();
    assert(messages.size() >= 2);
    assert(messages[messages.size()-2].find(counts) != std::string::npos);
}
}
namespace KashipanEngine {
void Log(const std::string &text, LogSeverity) { messages.push_back(text); }
const std::string &ProjectPaths::ProjectRoot() { return projectRoot; }
}
int main() {
    const auto root = std::filesystem::absolute("Generated/PSOCacheRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    projectRoot = KashipanEngine::ContentCache::PathText(root);
    ComPtr<ID3D12Device> device;
    assert(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))));
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature, errors;
    assert(SUCCEEDED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors)));
    ComPtr<ID3D12RootSignature> rootSignature;
    assert(SUCCEEDED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))));
    auto vs = Compile("float4 main(uint id:SV_VertexID):SV_Position{return float4(0,0,0,1);}", "vs_5_0");
    auto ps = Compile("float4 main():SV_Target{return float4(1,0,0,1);}", "ps_5_0");
    auto green = Compile("float4 main():SV_Target{return float4(0,1,0,1);}", "ps_5_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature.Get();
    desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    for (auto &blend : desc.BlendState.RenderTarget) {
        blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.StencilReadMask = desc.DepthStencilState.StencilWriteMask = 0xff;
    auto &front = desc.DepthStencilState.FrontFace;
    front.StencilFailOp = front.StencilDepthFailOp = front.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    front.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.BackFace = front;
    desc.SampleMask = UINT_MAX;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pso;
    PipelineStateCache first(device.Get());
    assert(SUCCEEDED(first.CreateGraphics(desc, signature.Get(), pso)));
    Expect(first,"hits=0, misses=1, stored=1, rejected=0");
    const auto directory = root / "Cache/Startup/PSOs";
    const auto originalPath = std::filesystem::directory_iterator(directory)->path();
    PipelineStateCache second(device.Get());
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, signature.Get(), pso)));
    Expect(second,"hits=1, misses=0, stored=0, rejected=0");
    desc.PS = {green->GetBufferPointer(), green->GetBufferSize()};
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, signature.Get(), pso)));
    Expect(second,"hits=1, misses=1, stored=1, rejected=0");
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, signature.Get(), pso)));
    Expect(second,"hits=1, misses=2, stored=2, rejected=0");
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> otherSignature;
    assert(SUCCEEDED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &otherSignature, &errors)));
    ComPtr<ID3D12RootSignature> otherRoot;
    assert(SUCCEEDED(device->CreateRootSignature(0, otherSignature->GetBufferPointer(), otherSignature->GetBufferSize(), IID_PPV_ARGS(&otherRoot))));
    desc.pRootSignature = otherRoot.Get();
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, otherSignature.Get(), pso)));
    Expect(second,"hits=1, misses=3, stored=3, rejected=0");
    desc.pRootSignature = rootSignature.Get();
    std::ofstream(originalPath,std::ios::binary) << "corrupt";
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, signature.Get(), pso)));
    Expect(second,"hits=1, misses=4, stored=4, rejected=0");
    // Intact container but unusable driver blob: API rejection must retry without the blob.
    const std::string badBlob(64,'x');
    assert(KashipanEngine::ContentCache::Store(originalPath,originalPath.stem().string(),
        KashipanEngine::ContentCache::Json::array(),badBlob.data(),badBlob.size(),true));
    pso.Reset();
    assert(SUCCEEDED(second.CreateGraphics(desc, signature.Get(), pso)));
    Expect(second,"hits=1, misses=5, stored=5, rejected=1");
    // Other cache users still require dependencies unless they explicitly opt in.
    assert(!KashipanEngine::ContentCache::Load(originalPath,originalPath.stem().string()));
    auto cs = Compile("[numthreads(1,1,1)] void main(){}", "cs_5_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC compute{};
    compute.pRootSignature = rootSignature.Get();
    compute.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
    pso.Reset();
    assert(SUCCEEDED(second.CreateCompute(compute, signature.Get(), pso)));
    Expect(second,"hits=1, misses=6, stored=6, rejected=1");
    pso.Reset();
    assert(SUCCEEDED(second.CreateCompute(compute, signature.Get(), pso)));
    Expect(second,"hits=2, misses=6, stored=6, rejected=1");
    std::cout << "PSO cache regression passed: graphics/compute reuse, shader/state/root invalidation, corruption, driver rejection\n";
}
