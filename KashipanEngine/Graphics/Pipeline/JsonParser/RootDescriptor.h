#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_ROOT_DESCRIPTOR ParseRootDescriptor(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.rootdescriptor.parse.start") + presetName, LogSeverity::Debug);

    D3D12_ROOT_DESCRIPTOR descriptor{};

    if (json.contains("ShaderRegister")) {
        descriptor.ShaderRegister = json["ShaderRegister"].get<UINT>();
    }
    if (json.contains("RegisterSpace")) {
        descriptor.RegisterSpace = json["RegisterSpace"].get<UINT>();
    }

    Log(Translation("engine.graphics.pipeline.jsonparser.rootdescriptor.parse.end") + presetName, LogSeverity::Debug);
    return descriptor;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
