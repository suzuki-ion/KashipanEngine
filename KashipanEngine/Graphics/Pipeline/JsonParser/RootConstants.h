#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_ROOT_CONSTANTS ParseRootConstants(const Json &json) {
    D3D12_ROOT_CONSTANTS constants{};

    if (json.contains("ShaderRegister")) {
        constants.ShaderRegister = json["ShaderRegister"].get<UINT>();
    }
    if (json.contains("RegisterSpace")) {
        constants.RegisterSpace = json["RegisterSpace"].get<UINT>();
    }
    if (json.contains("Num32BitValues")) {
        constants.Num32BitValues = json["Num32BitValues"].get<UINT>();
    }

    return constants;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
