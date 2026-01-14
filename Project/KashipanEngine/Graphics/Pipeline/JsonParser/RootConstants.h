#pragma once
#include <d3d12.h>
#include <string>
#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline D3D12_ROOT_CONSTANTS ParseRootConstants(const Json &json) {
    LogScope scope;
    const std::string presetName = json.contains("Name") ? json["Name"].get<std::string>() : std::string{};
    Log(Translation("engine.graphics.pipeline.jsonparser.rootconstants.parse.start") + presetName, LogSeverity::Debug);

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

    Log(Translation("engine.graphics.pipeline.jsonparser.rootconstants.parse.end") + presetName, LogSeverity::Debug);
    return constants;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
