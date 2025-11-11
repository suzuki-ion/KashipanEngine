#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_ROOT_DESCRIPTOR ParseRootDescriptor(const Json &json) {
    D3D12_ROOT_DESCRIPTOR descriptor{};

    if (json.contains("ShaderRegister")) {
        descriptor.ShaderRegister = json["ShaderRegister"].get<UINT>();
    }
    if (json.contains("RegisterSpace")) {
        descriptor.RegisterSpace = json["RegisterSpace"].get<UINT>();
    }

    return descriptor;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
