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
#include "Graphics/Pipeline/System/PipelineCreator.h"
#include "Graphics/PipelineManager.h"

namespace KashipanEngine {

PipelineManager::PipelineManager(Passkey<DirectXCommon>, ID3D12Device *device, const std::string &pipelineSettingsPath) {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.manager.construct.start"), LogSeverity::Debug);
    assert(device != nullptr);
    device_ = device;

    shaderCompiler_ = std::make_unique<ShaderCompiler>(Passkey<PipelineManager>{}, device_);
    pipelineCreator_ = std::make_unique<PipelineCreator>(Passkey<PipelineManager>{}, device_, &components_, shaderCompiler_.get());

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
    auto topology = it->second.TopologyType();
    const auto &set = it->second.GetPipelineSet();
    commandList->IASetPrimitiveTopology(topology);
    commandList->SetGraphicsRootSignature(set.RootSignature());
    commandList->SetPipelineState(set.PipelineState());
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
                components_.RegisterRootSignature(name, parsed.desc);
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
    PipelineInfo info;
    if (!pipelineCreator_->CreateRender(json, info)) {
        Log(Translation("engine.graphics.pipeline.load.render.failed") + json.value("Name", ""), LogSeverity::Warning);
        return;
    }
    pipelineInfos_[info.Name()] = info;
    Log(Translation("engine.graphics.pipeline.load.render.pso.create.succeeded") + info.Name(), LogSeverity::Info);
}

void PipelineManager::LoadComputePipeline(const Json &json) {
    PipelineInfo info;
    if (!pipelineCreator_->CreateCompute(json, info)) {
        Log(Translation("engine.graphics.pipeline.load.compute.failed") + json.value("Name", ""), LogSeverity::Warning);
        return;
    }
    pipelineInfos_[info.Name()] = info;
}

} // namespace KashipanEngine