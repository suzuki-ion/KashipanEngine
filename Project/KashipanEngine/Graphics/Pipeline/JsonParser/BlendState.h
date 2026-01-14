#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_BLEND_DESC ParseBlendState(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.blendstate.parse.start") + presetName, LogSeverity::Debug);

    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    if (json.contains("AlphaToCoverageEnable")) {
        blendDesc.AlphaToCoverageEnable = json["AlphaToCoverageEnable"].get<bool>() ? TRUE : FALSE;
    }
    if (json.contains("IndependentBlendEnable")) {
        blendDesc.IndependentBlendEnable = json["IndependentBlendEnable"].get<bool>() ? TRUE : FALSE;
    }

    if (json.contains("RenderTargets")) {
        UINT index = 0;
        for (const auto &target : json["RenderTargets"]) {
            if (index >= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT) break;
            D3D12_RENDER_TARGET_BLEND_DESC targetDesc{};

            if (target.contains("BlendEnable")) {
                targetDesc.BlendEnable = target["BlendEnable"].get<bool>() ? TRUE : FALSE;
            }
            if (target.contains("LogicOpEnable")) {
                targetDesc.LogicOpEnable = target["LogicOpEnable"].get<bool>() ? TRUE : FALSE;
            }
            if (target.contains("SrcBlend")) {
                targetDesc.SrcBlend = kBlendMap.at(target["SrcBlend"].get<std::string>());
            }
            if (target.contains("DestBlend")) {
                targetDesc.DestBlend = kBlendMap.at(target["DestBlend"].get<std::string>());
            }
            if (target.contains("BlendOp")) {
                targetDesc.BlendOp = kBlendOpMap.at(target["BlendOp"].get<std::string>());
            }
            if (target.contains("SrcBlendAlpha")) {
                targetDesc.SrcBlendAlpha = kBlendMap.at(target["SrcBlendAlpha"].get<std::string>());
            }
            if (target.contains("DestBlendAlpha")) {
                targetDesc.DestBlendAlpha = kBlendMap.at(target["DestBlendAlpha"].get<std::string>());
            }
            if (target.contains("BlendOpAlpha")) {
                targetDesc.BlendOpAlpha = kBlendOpMap.at(target["BlendOpAlpha"].get<std::string>());
            }
            if (target.contains("LogicOp")) {
                targetDesc.LogicOp = kLogicOpMap.at(target["LogicOp"].get<std::string>());
            }
            if (target.contains("RenderTargetWriteMask")) {
                targetDesc.RenderTargetWriteMask = kColorWriteEnableMap.at(target["RenderTargetWriteMask"].get<std::string>());
            }

            blendDesc.RenderTarget[index] = targetDesc;
            ++index;
        }
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.blendstate.parse.end") + presetName, LogSeverity::Debug);
    return blendDesc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
