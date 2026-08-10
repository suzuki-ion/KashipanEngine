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
#include "Core/ProjectPaths.h"

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
#include "Graphics/Pipeline/System/PipelineVariantResolver.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"
#include "Graphics/PipelineManager.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

#if defined(USE_IMGUI)
namespace {
PipelineManager *sActiveInstance = nullptr;
} // namespace
#endif

PipelineManager::PipelineManager(Passkey<GraphicsEngine>, ID3D12Device *device, const std::string &pipelineSettingsPath) {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.manager.construct.start"), LogSeverity::Debug);
    assert(device != nullptr);
    device_ = device;

    shaderCompiler_ = std::make_unique<ShaderCompiler>(Passkey<PipelineManager>{}, device_);
    pipelineCreator_ = std::make_unique<PipelineCreator>(Passkey<PipelineManager>{}, device_, &components_, shaderCompiler_.get());

    pipelineSettingsPath_ = ProjectPaths::ToPhysical(pipelineSettingsPath);
    Json settings = LoadJSON(pipelineSettingsPath_);
    // 設定ファイル内の参照先フォルダも論理パスで書かれているため、まとめて物理パスへ変換しておく
    pipelineFolderPath_ = ProjectPaths::ToPhysical(settings["PipelineFolder"].get<std::string>());
    presetFolderNames_ = settings["PresetFolders"].get<std::unordered_map<std::string, std::string>>();
    for (auto &[presetName, presetFolderPath] : presetFolderNames_) {
        presetFolderPath = ProjectPaths::ToPhysical(presetFolderPath);
    }

    LoadPreset();
    LoadPipelines();
#if defined(USE_IMGUI)
    sActiveInstance = this;
#endif
    Log(Translation("engine.graphics.pipeline.manager.construct.end"), LogSeverity::Info);
}

PipelineManager::~PipelineManager() {
#if defined(USE_IMGUI)
    if (sActiveInstance == this) sActiveInstance = nullptr;
#endif
}

#if defined(USE_IMGUI)
bool PipelineManager::TryGetOrCreatePipeline(const std::string &pipelineName) {
    if (!sActiveInstance) return false;
    return sActiveInstance->GetOrCreatePipeline(pipelineName);
}

const MaterialLayout *PipelineManager::TryGetMaterialLayout(const std::string &pipelineName) {
    if (!sActiveInstance || !sActiveInstance->HasPipeline(pipelineName)) return nullptr;
    return &sActiveInstance->GetPipeline(pipelineName).GetMaterialLayout();
}

std::string PipelineManager::TryGetShaderBaseDir() {
    if (!sActiveInstance) return {};
    const auto shaderFolderIt = sActiveInstance->presetFolderNames_.find("Shader");
    if (shaderFolderIt == sActiveInstance->presetFolderNames_.end()) return {};
    return shaderFolderIt->second;
}
#endif

void PipelineManager::ReloadPipelines() {
    LogScope scope;
    Log(Translation("engine.graphics.pipeline.reload"), LogSeverity::Info);

    pipelineInfos_.clear();
    components_.ClearAll();
    ShaderCompiler::ClearAllCompiledShaders(Passkey<PipelineManager>{});
    pipelineCreator_->ClearRootSignatureCache(Passkey<PipelineManager>{});

    LoadPreset();
    LoadPipelines();
}

#if defined(USE_IMGUI)
bool PipelineManager::TryReloadPipelines() {
    if (!sActiveInstance) return false;
    sActiveInstance->ReloadPipelines();
    return true;
}
#endif

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

namespace {
// ImGuiでの選択用に読み込み済みパイプライン名を保持する
std::vector<std::string> sRenderPipelineNames;
std::vector<std::string> sComputePipelineNames;
// パイプライン名からカテゴリ（PipelineInfo::Category）を引くための索引（カテゴリ絞り込み用）
std::unordered_map<std::string, std::string> sPipelineCategories;

std::vector<std::string> FilterNamesByCategory(const std::vector<std::string> &names, const std::string &category) {
    std::vector<std::string> filtered;
    bool anyCategorized = false;
    for (const auto &name : names) {
        auto it = sPipelineCategories.find(name);
        const std::string &pipelineCategory = (it != sPipelineCategories.end()) ? it->second : std::string{};
        if (!pipelineCategory.empty()) anyCategorized = true;
        if (pipelineCategory == category) filtered.push_back(name);
    }
    // 1件もCategoryが設定されていない（Categoryフィールド導入前に作られたプロジェクトの
    // パイプライン定義等）場合、絞り込むと選択肢が0件になり選択自体ができなくなってしまうため、
    // 絞り込まず全件を返す（後方互換）
    if (!anyCategorized) return names;
    return filtered;
}
} // namespace

const std::vector<std::string> &PipelineManager::GetLoadedRenderPipelineNames() {
    return sRenderPipelineNames;
}

const std::vector<std::string> &PipelineManager::GetLoadedComputePipelineNames() {
    return sComputePipelineNames;
}

std::vector<std::string> PipelineManager::GetLoadedRenderPipelineNames(const std::string &category) {
    return FilterNamesByCategory(sRenderPipelineNames, category);
}

std::vector<std::string> PipelineManager::GetLoadedComputePipelineNames(const std::string &category) {
    return FilterNamesByCategory(sComputePipelineNames, category);
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

    // ImGuiでの選択用に名前一覧を再構築する
    sRenderPipelineNames.clear();
    sComputePipelineNames.clear();
    sPipelineCategories.clear();
    for (const auto &kv : pipelineInfos_) {
        sPipelineCategories[kv.first] = kv.second.Category();
        if (kv.second.Type() == PipelineType::Render) {
            sRenderPipelineNames.push_back(kv.first);
        } else {
            sComputePipelineNames.push_back(kv.first);
        }
    }
    std::sort(sRenderPipelineNames.begin(), sRenderPipelineNames.end());
    std::sort(sComputePipelineNames.begin(), sComputePipelineNames.end());

    Log(Translation("engine.graphics.pipeline.load.end"), LogSeverity::Debug);
}

bool PipelineManager::GetOrCreatePipeline(const std::string &pipelineName) {
    LogScope scope;
    if (HasPipeline(pipelineName)) return true;

    const auto shaderFolderIt = presetFolderNames_.find("Shader");
    const std::string shaderBaseDir = (shaderFolderIt != presetFolderNames_.end()) ? shaderFolderIt->second : std::string{};
    auto resolution = TryResolvePipelineVariant(pipelineName, shaderBaseDir);
    if (!resolution.matched) return false;

    PipelineInfo info;
    if (!pipelineCreator_->CreateRender(resolution.synthesizedPipelineJson, info)) {
        Log(Translation("engine.graphics.pipeline.load.render.failed") + pipelineName, LogSeverity::Warning);
        return false;
    }
    pipelineInfos_[info.Name()] = info;

    // ImGuiでの選択用一覧にも反映する（LoadPipelines()末尾と同じ更新内容）
    sPipelineCategories[info.Name()] = info.Category();
    if (info.Type() == PipelineType::Render) {
        auto insertPos = std::lower_bound(sRenderPipelineNames.begin(), sRenderPipelineNames.end(), info.Name());
        if (insertPos == sRenderPipelineNames.end() || *insertPos != info.Name()) {
            sRenderPipelineNames.insert(insertPos, info.Name());
        }
    }

    Log(Translation("engine.graphics.pipeline.load.render.pso.create.succeeded") + info.Name() + " (dynamic variant)", LogSeverity::Info);
    return true;
}

} // namespace KashipanEngine