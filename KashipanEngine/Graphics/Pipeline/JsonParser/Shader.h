#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

struct ParsedShaderStage {
    std::string stageName;                      // "Vertex" など。単体読み込み時は空でも可
    ShaderCompiler::CompileInfo compileInfo;    // UsePreset でない場合に使用
    bool isUsePreset = false;                   // UsePreset 指定か
    std::string presetName;                     // UsePreset 名
};

struct ParsedShadersInfo {
    std::vector<ParsedShaderStage> stages;      // 解析されたステージ群（単体読み込みなら1要素）
    bool isAutoTopologyFromShaders = false;     // グループJSONでの自動トポロジー指定
    bool isAutoInputLayoutFromVS = false;       // グループJSONで VS から入力レイアウト自動生成
    bool isAutoRTCountFromPS = false;           // グループJSONで PS からRT数自動取得
    bool isGroup = false;                       // グループ形式か（true: Shader{ "Vertex":{...}, ... }）
};

// 1関数=1パーサ: グループ形式/単体形式の両方を扱う
// baseDir が与えられた場合、Path/ファイルパスを相対→絶対解決する
inline ParsedShadersInfo ParseShader(const Json &json, const std::filesystem::path &baseDir = {}) {
    LogScope scope;
    const std::string debugName = json.contains("BaseName") ? json["BaseName"].get<std::string>() : json.value("Name", std::string{});
    Log(Translation("engine.graphics.pipeline.jsonparser.shader.parse.start") + debugName, LogSeverity::Debug);

    ParsedShadersInfo info{};

    static const char* kStages[] = {"Vertex", "Pixel", "Geometry", "Hull", "Domain", "Compute"};

    // グループ形式判定: いずれかのステージキーを含むか
    bool hasAnyStage = false;
    for (auto *s : kStages) { if (json.contains(s)) { hasAnyStage = true; break; } }

    auto resolvePath = [&baseDir](std::string &p) {
        if (p.empty()) return;
        std::filesystem::path pathObj(p);
        if (!baseDir.empty() && !pathObj.is_absolute()) {
            p = (baseDir / pathObj).string();
        }
    };

    if (hasAnyStage) {
        info.isGroup = true;
        const std::string baseName = json.value("BaseName", std::string{});
        if (json.contains("AutoTopologyFromShaders")) info.isAutoTopologyFromShaders = json["AutoTopologyFromShaders"].get<bool>();
        if (json.contains("AutoInputLayoutFromVS")) info.isAutoInputLayoutFromVS = json["AutoInputLayoutFromVS"].get<bool>();
        if (json.contains("AutoRTCountFromPS")) info.isAutoRTCountFromPS = json["AutoRTCountFromPS"].get<bool>();

        for (auto *stage : kStages) {
            if (!json.contains(stage)) continue;
            const Json &stageJson = json[stage];
            ParsedShaderStage parsed{};
            parsed.stageName = stage;

            // 名前決定
            std::string finalName;
            if (stageJson.contains("Name")) {
                const std::string stageNamePart = stageJson["Name"].get<std::string>();
                finalName = baseName.empty() ? stageNamePart : (baseName + "." + stageNamePart);
            } else {
                finalName = baseName.empty() ? std::string(stage) : (baseName + "." + stage);
            }

            if (stageJson.contains("UsePreset")) {
                parsed.isUsePreset = true;
                parsed.presetName = stageJson["UsePreset"].get<std::string>();
                // 別名登録で利用するため解決済み名を保持
                parsed.compileInfo.name = finalName;
            } else {
                parsed.compileInfo.name = finalName;
                parsed.compileInfo.filePath = stageJson.value("Path", "");
                resolvePath(parsed.compileInfo.filePath);
                parsed.compileInfo.entryPoint = stageJson.value("EntryPoint", "main");
                parsed.compileInfo.targetProfile = stageJson.value("TargetProfile", "");
                if (stageJson.contains("Macros") && stageJson["Macros"].is_array()) {
                    for (const auto &m : stageJson["Macros"]) {
                        if (m.contains("Name")) {
                            parsed.compileInfo.macros.emplace_back(m["Name"].get<std::string>(), m.value("Value", ""));
                        }
                    }
                }
            }
            info.stages.emplace_back(std::move(parsed));
        }
    } else {
        // 単体形式
        ParsedShaderStage parsed{};
        std::string name = json.value("Name", std::string{});
        if (json.contains("UsePreset")) {
            parsed.isUsePreset = true;
            parsed.presetName = json["UsePreset"].get<std::string>();
            if (name.empty()) name = parsed.presetName; // 名前未指定ならプリセット名
            parsed.compileInfo.name = name;             // 別名登録にも使用
        } else {
            parsed.compileInfo.name = name;
            parsed.compileInfo.filePath = json.value("Path", "");
            resolvePath(parsed.compileInfo.filePath);
            parsed.compileInfo.entryPoint = json.value("EntryPoint", "main");
            parsed.compileInfo.targetProfile = json.value("TargetProfile", "");
            if (json.contains("Macros") && json["Macros"].is_array()) {
                for (const auto &m : json["Macros"]) {
                    if (m.contains("Name")) {
                        parsed.compileInfo.macros.emplace_back(m["Name"].get<std::string>(), m.value("Value", ""));
                    }
                }
            }
        }
        info.stages.emplace_back(std::move(parsed));
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.shader.parse.end") + debugName, LogSeverity::Debug);
    return info;
}

// プリセット単体シェーダーのコンパイル補助（LoadPreset から利用）
inline ShaderCompiler::ShaderCompiledInfo *ParseAndMaybeCompileSingleShader(const Json &json, const std::filesystem::path &baseDir, ShaderCompiler *compiler, ComponentsPresetContainer *components) {
    LogScope scope;
    ParsedShadersInfo parsed = ParseShader(json, baseDir);
    if (parsed.stages.empty()) return nullptr;
    auto &stage = parsed.stages.front(); // 単体前提

    if (stage.isUsePreset) {
        if (components->HasCompiledShader(stage.presetName)) {
            auto reused = components->GetCompiledShader(stage.presetName);
            // 別名登録（compileInfo.name or Name 指定があれば）
            std::string alias = stage.compileInfo.name;
            if (alias.empty() && json.contains("Name")) alias = json["Name"].get<std::string>();
            if (!alias.empty() && alias != stage.presetName) components->RegisterCompiledShader(alias, reused);
            return reused;
        }
        Log(Translation("engine.graphics.pipeline.shader.blob.notfound") + stage.presetName, LogSeverity::Warning);
        return nullptr;
    }

    const std::string &registerName = stage.compileInfo.name;
    if (components->HasCompiledShader(registerName)) return components->GetCompiledShader(registerName);
    auto compiled = compiler->CompileShader(stage.compileInfo);
    if (compiled) components->RegisterCompiledShader(registerName, compiled);
    return compiled;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
