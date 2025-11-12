#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

struct InputLayoutParsedInfo {
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    bool isAutoFromShader = false;  // VertexShaderから自動生成するかどうか
    bool hasElements = false;       // JSONでElementsが指定されているかどうか
};

inline InputLayoutParsedInfo ParseInputLayout(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    InputLayoutParsedInfo info{};

    if (json.contains("AutoFromShader")) {
        info.isAutoFromShader = json["AutoFromShader"].get<bool>();
    } else if (json.contains("AutoFromVS")) { // alias
        info.isAutoFromShader = json["AutoFromVS"].get<bool>();
    }

    if (!json.contains("Elements")) return info;

    info.hasElements = true;
    for (const auto &element : json["Elements"]) {
        D3D12_INPUT_ELEMENT_DESC inputElement{};
        if (element.contains("SemanticName")) {
            inputElement.SemanticName = _strdup(element["SemanticName"].get<std::string>().c_str());
        }
        if (element.contains("SemanticIndex")) {
            inputElement.SemanticIndex = element["SemanticIndex"].get<UINT>();
        }
        if (element.contains("Format")) {
            inputElement.Format = kDxgiFormatMap.at(element["Format"].get<std::string>());
        }
        if (element.contains("InputSlot")) {
            inputElement.InputSlot = element["InputSlot"].get<UINT>();
        }
        if (element.contains("AlignedByteOffset")) {
            if (element["AlignedByteOffset"].is_string()) {
                inputElement.AlignedByteOffset = std::get<UINT>(kDefineMap.at(element["AlignedByteOffset"].get<std::string>()));
            } else if (element["AlignedByteOffset"].is_number()) {
                inputElement.AlignedByteOffset = element["AlignedByteOffset"].get<UINT>();
            }
        } else {
            inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        }
        if (element.contains("InputSlotClass")) {
            inputElement.InputSlotClass = kInputClassificationMap.at(element["InputSlotClass"].get<std::string>());
        }
        if (element.contains("InstanceDataStepRate")) {
            inputElement.InstanceDataStepRate = element["InstanceDataStepRate"].get<UINT>();
        }
        info.elements.push_back(inputElement);
    }

    return info;
}

inline D3D12_INPUT_LAYOUT_DESC AsInputLayoutDesc(const InputLayoutParsedInfo &info) {
    D3D12_INPUT_LAYOUT_DESC desc{};
    if (!info.elements.empty()) {
        desc.NumElements = static_cast<UINT>(info.elements.size());
        desc.pInputElementDescs = info.elements.data();
    }
    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
