#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline std::vector<D3D12_STATIC_SAMPLER_DESC> ParseSamplerState(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
    if (!json.contains("Samplers")) return samplers;

    for (const auto &sampler : json["Samplers"]) {
        D3D12_STATIC_SAMPLER_DESC samplerDesc{};
        if (sampler.contains("Filter")) {
            samplerDesc.Filter = kFilterMap.at(sampler["Filter"].get<std::string>());
        }
        if (sampler.contains("AddressU")) {
            samplerDesc.AddressU = kTextureAddressModeMap.at(sampler["AddressU"].get<std::string>());
        }
        if (sampler.contains("AddressV")) {
            samplerDesc.AddressV = kTextureAddressModeMap.at(sampler["AddressV"].get<std::string>());
        }
        if (sampler.contains("AddressW")) {
            samplerDesc.AddressW = kTextureAddressModeMap.at(sampler["AddressW"].get<std::string>());
        }
        if (sampler.contains("MipLODBias")) {
            if (sampler["MipLODBias"].is_string()) {
                samplerDesc.MipLODBias = std::get<float>(kDefineMap.at(sampler["MipLODBias"].get<std::string>()));
            } else if (sampler["MipLODBias"].is_number()) {
                samplerDesc.MipLODBias = sampler["MipLODBias"].get<float>();
            }
        }
        if (sampler.contains("MaxAnisotropy")) {
            if (sampler["MaxAnisotropy"].is_string()) {
                samplerDesc.MaxAnisotropy = std::get<UINT>(kDefineMap.at(sampler["MaxAnisotropy"].get<std::string>()));
            } else if (sampler["MaxAnisotropy"].is_number()) {
                samplerDesc.MaxAnisotropy = sampler["MaxAnisotropy"].get<UINT>();
            }
        }
        if (sampler.contains("ComparisonFunc")) {
            samplerDesc.ComparisonFunc = kComparisonFuncMap.at(sampler["ComparisonFunc"].get<std::string>());
        }
        if (sampler.contains("BorderColor")) {
            samplerDesc.BorderColor = kStaticBorderColorMap.at(sampler["BorderColor"].get<std::string>());
        }
        if (sampler.contains("MinLOD")) {
            if (sampler["MinLOD"].is_string()) {
                samplerDesc.MinLOD = std::get<float>(kDefineMap.at(sampler["MinLOD"].get<std::string>()));
            } else if (sampler["MinLOD"].is_number()) {
                samplerDesc.MinLOD = sampler["MinLOD"].get<float>();
            }
        }
        if (sampler.contains("MaxLOD")) {
            if (sampler["MaxLOD"].is_string()) {
                samplerDesc.MaxLOD = std::get<float>(kDefineMap.at(sampler["MaxLOD"].get<std::string>()));
            } else if (sampler["MaxLOD"].is_number()) {
                samplerDesc.MaxLOD = sampler["MaxLOD"].get<float>();
            }
        }
        if (sampler.contains("ShaderRegister")) {
            samplerDesc.ShaderRegister = sampler["ShaderRegister"].get<UINT>();
        }
        if (sampler.contains("RegisterSpace")) {
            samplerDesc.RegisterSpace = sampler["RegisterSpace"].get<UINT>();
        }
        if (sampler.contains("ShaderVisibility")) {
            samplerDesc.ShaderVisibility = kShaderVisibilityMap.at(sampler["ShaderVisibility"].get<std::string>());
        }
        samplers.push_back(samplerDesc);
    }

    return samplers;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
