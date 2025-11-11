#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_GRAPHICS_PIPELINE_STATE_DESC ParseGraphicsPipelineState(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};

    if (json.contains("SampleMask")) {
        if (json["SampleMask"].is_string()) {
            desc.SampleMask = std::get<UINT>(kDefineMap.at(json["SampleMask"].get<std::string>()));
        } else if (json["SampleMask"].is_number()) {
            desc.SampleMask = json["SampleMask"].get<UINT>();
        }
    } else {
        desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    }
    if (json.contains("IBStripCutValue")) {
        desc.IBStripCutValue = kIndexBufferStripCutValueMap.at(json["IBStripCutValue"].get<std::string>());
    }
    if (json.contains("PrimitiveTopologyType")) {
        desc.PrimitiveTopologyType = kPrimitiveTopologyTypeMap.at(json["PrimitiveTopologyType"].get<std::string>());
    }
    if (json.contains("NumRenderTargets")) {
        desc.NumRenderTargets = json["NumRenderTargets"].get<UINT>();
    } else {
        desc.NumRenderTargets = 1;
    }
    if (json.contains("RTVFormats")) {
        const auto &rtvFormats = json["RTVFormats"];
        for (UINT i = 0; i < desc.NumRenderTargets; ++i) {
            if (i < rtvFormats.size()) {
                desc.RTVFormats[i] = kDxgiFormatMap.at(rtvFormats[i].get<std::string>());
            }
        }
    }
    if (json.contains("DSVFormat")) {
        desc.DSVFormat = kDxgiFormatMap.at(json["DSVFormat"].get<std::string>());
    }
    if (json.contains("SampleDesc")) {
        const auto &sampleDesc = json["SampleDesc"];
        if (sampleDesc.contains("Count")) {
            desc.SampleDesc.Count = sampleDesc["Count"].get<UINT>();
        }
        if (sampleDesc.contains("Quality")) {
            desc.SampleDesc.Quality = sampleDesc["Quality"].get<UINT>();
        }
    } else {
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
    }
    if (json.contains("NodeMask")) {
        desc.NodeMask = json["NodeMask"].get<UINT>();
    }
    if (json.contains("Flags")) {
        desc.Flags = kPipelineStateFlagsMap.at(json["Flags"].get<std::string>());
    } else {
        desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    }

    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
