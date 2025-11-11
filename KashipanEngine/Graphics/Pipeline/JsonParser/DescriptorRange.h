#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline std::vector<D3D12_DESCRIPTOR_RANGE> ParseDescriptorRanges(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    if (!json.contains("Ranges")) return ranges;

    for (const auto &range : json["Ranges"]) {
        D3D12_DESCRIPTOR_RANGE r{};
        if (range.contains("RangeType")) {
            r.RangeType = kDescriptorRangeTypeMap.at(range["RangeType"].get<std::string>());
        } else {
            continue; // skip invalid
        }
        if (range.contains("NumDescriptors")) {
            r.NumDescriptors = range["NumDescriptors"].get<UINT>();
        }
        if (range.contains("BaseShaderRegister")) {
            r.BaseShaderRegister = range["BaseShaderRegister"].get<UINT>();
        }
        if (range.contains("RegisterSpace")) {
            r.RegisterSpace = range["RegisterSpace"].get<UINT>();
        }
        if (range.contains("OffsetInDescriptorsFromTableStart")) {
            if (range["OffsetInDescriptorsFromTableStart"].is_string()) {
                r.OffsetInDescriptorsFromTableStart = std::get<UINT>(kDefineMap.at(range["OffsetInDescriptorsFromTableStart"].get<std::string>()));
            } else if (range["OffsetInDescriptorsFromTableStart"].is_number()) {
                r.OffsetInDescriptorsFromTableStart = range["OffsetInDescriptorsFromTableStart"].get<UINT>();
            }
        }
        ranges.push_back(r);
    }

    return ranges;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
