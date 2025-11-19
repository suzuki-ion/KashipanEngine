#include <cassert>
#include <wrl.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <cctype>

#include "Utilities/FileIO/JSON.h"
#include "Utilities/FileIO/Directory.h"

#include "Graphics/Pipeline/JsonParser/BlendState.h"
#include "Graphics/Pipeline/JsonParser/RasterizerState.h"
#include "Graphics/Pipeline/JsonParser/DepthStencilState.h"
#include "Graphics/Pipeline/JsonParser/InputLayout.h"
#include "Graphics/Pipeline/JsonParser/GraphicsPipelineState.h"
#include "Graphics/Pipeline/JsonParser/ComputePipelineState.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"
#include "Graphics/Pipeline/JsonParser/Shader.h"
#include "Graphics/Pipeline/JsonParser/DescriptorRange.h"
#include "Graphics/Pipeline/JsonParser/RootConstants.h"
#include "Graphics/Pipeline/JsonParser/RootDescriptor.h"
#include "Graphics/Pipeline/JsonParser/RootParameter.h"
#include "Graphics/Pipeline/JsonParser/SamplerState.h"

#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/PipelineCreator.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"
#include "Graphics/PipelineManager.h"

namespace KashipanEngine {

PipelineManager::PipelineManager(Passkey<GraphicsEngine>, ID3D12Device *device, const std::string &pipelineSettingsPath) {
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

    pipelineInfos_.clear();
    components_.ClearAll();
    ShaderCompiler::ClearAllCompiledShaders(Passkey<PipelineManager>{});

    LoadPreset();
    LoadPipelines();
}

void PipelineManager::ApplyPipeline(ID3D12GraphicsCommandList* commandList, const std::string &pipelineName) {
    LogScope scope;
    assert(commandList != nullptr);
    auto it = pipelineInfos_.find(pipelineName);
    if (it == pipelineInfos_.end()) {
        Log(Translation("engine.graphics.pipeline.notfound") + pipelineName, LogSeverity::Error);
        assert(false);
        return;
    }
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

    static const std::unordered_map<std::string, std::function<void(const Json&, const std::string&, const std::filesystem::path &)>> handlers = {
        {"BlendState",              [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterBlendState(n, ParseBlendState(j)); }},
        {"ComputePipelineState",    [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterComputePipelineState(n, ParseComputePipelineState(j)); }},
        {"DepthStencilState",       [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterDepthStencilState(n, ParseDepthStencilState(j)); }},
        {"DescriptorRange",         [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterDescriptorRange(n, ParseDescriptorRanges(j)); }},
        {"GraphicsPipelineState",   [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterGraphicsPipelineState(n, ParseGraphicsPipelineState(j)); }},
        {"InputLayout",             [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterInputLayout(n, ParseInputLayout(j)); }},
        {"RasterizerState",         [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterRasterizerState(n, ParseRasterizerState(j)); }},
        {"RootConstants",           [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterRootConstants(n, ParseRootConstants(j)); }},
        {"RootDescriptor",          [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterRootDescriptor(n, ParseRootDescriptor(j)); }},
        {"RootParameter",           [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterRootParameter(n, ParseRootParameters(j).parameters); }},
        {"RootSignature",           [this](const Json &j, const std::string &n, const std::filesystem::path &){ auto p = ParseRootSignature(j); components_.RegisterRootSignature(n, p); }},
        {"Sampler",                 [this](const Json &j, const std::string &n, const std::filesystem::path &){ components_.RegisterSampler(n, ParseSamplerState(j)); }}
        // Shader は下の特別処理で扱う
    };

    auto toLower = [](const std::string &s){
        std::string out; out.reserve(s.size());
        for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
        return out;
    };

    for (const auto &presetFolder : presetFolderNames_) {
        const std::string &category = presetFolder.first;
        const std::string &folder = presetFolder.second;
        auto directoryData = GetDirectoryData(folder, true, true);
        auto presetFiles = GetDirectoryDataByExtension(directoryData, { ".json", ".jsonc" }).files;

        for (const auto &file : presetFiles) {
            // example スキップ
            std::filesystem::path p(file);
            std::string fnameLower = toLower(p.filename().string());
            if (fnameLower == "example.json" || fnameLower == "example.jsonc" || toLower(p.stem().string()) == "example") continue;

            Json j = LoadJSON(file);
            if (j.contains("Name") && j["Name"].is_string()) {
                if (toLower(j["Name"].get<std::string>()) == "example") continue;
            }

            const std::filesystem::path baseDir = p.parent_path();

            if (category == "Shader") {
                // Shader 特別処理: グループ/単体すべてのステージを処理
                auto parsed = ParseShader(j, baseDir);
                for (auto &stage : parsed.stages) {
                    if (stage.isUsePreset) {
                        if (components_.HasCompiledShader(stage.presetName)) {
                            auto reused = components_.GetCompiledShader(stage.presetName);
                            if (!stage.compileInfo.name.empty() && stage.compileInfo.name != stage.presetName) {
                                components_.RegisterCompiledShader(stage.compileInfo.name, reused);
                            }
                        } else {
                            Log(Translation("engine.graphics.pipeline.shader.blob.notfound") + stage.presetName + " " + Translation("label.filepath") + file, LogSeverity::Warning);
                        }
                    } else {
                        auto compiled = shaderCompiler_->CompileShader(stage.compileInfo);
                        if (compiled) components_.RegisterCompiledShader(stage.compileInfo.name, compiled);
                        else Log(Translation("engine.graphics.shadercompiler.compile.failed") + stage.compileInfo.filePath + " " + Translation("label.filepath") + file, LogSeverity::Error);
                    }
                }
                continue;
            }

            // それ以外は従来通り
            if (!j.contains("Name")) continue;
            const std::string name = j["Name"].get<std::string>();
            auto it = handlers.find(category);
            if (it == handlers.end()) {
                Log(Translation("engine.graphics.pipeline.load.unknown.type") + category + " " + Translation("label.filepath") + file, LogSeverity::Warning);
                continue;
            }
            const auto &handler = it->second;
            handler(j, name, baseDir);
        }
    }

    Log(Translation("engine.graphics.pipeline.loadpreset.end"), LogSeverity::Debug);
}

void PipelineManager::LoadPipelines() {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.load.start"), LogSeverity::Debug);

    auto toLower = [](const std::string &s){
        std::string out; out.reserve(s.size());
        for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
        return out;
    };

    auto directoryData = GetDirectoryData(pipelineFolderPath_, true, true);
    auto pipelineFiles = GetDirectoryDataByExtension(directoryData, { ".json", ".jsonc" }).files;
    for (const auto &file : pipelineFiles) {
        // ファイル名が example の場合はスキップ
        std::filesystem::path p(file);
        std::string fnameLower = toLower(p.filename().string());
        if (fnameLower == "example.json" || fnameLower == "example.jsonc" || toLower(p.stem().string()) == "example") continue;

        Json pipelineJson = LoadJSON(file);
        // JSON の Name が "example" の場合もスキップ
        if (pipelineJson.contains("Name") && pipelineJson["Name"].is_string()) {
            if (toLower(pipelineJson["Name"].get<std::string>()) == "example") continue;
        }

        if (!pipelineJson.contains("PipelineType")) {
            Log(Translation("engine.graphics.pipeline.load.missing.pipelinetype") + std::string(" ") + Translation("label.filepath") + file, LogSeverity::Warning);
            continue;
        }
        std::string type = pipelineJson["PipelineType"].get<std::string>();
        if (type == "Render") {
            PipelineInfo info;
            Log(Translation("engine.graphics.pipeline.load.render.pso.create.start") + pipelineJson.value("Name", std::string()) + " " + Translation("label.filepath") + file, LogSeverity::Info);
            if (!pipelineCreator_->CreateRender(pipelineJson, info)) {
                Log(Translation("engine.graphics.pipeline.load.render.failed") + pipelineJson.value("Name", std::string()) + " " + Translation("label.filepath") + file, LogSeverity::Warning);
                continue;
            }
            pipelineInfos_[info.Name()] = info;
            Log(Translation("engine.graphics.pipeline.load.render.pso.create.succeeded") + info.Name(), LogSeverity::Info);
        } else if (type == "Compute") {
            PipelineInfo info;
            Log(Translation("engine.graphics.pipeline.load.compute.pso.create.start") + pipelineJson.value("Name", std::string()) + " " + Translation("label.filepath") + file, LogSeverity::Info);
            if (!pipelineCreator_->CreateCompute(pipelineJson, info)) {
                Log(Translation("engine.graphics.pipeline.load.compute.failed") + pipelineJson.value("Name", std::string()) + " " + Translation("label.filepath") + file, LogSeverity::Warning);
                continue;
            }
            pipelineInfos_[info.Name()] = info;
            Log(Translation("engine.graphics.pipeline.load.compute.pso.create.succeeded") + info.Name(), LogSeverity::Info);
        } else {
            Log(Translation("engine.graphics.pipeline.load.unknown.type") + type + " " + Translation("label.filepath") + file, LogSeverity::Warning);
        }
    }

    Log(Translation("engine.graphics.pipeline.load.end"), LogSeverity::Debug);
}

} // namespace KashipanEngine