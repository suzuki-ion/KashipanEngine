#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_RASTERIZER_DESC ParseRasterizerState(const Json &json) {
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

    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
