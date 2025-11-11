#include <cassert>
#include <wrl.h>
#include <unordered_map>
#include <string>
#include <vector>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/Directory.h"
#include "Debug/Logger.h"

#include "Graphics/Pipeline/JsonParser/BlendState.h"
#include "Graphics/Pipeline/JsonParser/RasterizerState.h"
#include "Graphics/Pipeline/JsonParser/DepthStencilState.h"
#include "Graphics/Pipeline/JsonParser/InputLayout.h"
#include "Graphics/Pipeline/JsonParser/GraphicsPipelineState.h"
#include "Graphics/Pipeline/JsonParser/ComputePipelineState.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"
#include "Graphics/Pipeline/JsonParser/Shader.h"

#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/ComponentsPresetContainer.h"
#include "Graphics/PipelineManager.h"

namespace KashipanEngine {
namespace {
D3D12_PRIMITIVE_TOPOLOGY ToD3DTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    switch (type) {
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
        default: return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}
}

PipelineManager::PipelineManager(ID3D12Device *device, const std::string &pipelineSettingsPath) {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.manager.construct.start"), LogSeverity::Debug);
    assert(device != nullptr);
    device_ = device;

    shaderCompiler_ = std::make_unique<ShaderCompiler>(Passkey<PipelineManager>{}, device_);

    pipelineSettingsPath_ = pipelineSettingsPath;
    Json settings = LoadJSON(pipelineSettingsPath_);
    pipelineFolderPath_ = settings["PipelineFolder"].get<std::string>();
    presetFolderNames_ = settings["PresetFolders"].get<std::unordered_map<std::string, std::string>>();

    LoadPreset();
    LoadPipelines();
    Log(Translation("engine.graphics.pipeline.manager.construct.end"), LogSeverity::Info);
}

void PipelineManager::ReloadPipelines() {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.reload"), LogSeverity::Info);

    // Clear all caches
    pipelineInfos_.clear();
    components_.ClearAll();
    ShaderCompiler::ClearAllCompiledShaders(Passkey<PipelineManager>{});

    LoadPreset();
    LoadPipelines();
}

void PipelineManager::SetCommandListPipeline(ID3D12GraphicsCommandList *commandList, const std::string &pipelineName) {
    LogScope scope;
    if (currentPipelineName_ == pipelineName) return;
    auto it = pipelineInfos_.find(pipelineName);
    if (it == pipelineInfos_.end()) {
        Log(Translation("engine.graphics.pipeline.notfound") + pipelineName, LogSeverity::Error);
        assert(false);
        return;
    }
    currentPipelineName_ = pipelineName;
    auto topology = it->second.topologyType;
    auto &set = it->second.pipelineSet;
    commandList->IASetPrimitiveTopology(topology);
    commandList->SetGraphicsRootSignature(set.rootSignature.Get());
    commandList->SetPipelineState(set.pipelineState.Get());
}

void PipelineManager::LoadPreset() {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.loadpreset.start"), LogSeverity::Debug);

    using namespace Pipeline::JsonParser;

    for (const auto &presetFolder : presetFolderNames_) {
        const std::string &category = presetFolder.first;
        const std::string &folder = presetFolder.second;
        auto directoryData = GetDirectoryData(folder, true, true);
        auto presetFiles = GetDirectoryDataByExtension(directoryData, { ".json", ".jsonc" }).files;
        for (const auto &file : presetFiles) {
            Json j = LoadJSON(file);
            if (!j.contains("Name")) continue;
            const std::string name = j["Name"].get<std::string>();

            if (category == "BlendState") {
                components_.RegisterBlendState(name, ParseBlendState(j));
            } else if (category == "RasterizerState") {
                components_.RegisterRasterizerState(name, ParseRasterizerState(j));
            } else if (category == "DepthStencilState") {
                components_.RegisterDepthStencilState(name, ParseDepthStencilState(j));
            } else if (category == "GraphicsPipelineState") {
                components_.RegisterGraphicsPipelineState(name, ParseGraphicsPipelineState(j));
            } else if (category == "ComputePipelineState") {
                components_.RegisterComputePipelineState(name, ParseComputePipelineState(j));
            } else if (category == "InputLayout") {
                components_.RegisterInputLayout(name, ParseInputLayout(j));
            } else if (category == "RootSignature") {
                auto parsed = ParseRootSignature(j);
                components_.RegisterRootSignature(name, parsed.desc); // descriptor ranges omitted as before
            } else if (category == "Shader") {
                ShaderCompiler::CompileInfo ci{};
                ci.name = name;
                ci.filePath = j.value("Path", "");
                ci.entryPoint = j.value("EntryPoint", "main");
                ci.targetProfile = j.value("TargetProfile", "");
                if (j.contains("Macros") && j["Macros"].is_array()) {
                    for (const auto &m : j["Macros"]) {
                        if (m.contains("Name")) ci.macros.emplace_back(m["Name"].get<std::string>(), m.value("Value", ""));
                    }
                }
                auto compiled = shaderCompiler_->CompileShader(ci);
                components_.RegisterCompiledShader(name, compiled);
            } else {
                Log(Translation("engine.graphics.pipeline.load.unknown.type") + category, LogSeverity::Warning);
            }
        }
    }

    Log(Translation("engine.graphics.pipeline.loadpreset.end"), LogSeverity::Debug);
}

void PipelineManager::LoadPipelines() {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.load.start"), LogSeverity::Debug);

    auto directoryData = GetDirectoryData(pipelineFolderPath_, true, true);
    auto pipelineFiles = GetDirectoryDataByExtension(directoryData, { ".json", ".jsonc" }).files;
    for (const auto &file : pipelineFiles) {
        Json pipelineJson = LoadJSON(file);
        if (!pipelineJson.contains("PipelineType")) {
            Log(Translation("engine.graphics.pipeline.load.missing.pipelinetype"), LogSeverity::Warning);
            continue;
        }
        std::string type = pipelineJson["PipelineType"].get<std::string>();
        if (type == "Render") {
            LoadRenderPipeline(pipelineJson);
        } else if (type == "Compute") {
            LoadComputePipeline(pipelineJson);
        } else {
            Log(Translation("engine.graphics.pipeline.load.unknown.type") + type, LogSeverity::Warning);
        }
    }

    Log(Translation("engine.graphics.pipeline.load.end"), LogSeverity::Debug);
}

void PipelineManager::LoadRenderPipeline(const Json &json) {
    LogScope scope;
    using namespace Pipeline::JsonParser;

    std::string name = json.value("Name", std::string{});
    if (name.empty()) {
        Log(Translation("engine.graphics.pipeline.load.render.missingname"), LogSeverity::Warning);
        return;
    }

    Log(Translation("engine.graphics.pipeline.load.render") + name, LogSeverity::Info);
    HRESULT hr;

    // Root signature
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (components_.HasRootSignature(presetName)) {
                rootSigDesc = components_.GetRootSignature(presetName);
            }
        } else {
            auto parsed = ParseRootSignature(rootJson);
            rootSigDesc = parsed.desc;
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.rootsignature.missing"), LogSeverity::Warning);
        return;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        return;
    }
    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.pipeline.load.render.rootsignature.create.failed") + name, LogSeverity::Error);
        return;
    }

    // Shaders
    ShaderCompiler::ShaderCompiledInfo *vs = nullptr, *ps = nullptr, *gs = nullptr, *hs = nullptr, *ds = nullptr;
    if (json.contains("Shader")) {
        const auto &shadersJson = json["Shader"];
        auto parsed = Pipeline::JsonParser::ParseShaderGroup(name, shadersJson);
        for (auto &p : parsed) {
            const std::string &stage = p.first;
            const auto &entry = p.second;
            ShaderCompiler::ShaderCompiledInfo *compiled = nullptr;
            if (entry.usePreset) {
                if (components_.HasCompiledShader(entry.presetName)) compiled = components_.GetCompiledShader(entry.presetName);
            } else {
                compiled = shaderCompiler_->CompileShader(entry.compileInfo);
                components_.RegisterCompiledShader(entry.compileInfo.name, compiled);
            }
            if (stage == "Vertex") vs = compiled;
            else if (stage == "Pixel") ps = compiled;
            else if (stage == "Geometry") gs = compiled;
            else if (stage == "Hull") hs = compiled;
            else if (stage == "Domain") ds = compiled;
        }
        if (!vs) {
            Log(Translation("engine.graphics.pipeline.load.render.vertex.missing"), LogSeverity::Warning);
            return;
        }
    }

    // Input layout
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    if (json.contains("InputLayout")) {
        const auto &ilJson = json["InputLayout"];
        if (ilJson.contains("UsePreset")) {
            auto presetName = ilJson["UsePreset"].get<std::string>();
            if (components_.HasInputLayout(presetName)) {
                const auto &elements = components_.GetInputLayout(presetName);
                inputLayoutDesc.NumElements = static_cast<UINT>(elements.size());
                inputLayoutDesc.pInputElementDescs = elements.empty() ? nullptr : elements.data();
            }
        } else {
            auto elements = ParseInputLayout(ilJson);
            inputLayoutDesc.NumElements = static_cast<UINT>(elements.size());
            inputLayoutDesc.pInputElementDescs = elements.empty() ? nullptr : elements.data();
        }
    }

    // Rasterizer
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    if (json.contains("RasterizerState")) {
        const auto &rj = json["RasterizerState"];
        if (rj.contains("UsePreset")) {
            rasterizerDesc = components_.GetRasterizerState(rj["UsePreset"].get<std::string>());
        } else {
            rasterizerDesc = ParseRasterizerState(rj);
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.rasterizer.missing"), LogSeverity::Warning);
        return;
    }

    // Blend
    D3D12_BLEND_DESC blendDesc{};
    if (json.contains("BlendState")) {
        const auto &bj = json["BlendState"];
        if (bj.contains("UsePreset")) {
            blendDesc = components_.GetBlendState(bj["UsePreset"].get<std::string>());
        } else {
            blendDesc = ParseBlendState(bj);
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.blend.missing"), LogSeverity::Warning);
        return;
    }

    // DepthStencil
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    if (json.contains("DepthStencilState")) {
        const auto &dj = json["DepthStencilState"];
        if (dj.contains("UsePreset")) {
            depthStencilDesc = components_.GetDepthStencilState(dj["UsePreset"].get<std::string>());
        } else {
            depthStencilDesc = ParseDepthStencilState(dj);
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.depthstencil.missing"), LogSeverity::Warning);
        return;
    }

    // PipelineState base
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    if (json.contains("PipelineState")) {
        const auto &pj = json["PipelineState"];
        if (pj.contains("UsePreset")) {
            pipelineDesc = components_.GetGraphicsPipelineState(pj["UsePreset"].get<std::string>());
        } else {
            pipelineDesc = ParseGraphicsPipelineState(pj);
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.pso.missing"), LogSeverity::Warning);
        return;
    }

    pipelineDesc.pRootSignature = rootSignature.Get();
    if (vs) pipelineDesc.VS = { vs->GetBytecodePtr(), vs->GetBytecodeSize() };
    if (ps) pipelineDesc.PS = { ps->GetBytecodePtr(), ps->GetBytecodeSize() };
    if (gs) pipelineDesc.GS = { gs->GetBytecodePtr(), gs->GetBytecodeSize() };
    if (hs) pipelineDesc.HS = { hs->GetBytecodePtr(), hs->GetBytecodeSize() };
    if (ds) pipelineDesc.DS = { ds->GetBytecodePtr(), ds->GetBytecodeSize() };

    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
    hr = device_->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.pipeline.load.render.pso.create.failed") + name, LogSeverity::Error);
        return;
    }

    PipelineInfo info;
    info.name = name;
    info.type = json["PipelineType"].get<std::string>();
    info.topologyType = ToD3DTopology(pipelineDesc.PrimitiveTopologyType);
    info.pipelineSet.rootSignature = rootSignature;
    info.pipelineSet.pipelineState = pipelineState;
    pipelineInfos_[name] = info;

    Log(Translation("engine.graphics.pipeline.load.render.pso.create.succeeded") + name, LogSeverity::Info);
}

void PipelineManager::LoadComputePipeline(const Json &json) {
    LogScope scope;
    using namespace Pipeline::JsonParser;

    std::string name = json.value("Name", std::string{});
    if (name.empty()) {
        Log(Translation("engine.graphics.pipeline.load.compute.missingname"), LogSeverity::Warning);
        return;
    }
    Log(Translation("engine.graphics.pipeline.load.compute") + name, LogSeverity::Info);

    HRESULT hr;

    // Root signature
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (components_.HasRootSignature(presetName)) {
                rootSigDesc = components_.GetRootSignature(presetName);
            }
        } else {
            auto parsed = ParseRootSignature(rootJson);
            rootSigDesc = parsed.desc;
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.compute.rootsignature.missing"), LogSeverity::Warning);
        return;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        return;
    }
    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.pipeline.load.compute.rootsignature.create.failed") + name, LogSeverity::Error);
        return;
    }

    // Compute shader
    ShaderCompiler::ShaderCompiledInfo *cs = nullptr;
    if (json.contains("Shader")) {
        const auto &sj = json["Shader"];
        auto entry = Pipeline::JsonParser::ParseShaderEntry(name, "Compute", sj);
        if (entry.usePreset) {
            if (components_.HasCompiledShader(entry.presetName)) cs = components_.GetCompiledShader(entry.presetName);
        } else {
            cs = shaderCompiler_->CompileShader(entry.compileInfo);
            components_.RegisterCompiledShader(entry.compileInfo.name, cs);
        }
    }
    if (!cs) {
        Log(Translation("engine.graphics.pipeline.load.compute.shader.missing"), LogSeverity::Warning);
        return;
    }

    // Pipeline state base
    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    if (json.contains("PipelineState")) {
        const auto &pj = json["PipelineState"];
        if (pj.contains("UsePreset")) {
            computeDesc = components_.GetComputePipelineState(pj["UsePreset"].get<std::string>());
        } else {
            computeDesc = ParseComputePipelineState(pj);
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.compute.pso.missing"), LogSeverity::Warning);
        return;
    }

    computeDesc.pRootSignature = rootSignature.Get();
    computeDesc.CS = { cs->GetBytecodePtr(), cs->GetBytecodeSize() };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
    hr = device_->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.pipeline.load.compute.pso.create.failed") + name, LogSeverity::Error);
        return;
    }

    PipelineInfo info;
    info.name = name;
    info.type = json["PipelineType"].get<std::string>();
    info.topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    info.pipelineSet.rootSignature = rootSignature;
    info.pipelineSet.pipelineState = pipelineState;
    pipelineInfos_[name] = info;

    Log(Translation("engine.graphics.pipeline.load.compute.pso.create.succeeded") + name, LogSeverity::Info);
}

} // namespace KashipanEngine