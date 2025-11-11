#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/JsonParser/RootParameter.h"
#include "Graphics/Pipeline/JsonParser/SamplerState.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

// Holds parsed root signature parts and keeps parameter memory valid.
struct RootSignatureParsed {
    D3D12_ROOT_SIGNATURE_DESC desc{};
    RootParametersParsed rootParams;
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
};

inline RootSignatureParsed ParseRootSignature(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;

    RootSignatureParsed result{};
    auto &desc = result.desc;

    if (json.contains("Flags")) {
        desc.Flags = kRootSignatureFlagsMap.at(json["Flags"].get<std::string>());
    }

    if (json.contains("RootParameter")) {
        result.rootParams = ParseRootParameters(json["RootParameter"]);
        desc.NumParameters = static_cast<UINT>(result.rootParams.parameters.size());
        desc.pParameters = result.rootParams.parameters.empty() ? nullptr : result.rootParams.parameters.data();
    }

    if (json.contains("Sampler")) {
        result.samplers = ParseSamplerState(json["Sampler"]);
        desc.NumStaticSamplers = static_cast<UINT>(result.samplers.size());
        desc.pStaticSamplers = result.samplers.empty() ? nullptr : result.samplers.data();
    }

    return result;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
