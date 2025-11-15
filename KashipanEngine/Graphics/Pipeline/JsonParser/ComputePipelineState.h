#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_COMPUTE_PIPELINE_STATE_DESC ParseComputePipelineState(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.cps.parse.start") + presetName, LogSeverity::Debug);

    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    if (json.contains("NodeMask")) {
        desc.NodeMask = json["NodeMask"].get<UINT>();
    }
    if (json.contains("Flags")) {
        desc.Flags = kPipelineStateFlagsMap.at(json["Flags"].get<std::string>());
    } else {
        desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.cps.parse.end") + presetName, LogSeverity::Debug);
    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
