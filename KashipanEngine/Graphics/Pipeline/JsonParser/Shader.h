#pragma once
#include <string>
#include <vector>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

struct ShaderParseResult {
    ShaderCompiler::CompileInfo compileInfo; // filled if needs compile
    bool usePreset = false;                  // true if UsePreset path
    std::string presetName;                  // preset shader name when usePreset
};

inline ShaderParseResult ParseShaderEntry(const std::string &baseName, const std::string &stageKey, const Json &shaderJson) {
    ShaderParseResult result;
    if (shaderJson.contains("UsePreset")) {
        result.usePreset = true;
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

inline std::vector<std::pair<std::string, ShaderParseResult>> ParseShaderGroup(const std::string &baseName, const Json &shadersJson) {
    static const char *kStages[] = {"Vertex", "Pixel", "Geometry", "Hull", "Domain", "Compute"};
    std::vector<std::pair<std::string, ShaderParseResult>> results;
    for (auto *stage : kStages) {
        if (!shadersJson.contains(stage)) continue;
        results.emplace_back(stage, ParseShaderEntry(baseName, stage, shadersJson[stage]));
    }
    return results;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
