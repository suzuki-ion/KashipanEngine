#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_DEPTH_STENCIL_DESC ParseDepthStencilState(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.depthstencilstate.parse.start") + presetName, LogSeverity::Debug);

    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    D3D12_DEPTH_STENCIL_DESC desc{};

    if (json.contains("DepthEnable")) {
        desc.DepthEnable = json["DepthEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("DepthWriteMask")) {
        desc.DepthWriteMask = kDepthWriteMaskMap.at(json["DepthWriteMask"].get<std::string>());
    }
    if (json.contains("DepthFunc")) {
        desc.DepthFunc = kComparisonFuncMap.at(json["DepthFunc"].get<std::string>());
    }
    if (json.contains("StencilEnable")) {
        desc.StencilEnable = json["StencilEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("StencilReadMask")) {
        if (json["StencilReadMask"].is_string()) {
            int stencilReadMask = std::get<int>(kDefineMap.at(json["StencilReadMask"].get<std::string>()));
            desc.StencilReadMask = static_cast<UINT8>(stencilReadMask);
        } else if (json["StencilReadMask"].is_number()) {
            desc.StencilReadMask = json["StencilReadMask"].get<UINT8>();
        }
    }
    if (json.contains("StencilWriteMask")) {
        if (json["StencilWriteMask"].is_string()) {
            int stencilWriteMask = std::get<int>(kDefineMap.at(json["StencilWriteMask"].get<std::string>()));
            desc.StencilWriteMask = static_cast<UINT8>(stencilWriteMask);
        } else if (json["StencilWriteMask"].is_number()) {
            desc.StencilWriteMask = json["StencilWriteMask"].get<UINT8>();
        }
    }
    if (json.contains("FrontFace")) {
        const auto &frontFace = json["FrontFace"];
        if (frontFace.contains("StencilFailOp")) {
            desc.FrontFace.StencilFailOp = kStencilOpMap.at(frontFace["StencilFailOp"].get<std::string>());
        }
        if (frontFace.contains("StencilDepthFailOp")) {
            desc.FrontFace.StencilDepthFailOp = kStencilOpMap.at(frontFace["StencilDepthFailOp"].get<std::string>());
        }
        if (frontFace.contains("StencilPassOp")) {
            desc.FrontFace.StencilPassOp = kStencilOpMap.at(frontFace["StencilPassOp"].get<std::string>());
        }
        if (frontFace.contains("StencilFunc")) {
            desc.FrontFace.StencilFunc = kComparisonFuncMap.at(frontFace["StencilFunc"].get<std::string>());
        }
    }
    if (json.contains("BackFace")) {
        const auto &backFace = json["BackFace"];
        if (backFace.contains("StencilFailOp")) {
            desc.BackFace.StencilFailOp = kStencilOpMap.at(backFace["StencilFailOp"].get<std::string>());
        }
        if (backFace.contains("StencilDepthFailOp")) {
            desc.BackFace.StencilDepthFailOp = kStencilOpMap.at(backFace["StencilDepthFailOp"].get<std::string>());
        }
        if (backFace.contains("StencilPassOp")) {
            desc.BackFace.StencilPassOp = kStencilOpMap.at(backFace["StencilPassOp"].get<std::string>());
        }
        if (backFace.contains("StencilFunc")) {
            desc.BackFace.StencilFunc = kComparisonFuncMap.at(backFace["StencilFunc"].get<std::string>());
        }
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.depthstencilstate.parse.end") + presetName, LogSeverity::Debug);
    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
