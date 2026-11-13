#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/JsonParser/DescriptorRange.h"
#include "Graphics/Pipeline/JsonParser/RootConstants.h"
#include "Graphics/Pipeline/JsonParser/RootDescriptor.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

// 解析済みのルートパラメーターを保持し、参照先が有効であり続けるようにディスクリプター範囲の格納領域を所有する。
struct RootParametersParsed {
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    // 各要素は1つのパラメーターのディスクリプター範囲格納領域に対応する。
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangesStorage;
};

inline RootParametersParsed ParseRootParameters(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.rootparameters.parse.start") + presetName, LogSeverity::Debug);

    using namespace KashipanEngine::Pipeline::EnumMaps;

    RootParametersParsed result{};
    if (!json.contains("Parameters")) {
        Log(Translation("engine.graphics.pipeline.jsonparser.rootparameters.parse.end") + presetName, LogSeverity::Debug);
        return result;
    }

    UINT i = 0;
    for (const auto &param : json["Parameters"]) {
        D3D12_ROOT_PARAMETER rootParam{};
        if (param.contains("ParameterType")) {
            rootParam.ParameterType = kRootParameterTypeMap.at(param["ParameterType"].get<std::string>());
        } else {
            continue;
        }
        if (param.contains("ShaderVisibility")) {
            rootParam.ShaderVisibility = kShaderVisibilityMap.at(param["ShaderVisibility"].get<std::string>());
        } else {
            continue;
        }
        if (param.contains("DescriptorTable")) {
            // メモリを維持するために範囲を保持する。
            result.rangesStorage.emplace_back(ParseDescriptorRanges(param["DescriptorTable"]));
            auto &rangesRef = result.rangesStorage.back();
            rootParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(rangesRef.size());
            rootParam.DescriptorTable.pDescriptorRanges = rangesRef.empty() ? nullptr : rangesRef.data();
        } else if (param.contains("Constants")) {
            auto constants = ParseRootConstants(param["Constants"]);
            rootParam.Constants = constants;
        } else if (param.contains("Descriptor")) {
            auto descriptor = ParseRootDescriptor(param["Descriptor"]);
            rootParam.Descriptor = descriptor;
        }
        result.parameters.push_back(rootParam);
        ++i;
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.rootparameters.parse.end") + presetName, LogSeverity::Debug);
    return result;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
