#pragma once
#include <string>
#include <vector>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

struct ShaderParseResult {
    ShaderCompiler::CompileInfo compileInfo;    // シェーダーのコンパイル情報（コンパイルが必要な場合に設定される）
    bool isUsePreset = false;   // UsePreset パスが指定されている場合に true
    std::string presetName;     // UsePreset が指定されている場合のプリセットシェーダー名
};

inline ShaderParseResult ParseShaderEntry(const std::string &baseName, const std::string &stageKey, const Json &shaderJson) {
    ShaderParseResult result;
    if (shaderJson.contains("UsePreset")) {
        result.isUsePreset = true;
        result.presetName = shaderJson["UsePreset"].get<std::string>();
        return result;
    }
    result.compileInfo.name = baseName + "_" + stageKey;
    result.compileInfo.filePath = shaderJson.value("Path", "");
    result.compileInfo.entryPoint = shaderJson.value("EntryPoint", "main");
    result.compileInfo.targetProfile = shaderJson.value("TargetProfile", "");
    if (shaderJson.contains("Macros") && shaderJson["Macros"].is_array()) {
        for (const auto &m : shaderJson["Macros"]) {
            if (m.contains("Name")) {
                result.compileInfo.macros.emplace_back(m["Name"].get<std::string>(), m.value("Value", ""));
            }
        }
    }
    return result;
}

struct ShaderGroupParsedInfo {
    std::vector<std::pair<std::string, ShaderParseResult>> stages;  // ステージ名 -> パース結果
    bool isAutoTopologyFromShaders = false; // HS/DS が存在する場合、パッチトポロジーを優先する
    bool isAutoInputLayoutFromVS = false;   // VS リフレクションから入力レイアウトを生成することを優先する
    bool isAutoRTCountFromPS = false;       // グラフィックス PSO からのミラー（利便性のため）
};

inline std::vector<std::pair<std::string, ShaderParseResult>> ParseShaderGroup(const std::string &baseName, const Json &shadersJson) {
    static const char *kStages[] = {"Vertex", "Pixel", "Geometry", "Hull", "Domain", "Compute"};
    std::vector<std::pair<std::string, ShaderParseResult>> results;
    for (auto *stage : kStages) {
        if (!shadersJson.contains(stage)) continue;
        results.emplace_back(stage, ParseShaderEntry(baseName, stage, shadersJson[stage]));
    }
    return results;
}

inline ShaderGroupParsedInfo ParseShaderGroupInfo(const std::string &baseName, const Json &shadersJson) {
    ShaderGroupParsedInfo info{};
    info.stages = ParseShaderGroup(baseName, shadersJson);
    if (shadersJson.contains("AutoTopologyFromShaders")) info.isAutoTopologyFromShaders = shadersJson["AutoTopologyFromShaders"].get<bool>();
    if (shadersJson.contains("AutoInputLayoutFromVS")) info.isAutoInputLayoutFromVS = shadersJson["AutoInputLayoutFromVS"].get<bool>();
    if (shadersJson.contains("AutoRTCountFromPS")) info.isAutoRTCountFromPS = shadersJson["AutoRTCountFromPS"].get<bool>();
    return info;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
