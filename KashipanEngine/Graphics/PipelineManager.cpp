#include <cassert>
#include <wrl.h>
#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/Directory.h"
#include "Debug/Logger.h"
#include "Graphics/PipelineSystem/EnumMaps.h"
#include "Graphics/PipelineSystem/DefineMaps.h"
#include "PipelineManager.h"

namespace KashipanEngine {
using namespace KashipanEngine::Pipeline::EnumMaps;
using namespace KashipanEngine::Pipeline::DefineMaps;

namespace {
static std::vector<std::string> GetFilesInDirectoryFlat(const std::string &folder, const std::vector<std::string> &exts) {
    std::vector<std::string> results;
    auto dir = GetDirectoryData(folder, false, true);
    for (auto &f : dir.files) {
        for (auto &ext : exts) {
            if (f.size() >= ext.size() && f.rfind(ext) == f.size() - ext.size()) {
                results.push_back(f);
                break;
            }
        }
    }
    return results;
}
}

PipelineManager::PipelineManager(ID3D12Device *device, const std::string &pipelineSettingsPath) {
    Log("PipelineManager constructor called.", LogSeverity::Debug);
    assert(device != nullptr);
    device_ = device;
    PipelineElems::Initialize(device_);
    shaderReflection_ = std::make_unique<ShaderReflection>(device_);

    pipelineSettingsPath_ = pipelineSettingsPath;
    Json settings = LoadJSON(pipelineSettingsPath);
    pipelineFolderPath_ = settings["PipelineFolder"].get<std::string>();
    presetFolderNames_ = settings["PresetFolders"].get<std::unordered_map<std::string, std::string>>();

    LoadPreset();
    LoadPipelines();
    Log("Pipelines loaded successfully.", LogSeverity::Info);
}

void PipelineManager::ReloadPipelines() {
    Log("Reloading Pipelines.", LogSeverity::Info);
    pipelineElems_.Reset();
    LoadPreset();
    LoadPipelines();
}

void PipelineManager::SetCommandListPipeline(ID3D12GraphicsCommandList *commandList, const std::string &pipelineName) {
    if (currentPipelineName_ == pipelineName) return;
    auto it = pipelineInfos_.find(pipelineName);
    if (it == pipelineInfos_.end()) {
        Log("Pipeline not found: " + pipelineName, LogSeverity::Error);
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
    for (const auto &presetFolder : presetFolderNames_) {
        auto presetFiles = GetFilesInDirectoryFlat(presetFolder.second, {".json", ".jsonc"});
        for (const auto &file : presetFiles) {
            kLoadFunctions_.at(presetFolder.first)(LoadJSON(file));
        }
    }
}

void PipelineManager::LoadPipelines() {
    auto pipelineFiles = GetFilesInDirectoryFlat(pipelineFolderPath_, {".json", ".jsonc"});
    for (const auto &file : pipelineFiles) {
        Json pipelineJson = LoadJSON(file);
        if (pipelineJson.contains("PipelineType")) {
            std::string type = pipelineJson["PipelineType"].get<std::string>();
            if (type == "Render") {
                LoadRenderPipeline(pipelineJson);
            } else if (type == "Compute") {
                LoadComputePipeline(pipelineJson);
            } else {
                Log("Unknown Pipeline type: " + type, LogSeverity::Warning);
            }
        } else {
            Log("Pipeline JSON is missing 'PipelineType' field.", LogSeverity::Warning);
        }
    }
}

void PipelineManager::LoadRenderPipeline(const Json &json) {
    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
        Log("Loading Pipeline: " + name, LogSeverity::Info);
    } else {
        Log("Pipeline JSON is missing 'Name' field.", LogSeverity::Warning);
    }

    HRESULT hr;
    std::unordered_map<std::string, IDxcBlob *> shaders = {
        {"Vertex", nullptr}, {"Pixel", nullptr}, {"Geometry", nullptr}, {"Hull", nullptr}, {"Domain", nullptr}
    };
    std::vector<std::string> shadersOrder = {"Vertex", "Pixel", "Geometry", "Hull", "Domain"};
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    std::vector<D3D12_ROOT_PARAMETER> rootParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    D3D12_BLEND_DESC blendDesc = {};
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};

    if (json.contains("Shader")) {
        Json shadersJson = json["Shader"];
        if (!shadersJson.contains("Vertex")) {
            Log("VertexShader is required but not found in Pipeline JSON.", LogSeverity::Warning);
            return;
        }
        for (const auto &shaderType : shadersOrder) {
            if (!shadersJson.contains(shaderType)) continue;
            Json shaderJson = shadersJson[shaderType];
            std::string shaderName;
            if (shaderJson.contains("UsePreset")) {
                std::string presetName = shaderJson["UsePreset"].get<std::string>();
                shaders[shaderType] = pipelineElems_.shader->GetShader(presetName);
                shaderName = presetName;
            } else {
                shaderJson["Name"] = name + "_" + shaderType;
                shaderJson["Type"] = shaderType;
                LoadShader(shaderJson);
                shaders[shaderType] = pipelineElems_.shader->GetShader(shaderJson["Name"].get<std::string>());
                shaderName = shaderJson["Name"].get<std::string>();
            }
            auto rootParametersForShader = pipelineElems_.rootParameter->GetRootParameter(shaderName);
            if (!rootParametersForShader.empty()) {
                rootParameters.insert(rootParameters.end(), rootParametersForShader.begin(), rootParametersForShader.end());
            }
            if (shaderType == "Vertex") {
                inputLayoutDesc = pipelineElems_.inputLayout->GetInputLayout(shaderName);
            }
        }
    }

    if (json.contains("RootSignature")) {
        Json rootSignatureJson = json["RootSignature"];
        if (rootSignatureJson.contains("UsePreset")) {
            std::string presetName = rootSignatureJson["UsePreset"].get<std::string>();
            rootSignatureDesc = pipelineElems_.rootSignature->GetRootSignatureDesc(presetName);
        } else {
            rootSignatureJson["Name"] = name;
            LoadRootSignature(rootSignatureJson);
            rootSignatureDesc = pipelineElems_.rootSignature->GetRootSignatureDesc(name);
        }
        if ((rootSignatureDesc.NumParameters > 0) && (rootSignatureDesc.pParameters != nullptr)) {
            rootParameters.clear();
        }
        if ((rootSignatureDesc.NumStaticSamplers > 0) && (rootSignatureDesc.pStaticSamplers != nullptr)) {
            samplers.clear();
        }
        if (rootSignatureJson.contains("RootParameter")) {
            rootParameters.clear();
            Json rootParameterJson = rootSignatureJson["RootParameter"];
            if (rootParameterJson.contains("UsePreset")) {
                auto presetRootParameters = pipelineElems_.rootParameter->GetRootParameter(rootParameterJson["UsePreset"].get<std::string>());
                rootParameters.insert(rootParameters.end(), presetRootParameters.begin(), presetRootParameters.end());
            } else {
                auto customRootParameters = pipelineElems_.rootParameter->GetRootParameter(name);
                rootParameters.insert(rootParameters.end(), customRootParameters.begin(), customRootParameters.end());
            }
        }
        if (rootSignatureJson.contains("Sampler")) {
            Json samplerJson = rootSignatureJson["Sampler"];
            if (samplerJson.contains("UsePreset")) {
                auto presetSamplers = pipelineElems_.sampler->GetSampler(samplerJson["UsePreset"].get<std::string>());
                samplers.insert(samplers.end(), presetSamplers.begin(), presetSamplers.end());
            } else {
                auto customSamplers = pipelineElems_.sampler->GetSampler(name);
                samplers.insert(samplers.end(), customSamplers.begin(), customSamplers.end());
            }
        }
    } else {
        Log("RootSignature is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    if (!rootParameters.empty()) {
        rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
        rootSignatureDesc.pParameters = rootParameters.data();
    }
    if (!samplers.empty()) {
        rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(samplers.size());
        rootSignatureDesc.pStaticSamplers = samplers.data();
    }

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errorBlob) {
            Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        }
        return;
    }
    hr = device_->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        Log("Failed to create root signature: " + name, LogSeverity::Error);
        return;
    }

    if (json.contains("InputLayout")) {
        Json inputLayoutJson = json["InputLayout"];
        if (inputLayoutJson.contains("UsePreset")) {
            inputLayoutDesc = pipelineElems_.inputLayout->GetInputLayout(inputLayoutJson["UsePreset"].get<std::string>());
        } else {
            inputLayoutJson["Name"] = name;
            LoadInputLayout(inputLayoutJson);
            inputLayoutDesc = pipelineElems_.inputLayout->GetInputLayout(name);
        }
    }

    if (json.contains("RasterizerState")) {
        Json rasterizerJson = json["RasterizerState"];
        if (rasterizerJson.contains("UsePreset")) {
            rasterizerDesc = pipelineElems_.rasterizerState->GetRasterizerState(rasterizerJson["UsePreset"].get<std::string>());
        } else {
            rasterizerJson["Name"] = name;
            LoadRasterizerState(rasterizerJson);
            rasterizerDesc = pipelineElems_.rasterizerState->GetRasterizerState(name);
        }
    } else {
        Log("Rasterizer state is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    if (json.contains("BlendState")) {
        Json blendJson = json["BlendState"];
        if (blendJson.contains("UsePreset")) {
            blendDesc = pipelineElems_.blendState->GetBlendState(blendJson["UsePreset"].get<std::string>());
        } else {
            blendJson["Name"] = name;
            LoadBlendState(blendJson);
            blendDesc = pipelineElems_.blendState->GetBlendState(name);
        }
    } else {
        Log("Blend state is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    if (json.contains("DepthStencilState")) {
        Json depthStencilJson = json["DepthStencilState"];
        if (depthStencilJson.contains("UsePreset")) {
            depthStencilDesc = pipelineElems_.depthStencilState->GetDepthStencilState(depthStencilJson["UsePreset"].get<std::string>());
        } else {
            depthStencilJson["Name"] = name;
            LoadDepthStencilState(depthStencilJson);
            depthStencilDesc = pipelineElems_.depthStencilState->GetDepthStencilState(name);
        }
    } else {
        Log("Depth stencil state is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    if (json.contains("PipelineState")) {
        Json pipelineStateJson = json["PipelineState"];
        if (pipelineStateJson.contains("UsePreset")) {
            pipelineDesc = pipelineElems_.graphicsPipelineState->GetPipelineState(pipelineStateJson["UsePreset"].get<std::string>());
        } else {
            pipelineStateJson["Name"] = name;
            LoadGraphicsPipelineState(pipelineStateJson);
            pipelineDesc = pipelineElems_.graphicsPipelineState->GetPipelineState(name);
        }
    } else {
        Log("Pipeline state is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    pipelineDesc.pRootSignature = rootSignature.Get();
    if (shaders["Vertex"]) {
        pipelineDesc.VS = { shaders["Vertex"]->GetBufferPointer(), shaders["Vertex"]->GetBufferSize() };
    }
    if (shaders["Pixel"]) {
        pipelineDesc.PS = { shaders["Pixel"]->GetBufferPointer(), shaders["Pixel"]->GetBufferSize() };
    }
    if (shaders["Geometry"]) {
        pipelineDesc.GS = { shaders["Geometry"]->GetBufferPointer(), shaders["Geometry"]->GetBufferSize() };
    }
    if (shaders["Hull"]) {
        pipelineDesc.HS = { shaders["Hull"]->GetBufferPointer(), shaders["Hull"]->GetBufferSize() };
    }
    if (shaders["Domain"]) {
        pipelineDesc.DS = { shaders["Domain"]->GetBufferPointer(), shaders["Domain"]->GetBufferSize() };
    }
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
    hr = device_->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        Log("Failed to create graphics pipeline state: " + name, LogSeverity::Error);
        return;
    }

    PipelineInfo info;
    info.name = name;
    info.type = json["PipelineType"].get<std::string>();
    switch (pipelineDesc.PrimitiveTopologyType) {
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: info.topologyType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: info.topologyType = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: info.topologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH: info.topologyType = D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST; break;
        default: info.topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED; break;
    }
    info.pipelineSet.rootSignature = rootSignature;
    info.pipelineSet.pipelineState = pipelineState;
    pipelineInfos_[name] = info;

    Log("Graphics pipeline created successfully: " + name, LogSeverity::Info);
}

void PipelineManager::LoadComputePipeline(const Json &json) {
    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
        Log("Loading Compute Pipeline: " + name, LogSeverity::Info);
    } else {
        Log("Compute Pipeline JSON is missing 'Name' field.", LogSeverity::Warning);
    }

    HRESULT hr;
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineDesc = {};
    std::vector<D3D12_ROOT_PARAMETER> rootParameters;
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    IDxcBlob *computeShader = nullptr;

    if (json.contains("Shader")) {
        Json shaderJson = json["Shader"];
        std::string shaderName;
        if (shaderJson.contains("UsePreset")) {
            computeShader = pipelineElems_.shader->GetShader(shaderJson["UsePreset"].get<std::string>());
            shaderName = shaderJson["UsePreset"].get<std::string>();
        } else {
            shaderJson["Name"] = name + "_Compute";
            LoadShader(shaderJson);
            computeShader = pipelineElems_.shader->GetShader(shaderJson["Name"].get<std::string>());
            shaderName = shaderJson["Name"].get<std::string>();
        }
        rootParameters = pipelineElems_.rootParameter->GetRootParameter(shaderName);
    } else {
        Log("Compute shader is required but not found in Compute Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    if (json.contains("RootSignature")) {
        Json rootSignatureJson = json["RootSignature"];
        if (rootSignatureJson.contains("UsePreset")) {
            std::string presetName = rootSignatureJson["UsePreset"].get<std::string>();
            rootSignatureDesc = pipelineElems_.rootSignature->GetRootSignatureDesc(presetName);
        } else {
            rootSignatureJson["Name"] = name;
            LoadRootSignature(rootSignatureJson);
            rootSignatureDesc = pipelineElems_.rootSignature->GetRootSignatureDesc(name);
        }
        if (rootSignatureJson.contains("RootParameter")) {
            rootParameters.clear();
            Json rootParameterJson = rootSignatureJson["RootParameter"];
            if (rootParameterJson.contains("UsePreset")) {
                auto presetRootParameters = pipelineElems_.rootParameter->GetRootParameter(rootParameterJson["UsePreset"].get<std::string>());
                rootParameters.insert(rootParameters.end(), presetRootParameters.begin(), presetRootParameters.end());
            } else {
                auto customRootParameters = pipelineElems_.rootParameter->GetRootParameter(name);
                rootParameters.insert(rootParameters.end(), customRootParameters.begin(), customRootParameters.end());
            }
        }
    } else {
        Log("RootSignature is required but not found in Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
    rootSignatureDesc.pParameters = rootParameters.data();

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errorBlob) {
            Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        }
        return;
    }
    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        Log("Failed to create root signature: " + name, LogSeverity::Error);
        return;
    }

    if (json.contains("PipelineState")) {
        Json pipelineStateJson = json["PipelineState"];
        if (pipelineStateJson.contains("UsePreset")) {
            computePipelineDesc = pipelineElems_.computePipelineState->GetPipelineState(pipelineStateJson["UsePreset"].get<std::string>());
        } else {
            pipelineStateJson["Name"] = name;
            LoadComputePipelineState(pipelineStateJson);
            computePipelineDesc = pipelineElems_.computePipelineState->GetPipelineState(name);
        }
    } else {
        Log("Pipeline state is required but not found in Compute Pipeline JSON.", LogSeverity::Warning);
        return;
    }

    computePipelineDesc.pRootSignature = rootSignature.Get();
    computePipelineDesc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
    hr = device_->CreateComputePipelineState(&computePipelineDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        Log("Failed to create compute pipeline state: " + name, LogSeverity::Error);
        return;
    }

    PipelineInfo info;
    info.name = name;
    info.type = json["PipelineType"].get<std::string>();
    info.topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    info.pipelineSet.rootSignature = rootSignature;
    info.pipelineSet.pipelineState = pipelineState;
    pipelineInfos_[name] = info;

    Log("Compute pipeline state created successfully: " + name, LogSeverity::Info);
}

void PipelineManager::LoadRootSignature(const Json &json) {
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Root signature name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("Flags")) {
        rootSignatureDesc.Flags = kRootSignatureFlagsMap.at(json["Flags"].get<std::string>());
    }
    if (json.contains("RootParameter")) {
        Json rootParameterJson = json["RootParameter"];
        std::string rootParameterName;
        if (rootParameterJson.contains("UsePreset")) {
            rootParameterName = rootParameterJson["UsePreset"].get<std::string>();
        } else {
            rootParameterJson["Name"] = name;
            LoadRootParameter(rootParameterJson);
            rootParameterName = name;
        }
        const auto &rootParameters = pipelineElems_.rootParameter->GetRootParameter(rootParameterName);
        rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
        rootSignatureDesc.pParameters = rootParameters.data();
    }
    if (json.contains("Sampler")) {
        Json samplerJson = json["Sampler"];
        std::string samplerName;
        if (samplerJson.contains("UsePreset")) {
            samplerName = samplerJson["UsePreset"].get<std::string>();
        } else {
            samplerJson["Name"] = name;
            LoadSampler(samplerJson);
            samplerName = name;
        }
        const auto &samplers = pipelineElems_.sampler->GetSampler(samplerName);
        rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(samplers.size());
        rootSignatureDesc.pStaticSamplers = samplers.data();
    }

    pipelineElems_.rootSignature->AddRootSignature(name, rootSignatureDesc);
}

void PipelineManager::LoadRootParameter(const Json &json) {
    std::vector<D3D12_ROOT_PARAMETER> rootParameters;

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Root parameter name is missing.", LogSeverity::Warning);
        return;
    }

    UINT i = 0;
    for (const auto &param : json["Parameters"]) {
        D3D12_ROOT_PARAMETER rootParam = {};
        if (param.contains("ParameterType")) {
            rootParam.ParameterType = kRootParameterTypeMap.at(param["ParameterType"].get<std::string>());
        } else {
            Log("Root parameter type is missing.", LogSeverity::Warning);
            continue;
        }
        if (param.contains("ShaderVisibility")) {
            rootParam.ShaderVisibility = kShaderVisibilityMap.at(param["ShaderVisibility"].get<std::string>());
        } else {
            Log("Root parameter shader visibility is missing.", LogSeverity::Warning);
            continue;
        }
        if (param.contains("DescriptorTable")) {
            Json descriptorTableJson = param["DescriptorTable"];
            descriptorTableJson["Name"] = name + "_" + std::to_string(i++);
            LoadDescriptorRange(descriptorTableJson);
            const auto &descriptorRanges = pipelineElems_.descriptorRange->GetDescriptorRange(descriptorTableJson["Name"].get<std::string>());
            rootParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorRanges.size());
            rootParam.DescriptorTable.pDescriptorRanges = descriptorRanges.data();
        } else if (param.contains("Constants")) {
            Json rootConstantsJson = param["Constants"];
            rootConstantsJson["Name"] = name + "_" + std::to_string(i++);
            LoadRootConstants(rootConstantsJson);
            rootParam.Constants = pipelineElems_.rootConstants->GetRootConstants(rootConstantsJson["Name"].get<std::string>());
        } else if (param.contains("Descriptor")) {
            Json rootDescriptorJson = param["Descriptor"];
            rootDescriptorJson["Name"] = name + "_" + std::to_string(i++);
            LoadRootDescriptor(rootDescriptorJson);
            rootParam.Descriptor = pipelineElems_.rootDescriptor->GetRootDescriptor(rootDescriptorJson["Name"].get<std::string>());
        }
        rootParameters.push_back(rootParam);
    }

    pipelineElems_.rootParameter->AddRootParameter(name, rootParameters);
}

void PipelineManager::LoadDescriptorRange(const Json &json) {
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Descriptor range name is missing.", LogSeverity::Warning);
        return;
    }

    for (const auto &range : json["Ranges"]) {
        D3D12_DESCRIPTOR_RANGE descriptorRange = {};
        if (range.contains("RangeType")) {
            descriptorRange.RangeType = kDescriptorRangeTypeMap.at(range["RangeType"].get<std::string>());
        } else {
            Log("Descriptor range type is missing.", LogSeverity::Warning);
            continue;
        }
        if (range.contains("NumDescriptors")) {
            descriptorRange.NumDescriptors = range["NumDescriptors"].get<UINT>();
        }
        if (range.contains("BaseShaderRegister")) {
            descriptorRange.BaseShaderRegister = range["BaseShaderRegister"].get<UINT>();
        }
        if (range.contains("RegisterSpace")) {
            descriptorRange.RegisterSpace = range["RegisterSpace"].get<UINT>();
        }
        if (range.contains("OffsetInDescriptorsFromTableStart")) {
            if (range["OffsetInDescriptorsFromTableStart"].is_string()) {
                descriptorRange.OffsetInDescriptorsFromTableStart = std::get<UINT>(kDefineMap.at(range["OffsetInDescriptorsFromTableStart"].get<std::string>()));
            } else if (range["OffsetInDescriptorsFromTableStart"].is_number()) {
                descriptorRange.OffsetInDescriptorsFromTableStart = range["OffsetInDescriptorsFromTableStart"].get<UINT>();
            }
        }
        descriptorRanges.push_back(descriptorRange);
    }

    pipelineElems_.descriptorRange->AddDescriptorRange(name, descriptorRanges);
}

void PipelineManager::LoadRootConstants(const Json &json) {
    D3D12_ROOT_CONSTANTS rootConstants = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Root constants name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("ShaderRegister")) {
        rootConstants.ShaderRegister = json["ShaderRegister"].get<UINT>();
    }
    if (json.contains("RegisterSpace")) {
        rootConstants.RegisterSpace = json["RegisterSpace"].get<UINT>();
    }
    if (json.contains("Num32BitValues")) {
        rootConstants.Num32BitValues = json["Num32BitValues"].get<UINT>();
    }

    pipelineElems_.rootConstants->AddRootConstants(name, rootConstants);
}

void PipelineManager::LoadRootDescriptor(const Json &json) {
    D3D12_ROOT_DESCRIPTOR rootDescriptor = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Root descriptor name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("ShaderRegister")) {
        rootDescriptor.ShaderRegister = json["ShaderRegister"].get<UINT>();
    }
    if (json.contains("RegisterSpace")) {
        rootDescriptor.RegisterSpace = json["RegisterSpace"].get<UINT>();
    }

    pipelineElems_.rootDescriptor->AddRootDescriptor(name, rootDescriptor);
}

void PipelineManager::LoadSampler(const Json &json) {
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Sampler name is missing.", LogSeverity::Warning);
        return;
    }

    for (const auto &sampler : json["Samplers"]) {
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        if (sampler.contains("Filter")) {
            samplerDesc.Filter = kFilterMap.at(sampler["Filter"].get<std::string>());
        }
        if (sampler.contains("AddressU")) {
            samplerDesc.AddressU = kTextureAddressModeMap.at(sampler["AddressU"].get<std::string>());
        }
        if (sampler.contains("AddressV")) {
            samplerDesc.AddressV = kTextureAddressModeMap.at(sampler["AddressV"].get<std::string>());
        }
        if (sampler.contains("AddressW")) {
            samplerDesc.AddressW = kTextureAddressModeMap.at(sampler["AddressW"].get<std::string>());
        }
        if (sampler.contains("MipLODBias")) {
            if (sampler["MipLODBias"].is_string()) {
                samplerDesc.MipLODBias = std::get<float>(kDefineMap.at(sampler["MipLODBias"].get<std::string>()));
            } else if (sampler["MipLODBias"].is_number()) {
                samplerDesc.MipLODBias = sampler["MipLODBias"].get<float>();
            }
        }
        if (sampler.contains("MaxAnisotropy")) {
            if (sampler["MaxAnisotropy"].is_string()) {
                samplerDesc.MaxAnisotropy = std::get<UINT>(kDefineMap.at(sampler["MaxAnisotropy"].get<std::string>()));
            } else if (sampler["MaxAnisotropy"].is_number()) {
                samplerDesc.MaxAnisotropy = sampler["MaxAnisotropy"].get<UINT>();
            }
        }
        if (sampler.contains("ComparisonFunc")) {
            samplerDesc.ComparisonFunc = kComparisonFuncMap.at(sampler["ComparisonFunc"].get<std::string>());
        }
        if (sampler.contains("BorderColor")) {
            samplerDesc.BorderColor = kStaticBorderColorMap.at(sampler["BorderColor"].get<std::string>());
        }
        if (sampler.contains("MinLOD")) {
            if (sampler["MinLOD"].is_string()) {
                samplerDesc.MinLOD = std::get<float>(kDefineMap.at(sampler["MinLOD"].get<std::string>()));
            } else if (sampler["MinLOD"].is_number()) {
                samplerDesc.MinLOD = sampler["MinLOD"].get<float>();
            }
        }
        if (sampler.contains("MaxLOD")) {
            if (sampler["MaxLOD"].is_string()) {
                samplerDesc.MaxLOD = std::get<float>(kDefineMap.at(sampler["MaxLOD"].get<std::string>()));
            } else if (sampler["MaxLOD"].is_number()) {
                samplerDesc.MaxLOD = sampler["MaxLOD"].get<float>();
            }
        }
        if (sampler.contains("ShaderRegister")) {
            samplerDesc.ShaderRegister = sampler["ShaderRegister"].get<UINT>();
        }
        if (sampler.contains("RegisterSpace")) {
            samplerDesc.RegisterSpace = sampler["RegisterSpace"].get<UINT>();
        }
        if (sampler.contains("ShaderVisibility")) {
            samplerDesc.ShaderVisibility = kShaderVisibilityMap.at(sampler["ShaderVisibility"].get<std::string>());
        }
        samplers.push_back(samplerDesc);
    }

    pipelineElems_.sampler->AddSampler(name, samplers);
}

void PipelineManager::LoadInputLayout(const Json &json) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Input layout name is missing.", LogSeverity::Warning);
        return;
    }

    for (const auto &element : json["Elements"]) {
        D3D12_INPUT_ELEMENT_DESC inputElement = {};
        if (element.contains("SemanticName")) {
            inputElement.SemanticName = _strdup(element["SemanticName"].get<std::string>().c_str());
        }
        if (element.contains("SemanticIndex")) {
            inputElement.SemanticIndex = element["SemanticIndex"].get<UINT>();
        }
        if (element.contains("Format")) {
            inputElement.Format = kDxgiFormatMap.at(element["Format"].get<std::string>());
        }
        if (element.contains("InputSlot")) {
            inputElement.InputSlot = element["InputSlot"].get<UINT>();
        }
        if (element.contains("AlignedByteOffset")) {
            if (element["AlignedByteOffset"].is_string()) {
                inputElement.AlignedByteOffset = std::get<UINT>(kDefineMap.at(element["AlignedByteOffset"].get<std::string>()));
            } else if (element["AlignedByteOffset"].is_number()) {
                inputElement.AlignedByteOffset = element["AlignedByteOffset"].get<UINT>();
            }
        } else {
            inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        }
        if (element.contains("InputSlotClass")) {
            inputElement.InputSlotClass = kInputClassificationMap.at(element["InputSlotClass"].get<std::string>());
        }
        if (element.contains("InstanceDataStepRate")) {
            inputElement.InstanceDataStepRate = element["InstanceDataStepRate"].get<UINT>();
        }
        inputLayout.push_back(inputElement);
    }

    pipelineElems_.inputLayout->AddInputLayout(name, inputLayout);
}

void PipelineManager::LoadRasterizerState(const Json &json) {
    D3D12_RASTERIZER_DESC rasterizerDesc = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Rasterizer state name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("FillMode")) {
        rasterizerDesc.FillMode = kFillModeMap.at(json["FillMode"].get<std::string>());
    }
    if (json.contains("CullMode")) {
        rasterizerDesc.CullMode = kCullModeMap.at(json["CullMode"].get<std::string>());
    }
    if (json.contains("FrontCounterClockwise")) {
        rasterizerDesc.FrontCounterClockwise = json["FrontCounterClockwise"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("DepthBias")) {
        rasterizerDesc.DepthBias = json["DepthBias"].get<INT>();
    }
    if (json.contains("DepthBiasClamp")) {
        rasterizerDesc.DepthBiasClamp = json["DepthBiasClamp"].get<float>();
    }
    if (json.contains("SlopeScaledDepthBias")) {
        rasterizerDesc.SlopeScaledDepthBias = json["SlopeScaledDepthBias"].get<float>();
    }
    if (json.contains("DepthClipEnable")) {
        rasterizerDesc.DepthClipEnable = json["DepthClipEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("MultisampleEnable")) {
        rasterizerDesc.MultisampleEnable = json["MultisampleEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("AntialiasedLineEnable")) {
        rasterizerDesc.AntialiasedLineEnable = json["AntialiasedLineEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("ForcedSampleCount")) {
        rasterizerDesc.ForcedSampleCount = json["ForcedSampleCount"].get<UINT>();
    }

    pipelineElems_.rasterizerState->AddRasterizerState(name, rasterizerDesc);
}

void PipelineManager::LoadBlendState(const Json &json) {
    D3D12_BLEND_DESC blendDesc = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Blend state name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("AlphaToCoverageEnable")) {
        blendDesc.AlphaToCoverageEnable = json["AlphaToCoverageEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("IndependentBlendEnable")) {
        blendDesc.IndependentBlendEnable = json["IndependentBlendEnable"].get<bool>() ? TRUE : FALSE;
    }

    UINT index = 0;
    for (const auto &target : json["RenderTargets"]) {
        D3D12_RENDER_TARGET_BLEND_DESC targetDesc = {};
        if (target.contains("BlendEnable")) {
            targetDesc.BlendEnable = target["BlendEnable"].get<bool>() ? TRUE : FALSE;
        }
        if (target.contains("LogicOpEnable")) {
            targetDesc.LogicOpEnable = target["LogicOpEnable"].get<bool>() ? TRUE : FALSE;
        }
        if (target.contains("SrcBlend")) {
            targetDesc.SrcBlend = kBlendMap.at(target["SrcBlend"].get<std::string>());
        }
        if (target.contains("DestBlend")) {
            targetDesc.DestBlend = kBlendMap.at(target["DestBlend"].get<std::string>());
        }
        if (target.contains("BlendOp")) {
            targetDesc.BlendOp = kBlendOpMap.at(target["BlendOp"].get<std::string>());
        }
        if (target.contains("SrcBlendAlpha")) {
            targetDesc.SrcBlendAlpha = kBlendMap.at(target["SrcBlendAlpha"].get<std::string>());
        }
        if (target.contains("DestBlendAlpha")) {
            targetDesc.DestBlendAlpha = kBlendMap.at(target["DestBlendAlpha"].get<std::string>());
        }
        if (target.contains("BlendOpAlpha")) {
            targetDesc.BlendOpAlpha = kBlendOpMap.at(target["BlendOpAlpha"].get<std::string>());
        }
        if (target.contains("LogicOp")) {
            targetDesc.LogicOp = kLogicOpMap.at(target["LogicOp"].get<std::string>());
        }
        if (target.contains("RenderTargetWriteMask")) {
            targetDesc.RenderTargetWriteMask = kColorWriteEnableMap.at(target["RenderTargetWriteMask"].get<std::string>());
        }
        blendDesc.RenderTarget[index] = targetDesc;
        index++;
        if (index >= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT) {
            Log("Number of render targets exceeds D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT.", LogSeverity::Warning);
            break;
        }
    }

    pipelineElems_.blendState->AddBlendState(name, blendDesc);
}

void PipelineManager::LoadShader(const Json &json) {
    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Shader name is missing.", LogSeverity::Warning);
        return;
    }

    std::string shaderType;
    if (json.contains("Type")) {
        shaderType = json["Type"].get<std::string>();
    } else {
        Log("Shader type is missing.", LogSeverity::Warning);
        return;
    }

    std::string shaderPath;
    if (json.contains("Path")) {
        shaderPath = json["Path"].get<std::string>();
    } else {
        Log("Shader path is missing.", LogSeverity::Warning);
        return;
    }

    std::string targetProfile;
    if (json.contains("TargetProfile")) {
        targetProfile = json["TargetProfile"].get<std::string>();
    } else {
        Log("Shader target profile is missing.", LogSeverity::Warning);
        return;
    }

    pipelineElems_.shader->AddShader(name, shaderPath, targetProfile);

    bool isReflection = false;
    if (json.contains("isReflection")) {
        isReflection = json["isReflection"].get<bool>();
        if (isReflection) {
            ShaderReflectionRun(name);
        }
    }
    if (!isReflection) {
        if (json.contains("RootParameter")) {
            Json rootParametersJson = json["RootParameter"];
            rootParametersJson["Name"] = name;
            LoadRootParameter(rootParametersJson);
        } else {
            Json emptyRootParameter = { {"Name", name}, {"Parameters", Json::array()} };
            LoadRootParameter(emptyRootParameter);
        }
    }
    if (shaderType == "Vertex") {
        if (json.contains("InputLayout")) {
            Json inputLayoutJson = json["InputLayout"];
            inputLayoutJson["Name"] = name;
            LoadInputLayout(inputLayoutJson);
        } else {
            Json emptyInputLayout = { {"Name", name}, {"Elements", Json::array()} };
            LoadInputLayout(emptyInputLayout);
        }
    }
}

void PipelineManager::LoadDepthStencilState(const Json &json) {
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};

    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Depth stencil state name is missing.", LogSeverity::Warning);
        return;
    }

    if (json.contains("DepthEnable")) {
        depthStencilDesc.DepthEnable = json["DepthEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("DepthWriteMask")) {
        depthStencilDesc.DepthWriteMask = kDepthWriteMaskMap.at(json["DepthWriteMask"].get<std::string>());
    }
    if (json.contains("DepthFunc")) {
        depthStencilDesc.DepthFunc = kComparisonFuncMap.at(json["DepthFunc"].get<std::string>());
    }
    if (json.contains("StencilEnable")) {
        depthStencilDesc.StencilEnable = json["StencilEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("StencilReadMask")) {
        if (json["StencilReadMask"].is_string()) {
            int stencilReadMask = std::get<int>(kDefineMap.at(json["StencilReadMask"].get<std::string>()));
            depthStencilDesc.StencilReadMask = static_cast<UINT8>(stencilReadMask);
        } else if (json["StencilReadMask"].is_number()) {
            depthStencilDesc.StencilReadMask = json["StencilReadMask"].get<UINT8>();
        }
    }
    if (json.contains("StencilWriteMask")) {
        if (json["StencilWriteMask"].is_string()) {
            int stencilWriteMask = std::get<int>(kDefineMap.at(json["StencilWriteMask"].get<std::string>()));
            depthStencilDesc.StencilWriteMask = static_cast<UINT8>(stencilWriteMask);
        } else if (json["StencilWriteMask"].is_number()) {
            depthStencilDesc.StencilWriteMask = json["StencilWriteMask"].get<UINT8>();
        }
    }
    if (json.contains("FrontFace")) {
        const auto &frontFace = json["FrontFace"];
        if (frontFace.contains("StencilFailOp")) {
            depthStencilDesc.FrontFace.StencilFailOp = kStencilOpMap.at(frontFace["StencilFailOp"].get<std::string>());
        }
        if (frontFace.contains("StencilDepthFailOp")) {
            depthStencilDesc.FrontFace.StencilDepthFailOp = kStencilOpMap.at(frontFace["StencilDepthFailOp"].get<std::string>());
        }
        if (frontFace.contains("StencilPassOp")) {
            depthStencilDesc.FrontFace.StencilPassOp = kStencilOpMap.at(frontFace["StencilPassOp"].get<std::string>());
        }
        if (frontFace.contains("StencilFunc")) {
            depthStencilDesc.FrontFace.StencilFunc = kComparisonFuncMap.at(frontFace["StencilFunc"].get<std::string>());
        }
    }
    if (json.contains("BackFace")) {
        const auto &backFace = json["BackFace"];
        if (backFace.contains("StencilFailOp")) {
            depthStencilDesc.BackFace.StencilFailOp = kStencilOpMap.at(backFace["StencilFailOp"].get<std::string>());
        }
        if (backFace.contains("StencilDepthFailOp")) {
            depthStencilDesc.BackFace.StencilDepthFailOp = kStencilOpMap.at(backFace["StencilDepthFailOp"].get<std::string>());
        }
        if (backFace.contains("StencilPassOp")) {
            depthStencilDesc.BackFace.StencilPassOp = kStencilOpMap.at(backFace["StencilPassOp"].get<std::string>());
        }
        if (backFace.contains("StencilFunc")) {
            depthStencilDesc.BackFace.StencilFunc = kComparisonFuncMap.at(backFace["StencilFunc"].get<std::string>());
        }
    }

    pipelineElems_.depthStencilState->AddDepthStencilState(name, depthStencilDesc);
}

void PipelineManager::LoadGraphicsPipelineState(const Json &json) {
    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Graphics pipeline state name is missing.", LogSeverity::Warning);
        return;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    if (json.contains("SampleMask")) {
        if (json["SampleMask"].is_string()) {
            pipelineDesc.SampleMask = std::get<UINT>(kDefineMap.at(json["SampleMask"].get<std::string>()));
        } else if (json["SampleMask"].is_number()) {
            pipelineDesc.SampleMask = json["SampleMask"].get<UINT>();
        }
    } else {
        pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    }
    if (json.contains("IBStripCutValue")) {
        pipelineDesc.IBStripCutValue = kIndexBufferStripCutValueMap.at(json["IBStripCutValue"].get<std::string>());
    }
    if (json.contains("PrimitiveTopologyType")) {
        pipelineDesc.PrimitiveTopologyType = kPrimitiveTopologyTypeMap.at(json["PrimitiveTopologyType"].get<std::string>());
    } else {
        Log("Primitive topology type is missing in graphics pipeline state JSON.", LogSeverity::Warning);
        return;
    }
    if (json.contains("NumRenderTargets")) {
        pipelineDesc.NumRenderTargets = json["NumRenderTargets"].get<UINT>();
    } else {
        pipelineDesc.NumRenderTargets = 1;
    }
    if (json.contains("RTVFormats")) {
        const auto &rtvFormats = json["RTVFormats"];
        for (UINT i = 0; i < pipelineDesc.NumRenderTargets; ++i) {
            if (i < rtvFormats.size()) {
                pipelineDesc.RTVFormats[i] = kDxgiFormatMap.at(rtvFormats[i].get<std::string>());
            } else {
                Log("Not enough RTV formats provided in graphics pipeline state JSON.", LogSeverity::Warning);
                break;
            }
        }
    } else {
        Log("RTV formats are missing in graphics pipeline state JSON.", LogSeverity::Warning);
        return;
    }
    if (json.contains("DSVFormat")) {
        pipelineDesc.DSVFormat = kDxgiFormatMap.at(json["DSVFormat"].get<std::string>());
    } else {
        Log("DSV format is missing in graphics pipeline state JSON.", LogSeverity::Warning);
        return;
    }
    if (json.contains("SampleDesc")) {
        Json sampleDescJson = json["SampleDesc"];
        if (sampleDescJson.contains("Count")) {
            pipelineDesc.SampleDesc.Count = sampleDescJson["Count"].get<UINT>();
        } else {
            Log("Sample count is missing in graphics pipeline state JSON.", LogSeverity::Warning);
            return;
        }
        if (sampleDescJson.contains("Quality")) {
            pipelineDesc.SampleDesc.Quality = sampleDescJson["Quality"].get<UINT>();
        } else {
            Log("Sample quality is missing in graphics pipeline state JSON.", LogSeverity::Warning);
            return;
        }
    } else {
        pipelineDesc.SampleDesc.Count = 1;
        pipelineDesc.SampleDesc.Quality = 0;
    }
    if (json.contains("NodeMask")) {
        pipelineDesc.NodeMask = json["NodeMask"].get<UINT>();
    }
    if (json.contains("Flags")) {
        pipelineDesc.Flags = kPipelineStateFlagsMap.at(json["Flags"].get<std::string>());
    } else {
        pipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    }

    pipelineElems_.graphicsPipelineState->AddPipelineState(name, pipelineDesc);
}

void PipelineManager::LoadComputePipelineState(const Json &json) {
    std::string name;
    if (json.contains("Name")) {
        name = json["Name"].get<std::string>();
    } else {
        Log("Compute pipeline state name is missing.", LogSeverity::Warning);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineDesc = {};
    if (json.contains("NodeMask")) {
        computePipelineDesc.NodeMask = json["NodeMask"].get<UINT>();
    }
    if (json.contains("Flags")) {
        computePipelineDesc.Flags = kPipelineStateFlagsMap.at(json["Flags"].get<std::string>());
    } else {
        computePipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    }
    pipelineElems_.computePipelineState->AddPipelineState(name, computePipelineDesc);
}

void PipelineManager::ShaderReflectionRun(const std::string &shaderName) {
    IDxcBlob *shaderBlob = pipelineElems_.shader->GetShader(shaderName);
    if (!shaderBlob) {
        Log("Shader blob not found for: " + shaderName, LogSeverity::Warning);
        return;
    }
    auto shaderReflectionInfo = shaderReflection_->GetShaderReflection(shaderBlob);
    auto rootParameters = shaderReflectionInfo.rootParameters;
    pipelineElems_.rootParameter->AddRootParameter(shaderName, rootParameters);
}

} // namespace KashipanEngine