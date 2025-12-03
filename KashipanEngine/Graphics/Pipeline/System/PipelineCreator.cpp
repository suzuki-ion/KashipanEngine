#include "PipelineCreator.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"
#include "Graphics/Pipeline/JsonParser/BlendState.h"
#include "Graphics/Pipeline/JsonParser/RasterizerState.h"
#include "Graphics/Pipeline/JsonParser/DepthStencilState.h"
#include "Graphics/Pipeline/JsonParser/InputLayout.h"
#include "Graphics/Pipeline/JsonParser/GraphicsPipelineState.h"
#include "Graphics/Pipeline/JsonParser/ComputePipelineState.h"
#include "Graphics/Pipeline/JsonParser/Shader.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include <sstream>

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
static std::string HrToHex(HRESULT hr) {
    std::stringstream ss; ss << "0x" << std::hex << std::uppercase << static_cast<unsigned int>(hr);
    return ss.str();
}
} // namespace

bool PipelineCreator::CreateRender(const Json &json, PipelineInfo &outInfo) {
    using namespace Pipeline::JsonParser;
    std::string name = json.value("Name", std::string{});
    if (name.empty()) {
        Log(Translation("engine.graphics.pipeline.load.render.missingname"), LogSeverity::Error);
        return false;
    }

    ParsedShadersInfo parsedShaders; // 先に Shader を読む必要がある場合がある
    if (json.contains("Shader")) {
        parsedShaders = ParseShader(json["Shader"]);
    } else {
        Log(Translation("engine.graphics.pipeline.load.shader.missing") + name, LogSeverity::Error);
        return false;
    }

    const D3D12_ROOT_SIGNATURE_DESC* pRootSigDesc = nullptr;
    std::optional<RootSignatureParsed> ownedRootSigParsed;
    D3D12_ROOT_SIGNATURE_DESC presetCopy{};

    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (!components_->HasRootSignature(presetName)) {
                Log(Translation("engine.graphics.pipeline.rootsignature.preset.notfound") + presetName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
                return false;
            }
            presetCopy = components_->GetRootSignature(presetName);
            pRootSigDesc = &presetCopy;
        } else {
            ownedRootSigParsed = ParseRootSignature(rootJson);
            pRootSigDesc = &ownedRootSigParsed->desc;
        }
    } else if (parsedShaders.isAutoRootDescriptorFromShader) {
        // リフレクション情報から自動ルートシグネチャ生成（簡易版）
        ownedRootSigParsed.emplace();
        ownedRootSigParsed->desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // 収集用
        struct RangeBuildKey { D3D12_DESCRIPTOR_RANGE_TYPE type; UINT space; };
        std::vector<RangeBuildKey> keys;
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> tempRanges; // SRV/UAV/Sampler用
        // CBVのRootDescriptor格納用（root parametersへの直接登録）
        struct CbvRootDesc { UINT shaderRegister; UINT space; };
        std::vector<CbvRootDesc> cbvRootDescriptors;

        auto getRangeType = [](D3D_SHADER_INPUT_TYPE t)->D3D12_DESCRIPTOR_RANGE_TYPE {
            switch (t) {
                case D3D_SIT_TEXTURE:
                case D3D_SIT_STRUCTURED:
                case D3D_SIT_BYTEADDRESS:
                case D3D_SIT_RTACCELERATIONSTRUCTURE:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                case D3D_SIT_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                case D3D_SIT_UAV_RWTYPED:
                case D3D_SIT_UAV_RWSTRUCTURED:
                case D3D_SIT_UAV_RWBYTEADDRESS:
                case D3D_SIT_UAV_APPEND_STRUCTURED:
                case D3D_SIT_UAV_CONSUME_STRUCTURED:
                case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                default: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            }
            };

        // リソース収集
        for (auto &stage : parsedShaders.stages) {
            ShaderCompiler::ShaderCompiledInfo *compiled = nullptr;
            if (stage.isUsePreset) {
                if (components_->HasCompiledShader(stage.presetName)) compiled = components_->GetCompiledShader(stage.presetName); else continue;
            } else {
                compiled = shaderCompiler_->CompileShader(stage.compileInfo);
                if (compiled) components_->RegisterCompiledShader(stage.compileInfo.name, compiled);
            }
            if (!compiled) continue;

            for (const auto &kv : compiled->GetReflectionInfo().ResourceBindings()) {
                const auto &rb = kv.second;
                if (rb.Type() == D3D_SIT_CBUFFER) {
                    // CBV は RootDescriptor として追加
                    cbvRootDescriptors.push_back({ rb.BindPoint(), rb.Space() });
                    continue;
                }
                // SRV/UAV/Sampler は DescriptorTable にまとめる
                D3D12_DESCRIPTOR_RANGE_TYPE rtype = getRangeType(rb.Type());
                RangeBuildKey key{ rtype, rb.Space() };
                size_t idx = 0; bool found = false;
                for (; idx < keys.size(); ++idx) {
                    if (keys[idx].type == key.type && keys[idx].space == key.space) { found = true; break; }
                }
                if (!found) { keys.push_back(key); tempRanges.emplace_back(); idx = keys.size() - 1; }

                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = key.type;
                range.NumDescriptors = rb.BindCount() == 0 ? 1u : rb.BindCount();
                range.BaseShaderRegister = rb.BindPoint();
                range.RegisterSpace = rb.Space();
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                tempRanges[idx].push_back(range);
            }
        }

        // SRV/UAV の連番統合
        for (auto &ranges : tempRanges) {
            std::sort(ranges.begin(), ranges.end(), [](auto &a, auto &b) { return a.BaseShaderRegister < b.BaseShaderRegister; });
            std::vector<D3D12_DESCRIPTOR_RANGE> merged;
            for (size_t i = 0; i < ranges.size();) {
                D3D12_DESCRIPTOR_RANGE cur = ranges[i];
                ++i;
                while (i < ranges.size()) {
                    auto &n = ranges[i];
                    if (n.BaseShaderRegister == cur.BaseShaderRegister + cur.NumDescriptors &&
                        n.RegisterSpace == cur.RegisterSpace && n.RangeType == cur.RangeType) {
                        cur.NumDescriptors += n.NumDescriptors;
                        ++i;
                    } else break;
                }
                merged.push_back(cur);
            }
            ranges = std::move(merged);
        }

        // RootParameter 構築: まず CBV RootDescriptor
        for (const auto &cbv : cbvRootDescriptors) {
            D3D12_ROOT_PARAMETER param{};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.Descriptor.ShaderRegister = cbv.shaderRegister;
            param.Descriptor.RegisterSpace = cbv.space;
            ownedRootSigParsed->rootParams.parameters.push_back(param);
        }

        // 次に SRV/UAV/Sampler の DescriptorTable
        for (size_t i = 0; i < keys.size(); ++i) {
            if (tempRanges[i].empty()) continue;
            D3D12_ROOT_PARAMETER param{};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            ownedRootSigParsed->rootParams.rangesStorage.push_back(tempRanges[i]);
            auto &storageRef = ownedRootSigParsed->rootParams.rangesStorage.back();
            param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(storageRef.size());
            param.DescriptorTable.pDescriptorRanges = storageRef.data();
            ownedRootSigParsed->rootParams.parameters.push_back(param);
        }

        // desc の最終設定
        ownedRootSigParsed->desc.NumParameters = static_cast<UINT>(ownedRootSigParsed->rootParams.parameters.size());
        ownedRootSigParsed->desc.pParameters = ownedRootSigParsed->rootParams.parameters.empty() ? nullptr : ownedRootSigParsed->rootParams.parameters.data();
        ownedRootSigParsed->desc.NumStaticSamplers = 0;
        ownedRootSigParsed->desc.pStaticSamplers = nullptr;

        pRootSigDesc = &ownedRootSigParsed->desc;
        outInfo.autoRootDescriptorGenerated = true;
    } else {
        Log(Translation("engine.graphics.pipeline.load.render.rootsignature.missing") + name, LogSeverity::Error);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3D12SerializeRootSignature(pRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf()))) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        Log(Translation("engine.graphics.pipeline.rootsignature.serialize.failed") + name, LogSeverity::Error);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    {
        HRESULT hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr)) {
            Log(Translation("engine.graphics.pipeline.rootsignature.create.failed") + name + " " + Translation("error.code.label") + HrToHex(hr), LogSeverity::Error);
            return false;
        }
    }

    // Shaders コンパイルは parsedShaders から再利用（上で一部コンパイル済)
    ShaderCompiler::ShaderCompiledInfo *vs = nullptr, *ps = nullptr, *gs = nullptr, *hs = nullptr, *ds = nullptr;
    std::vector<std::pair<ShaderCompiler::ShaderCompiledInfo*, std::string>> shadersWithStages;
    for (auto &stageInfo : parsedShaders.stages) {
        ShaderCompiler::ShaderCompiledInfo *compiled = nullptr;
        if (stageInfo.isUsePreset) {
            if (components_->HasCompiledShader(stageInfo.presetName)) compiled = components_->GetCompiledShader(stageInfo.presetName);
            else Log(Translation("engine.graphics.pipeline.shader.preset.notfound") + stageInfo.presetName + " " + Translation("engine.graphics.pipeline.stage.label") + stageInfo.stageName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
        } else {
            if (components_->HasCompiledShader(stageInfo.compileInfo.name)) compiled = components_->GetCompiledShader(stageInfo.compileInfo.name); else {
                compiled = shaderCompiler_->CompileShader(stageInfo.compileInfo);
                if (compiled) components_->RegisterCompiledShader(stageInfo.compileInfo.name, compiled);
                else Log(Translation("engine.graphics.pipeline.shader.compile.failed") + stageInfo.compileInfo.filePath + " " + Translation("engine.graphics.pipeline.stage.label") + stageInfo.stageName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
            }
        }
        shadersWithStages.emplace_back(compiled, stageInfo.stageName);
        if (stageInfo.stageName == "Vertex") vs = compiled;
        else if (stageInfo.stageName == "Pixel") ps = compiled;
        else if (stageInfo.stageName == "Geometry") gs = compiled;
        else if (stageInfo.stageName == "Hull") hs = compiled;
        else if (stageInfo.stageName == "Domain") ds = compiled;
    }
    if (!vs) {
        Log(Translation("engine.graphics.pipeline.load.render.vertex.missing") + name, LogSeverity::Error);
        return false;
    }

    // Input layout
    InputLayoutParsedInfo inputLayoutInfo{};
    if (json.contains("InputLayout")) {
        const auto &ilJson = json["InputLayout"];
        if (ilJson.contains("UsePreset")) {
            auto presetName = ilJson["UsePreset"].get<std::string>();
            if (components_->HasInputLayout(presetName)) inputLayoutInfo = components_->GetInputLayout(presetName);
            else Log(Translation("engine.graphics.pipeline.inputlayout.preset.notfound") + presetName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
        } else inputLayoutInfo = ParseInputLayout(ilJson);
    }
    if ((inputLayoutInfo.isAutoFromShader || parsedShaders.isAutoInputLayoutFromVS) && inputLayoutInfo.elements.empty() && vs) {
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
    // 事前検証: VS が頂点入力を要求しているのに IL が空の場合は詳細を出力
    {
        const auto &refl = vs->GetReflectionInfo();
        if (refl.InputParameters().size() > 0 && inputLayoutDesc.NumElements == 0) {
            Log(Translation("engine.graphics.pipeline.inputlayout.empty.withvsinputs") + std::to_string(refl.InputParameters().size()) + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
        }
    }

    // Fixed states
    if (!json.contains("RasterizerState")) { Log(Translation("engine.graphics.pipeline.load.render.rasterizer.missing") + name, LogSeverity::Error); return false; }
    if (!json.contains("BlendState")) { Log(Translation("engine.graphics.pipeline.load.render.blend.missing") + name, LogSeverity::Error); return false; }
    if (!json.contains("DepthStencilState")) { Log(Translation("engine.graphics.pipeline.load.render.depthstencil.missing") + name, LogSeverity::Error); return false; }
    if (!json.contains("PipelineState")) { Log(Translation("engine.graphics.pipeline.load.render.pso.missing") + name, LogSeverity::Error); return false; }

    D3D12_RASTERIZER_DESC rasterDesc = json["RasterizerState"].contains("UsePreset") ? components_->GetRasterizerState(json["RasterizerState"]["UsePreset"].get<std::string>()) : ParseRasterizerState(json["RasterizerState"]);
    D3D12_BLEND_DESC blendDesc = json["BlendState"].contains("UsePreset") ? components_->GetBlendState(json["BlendState"]["UsePreset"].get<std::string>()) : ParseBlendState(json["BlendState"]);
    D3D12_DEPTH_STENCIL_DESC depthDesc = json["DepthStencilState"].contains("UsePreset") ? components_->GetDepthStencilState(json["DepthStencilState"]["UsePreset"].get<std::string>()) : ParseDepthStencilState(json["DepthStencilState"]);
    GraphicsPipelineStateParsedInfo gpsInfo = json["PipelineState"].contains("UsePreset") ? components_->GetGraphicsPipelineState(json["PipelineState"]["UsePreset"].get<std::string>()) : ParseGraphicsPipelineState(json["PipelineState"]);

    if (gpsInfo.desc.PrimitiveTopologyType == D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED && parsedShaders.isAutoTopologyFromShaders) {
        if (std::any_of(parsedShaders.stages.begin(), parsedShaders.stages.end(), [](auto &s){ return s.stageName == "Hull"; }) &&
            std::any_of(parsedShaders.stages.begin(), parsedShaders.stages.end(), [](auto &s){ return s.stageName == "Domain"; })) {
            gpsInfo.desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        }
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
    {
        HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        if (FAILED(hr)) {
            Log(Translation("engine.graphics.pipeline.load.render.pso.create.failed") + name + " " + Translation("error.code.label") + HrToHex(hr), LogSeverity::Error);
            return false;
        }
    }

    outInfo.name = name;
    outInfo.type = PipelineType::Render;
    outInfo.topologyType = ToD3DTopology(psoDesc.PrimitiveTopologyType);
    outInfo.pipelineSet.rootSignature = rootSignature;
    outInfo.pipelineSet.pipelineState = pso;
    outInfo.shaders.clear();
    if (vs) outInfo.shaders.push_back(vs);
    if (ps) outInfo.shaders.push_back(ps);
    if (gs) outInfo.shaders.push_back(gs);
    if (hs) outInfo.shaders.push_back(hs);
    if (ds) outInfo.shaders.push_back(ds);
    BuildShaderVariableBinder(outInfo, shadersWithStages, ownedRootSigParsed);
    return true;
}

bool PipelineCreator::CreateCompute(const Json &json, PipelineInfo &outInfo) {
    using namespace Pipeline::JsonParser;
    std::string name = json.value("Name", std::string{});
    if (name.empty()) {
        Log(Translation("engine.graphics.pipeline.load.compute.missingname"), LogSeverity::Error);
        return false;
    }
    const D3D12_ROOT_SIGNATURE_DESC* pRootSigDesc = nullptr;
    std::optional<RootSignatureParsed> ownedRootSigParsed;
    D3D12_ROOT_SIGNATURE_DESC presetCopy{};
    if (json.contains("RootSignature")) {
        auto rootJson = json["RootSignature"];
        if (rootJson.contains("UsePreset")) {
            const auto presetName = rootJson["UsePreset"].get<std::string>();
            if (!components_->HasRootSignature(presetName)) {
                Log(Translation("engine.graphics.pipeline.rootsignature.preset.notfound") + presetName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
                return false;
            }
            presetCopy = components_->GetRootSignature(presetName);
            pRootSigDesc = &presetCopy;
        } else {
            ownedRootSigParsed = ParseRootSignature(rootJson);
            pRootSigDesc = &ownedRootSigParsed->desc;
        }
    } else {
        Log(Translation("engine.graphics.pipeline.load.compute.rootsignature.missing") + name, LogSeverity::Error);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3D12SerializeRootSignature(pRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf()))) {
        if (errorBlob) Log(std::string(static_cast<const char*>(errorBlob->GetBufferPointer())), LogSeverity::Error);
        Log(Translation("engine.graphics.pipeline.rootsignature.serialize.failed") + name, LogSeverity::Error);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    {
        HRESULT hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr)) {
            Log(Translation("engine.graphics.pipeline.rootsignature.create.failed") + name + " " + Translation("error.code.label") + HrToHex(hr), LogSeverity::Error);
            return false;
        }
    }
    ShaderCompiler::ShaderCompiledInfo *cs = nullptr;
    if (json.contains("Shader")) {
        ParsedShadersInfo parsedShaders = ParseShader(json["Shader"]);
        std::vector<std::pair<ShaderCompiler::ShaderCompiledInfo*, std::string>> shadersWithStages;
        for (auto &stageInfo : parsedShaders.stages) {
            if (stageInfo.stageName != "Compute" && parsedShaders.isGroup) continue;
            ShaderCompiler::ShaderCompiledInfo *compiled = nullptr;
            if (stageInfo.isUsePreset) {
                if (components_->HasCompiledShader(stageInfo.presetName)) compiled = components_->GetCompiledShader(stageInfo.presetName);
                else Log(Translation("engine.graphics.pipeline.shader.preset.notfound") + stageInfo.presetName + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
            } else {
                compiled = shaderCompiler_->CompileShader(stageInfo.compileInfo);
                if (compiled) components_->RegisterCompiledShader(stageInfo.compileInfo.name, compiled);
                else Log(Translation("engine.graphics.pipeline.shader.compile.failed") + stageInfo.compileInfo.filePath + " " + Translation("engine.graphics.pipeline.name.label") + name, LogSeverity::Error);
            }
            if (compiled) cs = compiled;
            shadersWithStages.emplace_back(compiled, stageInfo.stageName);
        }
        outInfo.shaders.clear();
        for (const auto &p : shadersWithStages) if (p.first) outInfo.shaders.push_back(p.first);
        BuildShaderVariableBinder(outInfo, shadersWithStages, ownedRootSigParsed);
    } else {
        Log(Translation("engine.graphics.pipeline.load.shader.missing") + name, LogSeverity::Error);
        return false;
    }
    if (!cs) {
        Log(Translation("engine.graphics.pipeline.load.compute.shader.missing") + name, LogSeverity::Error);
        return false;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    if (json.contains("PipelineState")) {
        const auto &pj = json["PipelineState"];
        if (pj.contains("UsePreset")) computeDesc = components_->GetComputePipelineState(pj["UsePreset"].get<std::string>()); else computeDesc = ParseComputePipelineState(pj);
    } else {
        Log(Translation("engine.graphics.pipeline.load.compute.pso.missing") + name, LogSeverity::Error);
        return false;
    }
    computeDesc.pRootSignature = rootSignature.Get();
    computeDesc.CS = { cs->GetBytecodePtr(), cs->GetBytecodeSize() };
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    {
        HRESULT hr = device_->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pso));
        if (FAILED(hr)) {
            Log(Translation("engine.graphics.pipeline.load.compute.pso.create.failed") + name + " " + Translation("error.code.label") + HrToHex(hr), LogSeverity::Error);
            return false;
        }
    }
    outInfo.name = name;
    outInfo.type = PipelineType::Compute;
    outInfo.topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    outInfo.pipelineSet.rootSignature = rootSignature;
    outInfo.pipelineSet.pipelineState = pso;
    return true;
}

void PipelineCreator::BuildShaderVariableBinder(PipelineInfo &outInfo, const std::vector<std::pair<ShaderCompiler::ShaderCompiledInfo*, std::string>> &shadersWithStages, std::optional<Pipeline::JsonParser::RootSignatureParsed> customRootSig) {
    MyStd::NameMap<ShaderVariableBinding> nameMap;
    for (const auto &entry : shadersWithStages) {
        ShaderCompiler::ShaderCompiledInfo *shader = entry.first;
        const std::string &stageName = entry.second;
        if (!shader) continue;
        auto single = CreateShaderVariableMap(*shader, false);
        std::string prefix = stageName;
        if (!prefix.empty()) prefix += ":";
        for (auto it = single.begin(); it != single.end(); ++it) {
            std::string namespacedName = prefix + it->key;
            if (!nameMap.Contains(namespacedName)) {
                nameMap.Set(namespacedName, it->value);
            }
        }
    }
    outInfo.variableBinder.SetNameMap(nameMap);
    const auto &rootSigParsed = customRootSig ? *customRootSig : Pipeline::JsonParser::RootSignatureParsed{};
    for (size_t i = 0; i < rootSigParsed.rootParams.parameters.size(); ++i) {
        const auto &param = rootSigParsed.rootParams.parameters[i];
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            for (const auto &range : rootSigParsed.rootParams.rangesStorage[i]) {
                D3D_SHADER_INPUT_TYPE type =
                    range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV ? D3D_SIT_CBUFFER :
                    range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? D3D_SIT_TEXTURE :
                    range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV ? D3D_SIT_UAV_RWTYPED :
                    D3D_SIT_SAMPLER;
                outInfo.variableBinder.RegisterDescriptorTableRange({},
                    type,
                    range.BaseShaderRegister,
                    range.RegisterSpace,
                    range.NumDescriptors,
                    static_cast<UINT>(i)
                );
            }
        } else if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ||
            param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ||
            param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV) {
            outInfo.variableBinder.RegisterRootDescriptor({},
                param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV ? D3D_SIT_CBUFFER :
                param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV ? D3D_SIT_TEXTURE :
                D3D_SIT_UAV_RWTYPED,
                param.Descriptor.ShaderRegister,
                param.Descriptor.RegisterSpace,
                static_cast<UINT>(i)
            );
        }
    }
}

} // namespace KashipanEngine
