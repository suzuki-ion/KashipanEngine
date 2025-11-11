#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/JsonParser/DescriptorRange.h"
#include "Graphics/Pipeline/JsonParser/RootConstants.h"
#include "Graphics/Pipeline/JsonParser/RootDescriptor.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

// Holds parsed root parameters and owns descriptor range storage so pointers remain valid.
struct RootParametersParsed {
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    // Each entry corresponds to one parameter's descriptor ranges storage
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangesStorage;
};

inline RootParametersParsed ParseRootParameters(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;

    RootParametersParsed result{};
    if (!json.contains("Parameters")) return result;

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
            // Store ranges to keep memory alive
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

    return result;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
