#pragma once
#include <d3d12.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/EnumMaps.h"
#include "Graphics/Pipeline/DefineMaps.h"

namespace KashipanEngine {
namespace Pipeline::JsonParser {

inline std::vector<D3D12_INPUT_ELEMENT_DESC> ParseInputLayout(const Json &json) {
    using namespace KashipanEngine::Pipeline::EnumMaps;
    using namespace KashipanEngine::Pipeline::DefineMaps;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    if (!json.contains("Elements")) return inputLayout;

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
        inputLayout.push_back(inputElement);
    }

    return inputLayout;
}

inline D3D12_INPUT_LAYOUT_DESC AsInputLayoutDesc(const std::vector<D3D12_INPUT_ELEMENT_DESC> &elements) {
    D3D12_INPUT_LAYOUT_DESC desc{};
    if (!elements.empty()) {
        desc.NumElements = static_cast<UINT>(elements.size());
        desc.pInputElementDescs = elements.data();
    }
    return desc;
}

} // namespace Pipeline::JsonParser
} // namespace KashipanEngine
