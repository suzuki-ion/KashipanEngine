#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_RASTERIZER_DESC ParseRasterizerState(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.rasterizerstate.parse.start") + presetName, LogSeverity::Debug);

    using namespace KashipanEngine::Pipeline::EnumMaps;
    D3D12_RASTERIZER_DESC desc{};

    if (json.contains("FillMode")) {
        desc.FillMode = kFillModeMap.at(json["FillMode"].get<std::string>());
    }
    if (json.contains("CullMode")) {
        desc.CullMode = kCullModeMap.at(json["CullMode"].get<std::string>());
    }
    if (json.contains("FrontCounterClockwise")) {
        desc.FrontCounterClockwise = json["FrontCounterClockwise"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("DepthBias")) {
        desc.DepthBias = json["DepthBias"].get<INT>();
    }
    if (json.contains("DepthBiasClamp")) {
        desc.DepthBiasClamp = json["DepthBiasClamp"].get<float>();
    }
    if (json.contains("SlopeScaledDepthBias")) {
        desc.SlopeScaledDepthBias = json["SlopeScaledDepthBias"].get<float>();
    }
    if (json.contains("DepthClipEnable")) {
        desc.DepthClipEnable = json["DepthClipEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("MultisampleEnable")) {
        desc.MultisampleEnable = json["MultisampleEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("AntialiasedLineEnable")) {
        desc.AntialiasedLineEnable = json["AntialiasedLineEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("ForcedSampleCount")) {
        desc.ForcedSampleCount = json["ForcedSampleCount"].get<UINT>();
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.rasterizerstate.parse.end") + presetName, LogSeverity::Debug);
    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
