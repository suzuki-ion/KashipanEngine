#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <chrono>
#include <format>
#include <stdexcept>
#include "Core/ProjectPaths.h"
#include "Debug/Logger.h"
#include "Utilities/FileIO/ContentCache.h"
#pragma comment(lib, "dxgi.lib")

namespace KashipanEngine {
/// Persistent driver PSO blobs. Keys describe values, never addresses or struct padding.
class PipelineStateCache final {
    using Json = ContentCache::Json;
    using Clock = std::chrono::steady_clock;
public:
    explicit PipelineStateCache(ID3D12Device *device) : device_(device) {
        try {
            Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
                FAILED(factory->EnumAdapterByLuid(device_->GetAdapterLuid(), IID_PPV_ARGS(&adapter))) ||
                FAILED(adapter->GetDesc1(&desc))) return;
            LARGE_INTEGER driver{};
            // Some drivers do not expose this query. Cached-blob rejection still falls back safely.
            const HRESULT versionResult = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver);
            identity_ = Json{{"version", 1}, {"vendor", desc.VendorId}, {"device", desc.DeviceId},
                {"subsystem", desc.SubSysId}, {"revision", desc.Revision}, {"flags", desc.Flags},
                {"driverQuery", static_cast<long>(versionResult)}, {"driverVersion", driver.QuadPart}}.dump();
            directory_ = ContentCache::Directory(ProjectPaths::ProjectRoot(), "PSOs");
        } catch (...) { identity_.clear(); } // Cache availability must not determine rendering success.
    }

    HRESULT CreateGraphics(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, ID3DBlob *rootSignature,
        Microsoft::WRL::ComPtr<ID3D12PipelineState> &pso) {
        return Create(desc, rootSignature, pso, [this](const auto &state, auto &output) {
            return device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&output));
        });
    }
    HRESULT CreateCompute(const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc, ID3DBlob *rootSignature,
        Microsoft::WRL::ComPtr<ID3D12PipelineState> &pso) {
        return Create(desc, rootSignature, pso, [this](const auto &state, auto &output) {
            return device_->CreateComputePipelineState(&state, IID_PPV_ARGS(&output));
        });
    }
    void LogStatistics() const {
        Log(std::format("[StartupCache] PSOs: hits={}, misses={}, stored={}, rejected={}", hits_, misses_, stored_, rejected_), LogSeverity::Info);
        Log(std::format("[PSOCacheTiming] key={:.3f} ms, read={:.3f} ms, create={:.3f} ms, write={:.3f} ms",
            keyMs_, readMs_, createMs_, writeMs_), LogSeverity::Info);
    }

private:
    static double Elapsed(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
    static std::string Hash(const void *data, size_t size) {
        if (size && !data) throw std::runtime_error("Invalid PSO cache input");
        const auto result = size ? ContentCache::Digest(data, size) : ContentCache::Digest(std::string{});
        if (result.empty()) throw std::runtime_error("PSO cache hash failed");
        return result;
    }
    static std::string Shader(const D3D12_SHADER_BYTECODE &shader) { return Hash(shader.pShaderBytecode, shader.BytecodeLength); }
    static Json Stencil(const D3D12_DEPTH_STENCILOP_DESC &value) {
        return Json::array({value.StencilFailOp, value.StencilDepthFailOp, value.StencilPassOp, value.StencilFunc});
    }
    static Json Describe(const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc) {
        return Json{{"type", "compute"}, {"cs", Shader(desc.CS)}, {"node", desc.NodeMask}, {"flags", desc.Flags}};
    }
    static Json Describe(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc) {
        Json inputs = Json::array(), output = Json::array(), strides = Json::array(), blend = Json::array(), formats = Json::array();
        for (UINT i = 0; i < desc.InputLayout.NumElements; ++i) {
            const auto &e = desc.InputLayout.pInputElementDescs[i];
            inputs.push_back(Json::array({e.SemanticName ? e.SemanticName : "", e.SemanticIndex, e.Format,
                e.InputSlot, e.AlignedByteOffset, e.InputSlotClass, e.InstanceDataStepRate}));
        }
        for (UINT i = 0; i < desc.StreamOutput.NumEntries; ++i) {
            const auto &e = desc.StreamOutput.pSODeclaration[i];
            output.push_back(Json::array({e.Stream, e.SemanticName ? e.SemanticName : "", e.SemanticIndex,
                e.StartComponent, e.ComponentCount, e.OutputSlot}));
        }
        for (UINT i = 0; i < desc.StreamOutput.NumStrides; ++i) strides.push_back(desc.StreamOutput.pBufferStrides[i]);
        for (const auto &r : desc.BlendState.RenderTarget) {
            blend.push_back(Json::array({r.BlendEnable, r.LogicOpEnable, r.SrcBlend, r.DestBlend, r.BlendOp,
                r.SrcBlendAlpha, r.DestBlendAlpha, r.BlendOpAlpha, r.LogicOp, r.RenderTargetWriteMask}));
        }
        for (auto format : desc.RTVFormats) formats.push_back(format);
        const auto &r = desc.RasterizerState;
        const auto &d = desc.DepthStencilState;
        return Json{{"type", "graphics"}, {"vs", Shader(desc.VS)}, {"ps", Shader(desc.PS)}, {"gs", Shader(desc.GS)},
            {"hs", Shader(desc.HS)}, {"ds", Shader(desc.DS)}, {"inputs", inputs},
            {"streamOutput", Json::array({output, strides, desc.StreamOutput.RasterizedStream})},
            {"blend", Json::array({desc.BlendState.AlphaToCoverageEnable, desc.BlendState.IndependentBlendEnable, blend})},
            {"rasterizer", Json::array({r.FillMode, r.CullMode, r.FrontCounterClockwise, r.DepthBias, r.DepthBiasClamp,
                r.SlopeScaledDepthBias, r.DepthClipEnable, r.MultisampleEnable, r.AntialiasedLineEnable, r.ForcedSampleCount, r.ConservativeRaster})},
            {"depthStencil", Json::array({d.DepthEnable, d.DepthWriteMask, d.DepthFunc, d.StencilEnable,
                d.StencilReadMask, d.StencilWriteMask, Stencil(d.FrontFace), Stencil(d.BackFace)})},
            {"sampleMask", desc.SampleMask}, {"stripCut", desc.IBStripCutValue}, {"topology", desc.PrimitiveTopologyType},
            {"rtCount", desc.NumRenderTargets}, {"rtFormats", formats}, {"depthFormat", desc.DSVFormat},
            {"samples", Json::array({desc.SampleDesc.Count, desc.SampleDesc.Quality})}, {"node", desc.NodeMask}, {"flags", desc.Flags}};
    }

    template<class Desc, class Factory>
    HRESULT Create(Desc desc, ID3DBlob *rootSignature, Microsoft::WRL::ComPtr<ID3D12PipelineState> &pso, Factory factory) {
        std::string key;
        std::filesystem::path path;
        std::optional<std::vector<char>> cached;
        const auto keyStart = Clock::now();
        try {
            if (!identity_.empty() && rootSignature) {
                const auto rootHash = Hash(rootSignature->GetBufferPointer(), rootSignature->GetBufferSize());
                const auto description = Describe(desc).dump();
                key = ContentCache::Digest(identity_ + "|" + rootHash + "|" + description);
                if (!key.empty()) path = directory_ / (key + ".cache");
            }
        } catch (...) { key.clear(); }
        keyMs_ += Elapsed(keyStart);
        if (!key.empty()) {
            const auto readStart = Clock::now();
            // All descriptor, root-signature and shader contents are already hashed into the key.
            cached = ContentCache::Load(path, key, true);
            readMs_ += Elapsed(readStart);
        }
        if (cached) {
            desc.CachedPSO = {cached->data(), cached->size()};
            const auto createStart = Clock::now();
            const HRESULT cachedResult = factory(desc, pso);
            createMs_ += Elapsed(createStart);
            if (SUCCEEDED(cachedResult)) { ++hits_; return cachedResult; }
            ++rejected_;
            pso.Reset();
        }
        ++misses_;
        desc.CachedPSO = {};
        const auto createStart = Clock::now();
        const HRESULT result = factory(desc, pso);
        createMs_ += Elapsed(createStart);
        if (SUCCEEDED(result) && !key.empty()) {
            const auto writeStart = Clock::now();
            Microsoft::WRL::ComPtr<ID3DBlob> blob;
            if (SUCCEEDED(pso->GetCachedBlob(&blob)) && blob &&
                ContentCache::Store(path, key, Json::array(), blob->GetBufferPointer(), blob->GetBufferSize(), true)) ++stored_;
            writeMs_ += Elapsed(writeStart);
        }
        return result;
    }
    ID3D12Device *device_;
    std::string identity_;
    std::filesystem::path directory_;
    size_t hits_ = 0, misses_ = 0, stored_ = 0, rejected_ = 0;
    double keyMs_ = 0, readMs_ = 0, createMs_ = 0, writeMs_ = 0;
};
} // namespace KashipanEngine
