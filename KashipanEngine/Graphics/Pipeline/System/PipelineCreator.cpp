#include "PipelineCreator.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"
#include "Graphics/Pipeline/JsonParser/BlendState.h"
#include "Graphics/Pipeline/JsonParser/RasterizerState.h"
#include "Graphics/Pipeline/JsonParser/DepthStencilState.h"
#include "Graphics/Pipeline/JsonParser/InputLayout.h"
#include "Graphics/Pipeline/JsonParser/GraphicsPipelineState.h"
#include "Graphics/Pipeline/JsonParser/ComputePipelineState.h"
#include "Graphics/Pipeline/JsonParser/Shader.h"

namespace KashipanEngine {
namespace {
D3D12_PRIMITIVE_TOPOLOGY ToD3DTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
    switch (type) {
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
        default: return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}
DXGI_FORMAT InferFormatFromMaskAndType(BYTE mask, D3D_REGISTER_COMPONENT_TYPE compType) {
    int compCount = 0;
    if (mask & 0x1) ++compCount;
    if (mask & 0x2) ++compCount;
    if (mask & 0x4) ++compCount;
    if (mask & 0x8) ++compCount;
    if (compCount <= 0) compCount = 1;
    switch (compType) {
        case D3D_REGISTER_COMPONENT_UINT32:
        switch (compCount) { case 1: return DXGI_FORMAT_R32_UINT; case 2: return DXGI_FORMAT_R32G32_UINT; case 3: return DXGI_FORMAT_R32G32B32_UINT; default: return DXGI_FORMAT_R32G32B32A32_UINT; }
        case D3D_REGISTER_COMPONENT_SINT32:
        switch (compCount) { case 1: return DXGI_FORMAT_R32_SINT; case 2: return DXGI_FORMAT_R32G32_SINT; case 3: return DXGI_FORMAT_R32G32B32_SINT; default: return DXGI_FORMAT_R32G32B32A32_SINT; }
        case D3D_REGISTER_COMPONENT_FLOAT32:
        default:
        switch (compCount) { case 1: return DXGI_FORMAT_R32_FLOAT; case 2: return DXGI_FORMAT_R32G32_FLOAT; case 3: return DXGI_FORMAT_R32G32B32_FLOAT; default: return DXGI_FORMAT_R32G32B32A32_FLOAT; }
    }
}
} // namespace

bool PipelineCreator::CreateRender(const Json &json, PipelineInfo &outInfo) {
    using namespace Pipeline::JsonParser;
    std::string name = json.value("Name", std::string{});
    if (name.empty()) return false;

    // Root signature
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (components_->HasRootSignature(presetName)) rootSigDesc = components_->GetRootSignature(presetName);
        } else {
            auto parsed = ParseRootSignature(rootJson);
            rootSigDesc = parsed.desc;
        }
    } else return false;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf()))) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    if (FAILED(device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) return false;

    // Shaders
    ShaderCompiler::ShaderCompiledInfo *vs = nullptr, *ps = nullptr, *gs = nullptr, *hs = nullptr, *ds = nullptr;
    if (!json.contains("Shader")) return false;
    ShaderGroupParsedInfo shaderGroupInfo = ParseShaderGroupInfo(name, json["Shader"]);
    for (auto &p : shaderGroupInfo.stages) {
        const std::string &stage = p.first;
        const auto &entry = p.second;
        ShaderCompiler::ShaderCompiledInfo *compiled = nullptr;
        if (entry.isUsePreset) {
            if (components_->HasCompiledShader(entry.presetName)) compiled = components_->GetCompiledShader(entry.presetName);
        } else {
            compiled = shaderCompiler_->CompileShader(entry.compileInfo);
            components_->RegisterCompiledShader(entry.compileInfo.name, compiled);
        }
        if (stage == "Vertex") vs = compiled;
        else if (stage == "Pixel") ps = compiled;
        else if (stage == "Geometry") gs = compiled;
        else if (stage == "Hull") hs = compiled;
        else if (stage == "Domain") ds = compiled;
    }
    if (!vs) return false;

    // Input layout
    InputLayoutParsedInfo inputLayoutInfo{};
    if (json.contains("InputLayout")) {
        const auto &ilJson = json["InputLayout"];
        if (ilJson.contains("UsePreset")) {
            auto presetName = ilJson["UsePreset"].get<std::string>();
            if (components_->HasInputLayout(presetName)) inputLayoutInfo = components_->GetInputLayout(presetName);
        } else inputLayoutInfo = ParseInputLayout(ilJson);
    }
    if ((inputLayoutInfo.isAutoFromShader || shaderGroupInfo.isAutoInputLayoutFromVS) && inputLayoutInfo.elements.empty() && vs) {
        const auto &refl = vs->GetReflectionInfo();
        for (const auto &in : refl.InputParameters()) {
            D3D12_INPUT_ELEMENT_DESC e{};
            e.SemanticName = _strdup(in.SemanticName().c_str());
            e.SemanticIndex = in.SemanticIndex();
            e.Format = InferFormatFromMaskAndType(in.UsageMask(), in.ComponentType());
            e.InputSlot = 0;
            e.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
            e.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            e.InstanceDataStepRate = 0;
            inputLayoutInfo.elements.push_back(e);
        }
    }
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = AsInputLayoutDesc(inputLayoutInfo);

    // Fixed states
    if (!json.contains("RasterizerState") || !json.contains("BlendState") || !json.contains("DepthStencilState") || !json.contains("PipelineState")) return false;
    D3D12_RASTERIZER_DESC rasterDesc = json["RasterizerState"].contains("UsePreset") ? components_->GetRasterizerState(json["RasterizerState"]["UsePreset"].get<std::string>()) : ParseRasterizerState(json["RasterizerState"]);
    D3D12_BLEND_DESC blendDesc = json["BlendState"].contains("UsePreset") ? components_->GetBlendState(json["BlendState"]["UsePreset"].get<std::string>()) : ParseBlendState(json["BlendState"]);
    D3D12_DEPTH_STENCIL_DESC depthDesc = json["DepthStencilState"].contains("UsePreset") ? components_->GetDepthStencilState(json["DepthStencilState"]["UsePreset"].get<std::string>()) : ParseDepthStencilState(json["DepthStencilState"]);
    GraphicsPipelineStateParsedInfo gpsInfo = json["PipelineState"].contains("UsePreset") ? components_->GetGraphicsPipelineState(json["PipelineState"]["UsePreset"].get<std::string>()) : ParseGraphicsPipelineState(json["PipelineState"]);

    if (gpsInfo.desc.PrimitiveTopologyType == D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED && shaderGroupInfo.isAutoTopologyFromShaders) {
        if (hs && ds) gpsInfo.desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = gpsInfo.desc;
    psoDesc.pRootSignature = rootSignature.Get();
    if (vs) psoDesc.VS = { vs->GetBytecodePtr(), vs->GetBytecodeSize() };
    if (ps) psoDesc.PS = { ps->GetBytecodePtr(), ps->GetBytecodeSize() };
    if (gs) psoDesc.GS = { gs->GetBytecodePtr(), gs->GetBytecodeSize() };
    if (hs) psoDesc.HS = { hs->GetBytecodePtr(), hs->GetBytecodeSize() };
    if (ds) psoDesc.DS = { ds->GetBytecodePtr(), ds->GetBytecodeSize() };
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthDesc;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    if (FAILED(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)))) return false;

    outInfo.name = name;
    outInfo.type = PipelineType::Render;
    outInfo.topologyType = ToD3DTopology(psoDesc.PrimitiveTopologyType);
    outInfo.pipelineSet.rootSignature = rootSignature;
    outInfo.pipelineSet.pipelineState = pso;
    return true;
}

bool PipelineCreator::CreateCompute(const Json &json, PipelineInfo &outInfo) {
    using namespace Pipeline::JsonParser;
    std::string name = json.value("Name", std::string{});
    if (name.empty()) return false;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (components_->HasRootSignature(presetName)) rootSigDesc = components_->GetRootSignature(presetName);
        } else {
            auto parsed = ParseRootSignature(rootJson);
            rootSigDesc = parsed.desc;
        }
    } else return false;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf()))) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    if (FAILED(device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) return false;

    ShaderCompiler::ShaderCompiledInfo *cs = nullptr;
    if (json.contains("Shader")) {
        auto entry = ParseShaderEntry(name, "Compute", json["Shader"]);
        if (entry.isUsePreset) {
            if (components_->HasCompiledShader(entry.presetName)) cs = components_->GetCompiledShader(entry.presetName);
        } else {
            cs = shaderCompiler_->CompileShader(entry.compileInfo);
            components_->RegisterCompiledShader(entry.compileInfo.name, cs);
        }
    }
    if (!cs) return false;

    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    if (json.contains("PipelineState")) {
        const auto &pj = json["PipelineState"];
        if (pj.contains("UsePreset")) computeDesc = components_->GetComputePipelineState(pj["UsePreset"].get<std::string>()); else computeDesc = ParseComputePipelineState(pj);
    } else return false;

    computeDesc.pRootSignature = rootSignature.Get();
    computeDesc.CS = { cs->GetBytecodePtr(), cs->GetBytecodeSize() };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    if (FAILED(device_->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pso)))) return false;

    outInfo.name = name;
    outInfo.type = PipelineType::Compute;
    outInfo.topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    outInfo.pipelineSet.rootSignature = rootSignature;
    outInfo.pipelineSet.pipelineState = pso;
    return true;
}

} // namespace KashipanEngine
