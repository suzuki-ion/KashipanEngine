#pragma once
#include <d3d12.h>
#include <string>
#include <VectorMap.h>
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/JsonParser/InputLayout.h"
#include "Graphics/Pipeline/JsonParser/GraphicsPipelineState.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"

namespace KashipanEngine {

class PipelineManager;

/// @brief パイプライン構成要素のプリセットを保持するコンテナ
class ComponentsPresetContainer final {
public:
    /// @brief PipelineManager専用コンストラクタ
    ComponentsPresetContainer(Passkey<PipelineManager>) {
        LogScope scope;
        Log(Translation("instance.created"), LogSeverity::Debug);
    }

    // RootSignature 保持用構造体（ポインタ寿命確保）
    struct RootSignatureStored {
        D3D12_ROOT_SIGNATURE_DESC desc{};               // 参照用
        std::vector<D3D12_ROOT_PARAMETER> parameters;  // 所有
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers; // 所有
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> rangesStorage; // 所有: 各パラメータの DescriptorTable 用
    };

    // 型エイリアス（構成要素のパース情報を保持）
    using RootSignaturePresets         = MyStd::VectorMap<std::string, RootSignatureStored>; // 変更
    using RootParameterPresets         = MyStd::VectorMap<std::string, std::vector<D3D12_ROOT_PARAMETER>>;
    using DescriptorRangePresets       = MyStd::VectorMap<std::string, std::vector<D3D12_DESCRIPTOR_RANGE>>;
    using RootConstantsPresets         = MyStd::VectorMap<std::string, D3D12_ROOT_CONSTANTS>;
    using RootDescriptorPresets        = MyStd::VectorMap<std::string, D3D12_ROOT_DESCRIPTOR>;
    using SamplerPresets               = MyStd::VectorMap<std::string, std::vector<D3D12_STATIC_SAMPLER_DESC>>;
    using InputLayoutPresets           = MyStd::VectorMap<std::string, Pipeline::JsonParser::InputLayoutParsedInfo>;
    using RasterizerStatePresets       = MyStd::VectorMap<std::string, D3D12_RASTERIZER_DESC>;
    using BlendStatePresets            = MyStd::VectorMap<std::string, D3D12_BLEND_DESC>;
    using DepthStencilStatePresets     = MyStd::VectorMap<std::string, D3D12_DEPTH_STENCIL_DESC>;
    using GraphicsPipelineStatePresets = MyStd::VectorMap<std::string, Pipeline::JsonParser::GraphicsPipelineStateParsedInfo>;
    using ComputePipelineStatePresets  = MyStd::VectorMap<std::string, D3D12_COMPUTE_PIPELINE_STATE_DESC>;
    using CompiledShaderPresets        = MyStd::VectorMap<std::string, ShaderCompiler::ShaderCompiledInfo*>;

    //--------- RootSignature ---------//
    void RegisterRootSignature(const std::string &name, const Pipeline::JsonParser::RootSignatureParsed &parsed) {
        // 一時オブジェクトにポインタを張ると push_back 時にコピーされ寿命が切れてしまうため
        // 先に値だけコピーし、コンテナ挿入後にポインタを再設定する。
        RootSignatureStored stored;
        stored.parameters = parsed.rootParams.parameters; // コピー
        stored.samplers   = parsed.samplers;              // コピー
        // rangesStorage をコピーしておく（DescriptorTable の pDescriptorRanges 用）
        stored.rangesStorage = parsed.rootParams.rangesStorage;
        stored.desc       = parsed.desc;                  // ここではポインタはまだ設定しない
        rootSignatures_.push_back(name, stored);          // コピー（VectorMap 内に確定）
        // コンテナ内オブジェクト参照を取得してポインタを正しく設定
        auto &storedRef = rootSignatures_.at(name).value;
        // 各パラメータ内の DescriptorTable.pDescriptorRanges を storedRef.rangesStorage に向ける
        // rangesStorage に格納されたエントリは DescriptorTable を持つパラメータ順に格納されている
        size_t rangeIdx = 0;
        for (size_t i = 0; i < storedRef.parameters.size(); ++i) {
            if (storedRef.parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
                if (rangeIdx < storedRef.rangesStorage.size()) {
                    auto &ranges = storedRef.rangesStorage[rangeIdx++];
                    storedRef.parameters[i].DescriptorTable.pDescriptorRanges = ranges.empty() ? nullptr : ranges.data();
                    storedRef.parameters[i].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
                } else {
                    storedRef.parameters[i].DescriptorTable.pDescriptorRanges = nullptr;
                    storedRef.parameters[i].DescriptorTable.NumDescriptorRanges = 0;
                }
            }
        }
        // desc のパラメータ/サンプラー配列も再設定
        storedRef.desc.pParameters      = storedRef.parameters.empty() ? nullptr : storedRef.parameters.data();
        storedRef.desc.NumParameters    = static_cast<UINT>(storedRef.parameters.size());
        storedRef.desc.pStaticSamplers  = storedRef.samplers.empty() ? nullptr : storedRef.samplers.data();
        storedRef.desc.NumStaticSamplers= static_cast<UINT>(storedRef.samplers.size());
    }
    bool RemoveRootSignature(const std::string &name) { return RemoveByKey(rootSignatures_, name); }
    [[nodiscard]] bool HasRootSignature(const std::string &name) const { return HasKey(rootSignatures_, name); }
    [[nodiscard]] const D3D12_ROOT_SIGNATURE_DESC &GetRootSignature(const std::string &name) const { return rootSignatures_.at(name).value.desc; }
    [[nodiscard]] const RootSignaturePresets &RootSignatures() const { return rootSignatures_; }

    //--------- RootParameter ---------//
    void RegisterRootParameter(const std::string &name, const std::vector<D3D12_ROOT_PARAMETER> &params) { rootParameters_.push_back(name, params); }
    bool RemoveRootParameter(const std::string &name) { return RemoveByKey(rootParameters_, name); }
    [[nodiscard]] bool HasRootParameter(const std::string &name) const { return HasKey(rootParameters_, name); }
    [[nodiscard]] const std::vector<D3D12_ROOT_PARAMETER> &GetRootParameter(const std::string &name) const { return rootParameters_.at(name).value; }
    [[nodiscard]] const RootParameterPresets &RootParameters() const { return rootParameters_; }

    //--------- DescriptorRange ---------//
    void RegisterDescriptorRange(const std::string &name, const std::vector<D3D12_DESCRIPTOR_RANGE> &ranges) { descriptorRanges_.push_back(name, ranges); }
    bool RemoveDescriptorRange(const std::string &name) { return RemoveByKey(descriptorRanges_, name); }
    [[nodiscard]] bool HasDescriptorRange(const std::string &name) const { return HasKey(descriptorRanges_, name); }
    [[nodiscard]] const std::vector<D3D12_DESCRIPTOR_RANGE> &GetDescriptorRange(const std::string &name) const { return descriptorRanges_.at(name).value; }
    [[nodiscard]] const DescriptorRangePresets &DescriptorRanges() const { return descriptorRanges_; }

    //--------- RootConstants ---------//
    void RegisterRootConstants(const std::string &name, const D3D12_ROOT_CONSTANTS &c) { rootConstants_.push_back(name, c); }
    bool RemoveRootConstants(const std::string &name) { return RemoveByKey(rootConstants_, name); }
    [[nodiscard]] bool HasRootConstants(const std::string &name) const { return HasKey(rootConstants_, name); }
    [[nodiscard]] const D3D12_ROOT_CONSTANTS &GetRootConstants(const std::string &name) const { return rootConstants_.at(name).value; }
    [[nodiscard]] const RootConstantsPresets &RootConstantsMap() const { return rootConstants_; }

    //--------- RootDescriptor ---------//
    void RegisterRootDescriptor(const std::string &name, const D3D12_ROOT_DESCRIPTOR &d) { rootDescriptors_.push_back(name, d); }
    bool RemoveRootDescriptor(const std::string &name) { return RemoveByKey(rootDescriptors_, name); }
    [[nodiscard]] bool HasRootDescriptor(const std::string &name) const { return HasKey(rootDescriptors_, name); }
    [[nodiscard]] const D3D12_ROOT_DESCRIPTOR &GetRootDescriptor(const std::string &name) const { return rootDescriptors_.at(name).value; }
    [[nodiscard]] const RootDescriptorPresets &RootDescriptors() const { return rootDescriptors_; }

    //--------- Sampler ---------//
    void RegisterSampler(const std::string &name, const std::vector<D3D12_STATIC_SAMPLER_DESC> &s) { samplers_.push_back(name, s); }
    bool RemoveSampler(const std::string &name) { return RemoveByKey(samplers_, name); }
    [[nodiscard]] bool HasSampler(const std::string &name) const { return HasKey(samplers_, name); }
    [[nodiscard]] const std::vector<D3D12_STATIC_SAMPLER_DESC> &GetSampler(const std::string &name) const { return samplers_.at(name).value; }
    [[nodiscard]] const SamplerPresets &Samplers() const { return samplers_; }

    //--------- InputLayout ---------//
    void RegisterInputLayout(const std::string &name, const Pipeline::JsonParser::InputLayoutParsedInfo &e) { inputLayouts_.push_back(name, e); }
    bool RemoveInputLayout(const std::string &name) { return RemoveByKey(inputLayouts_, name); }
    [[nodiscard]] bool HasInputLayout(const std::string &name) const { return HasKey(inputLayouts_, name); }
    [[nodiscard]] const Pipeline::JsonParser::InputLayoutParsedInfo &GetInputLayout(const std::string &name) const { return inputLayouts_.at(name).value; }
    [[nodiscard]] const InputLayoutPresets &InputLayouts() const { return inputLayouts_; }

    //--------- RasterizerState ---------//
    void RegisterRasterizerState(const std::string &name, const D3D12_RASTERIZER_DESC &d) { rasterizerStates_.push_back(name, d); }
    bool RemoveRasterizerState(const std::string &name) { return RemoveByKey(rasterizerStates_, name); }
    [[nodiscard]] bool HasRasterizerState(const std::string &name) const { return HasKey(rasterizerStates_, name); }
    [[nodiscard]] const D3D12_RASTERIZER_DESC &GetRasterizerState(const std::string &name) const { return rasterizerStates_.at(name).value; }
    [[nodiscard]] const RasterizerStatePresets &RasterizerStates() const { return rasterizerStates_; }

    //--------- BlendState ---------//
    void RegisterBlendState(const std::string &name, const D3D12_BLEND_DESC &d) { blendStates_.push_back(name, d); }
    bool RemoveBlendState(const std::string &name) { return RemoveByKey(blendStates_, name); }
    [[nodiscard]] bool HasBlendState(const std::string &name) const { return HasKey(blendStates_, name); }
    [[nodiscard]] const D3D12_BLEND_DESC &GetBlendState(const std::string &name) const { return blendStates_.at(name).value; }
    [[nodiscard]] const BlendStatePresets &BlendStates() const { return blendStates_; }

    //--------- DepthStencilState ---------//
    void RegisterDepthStencilState(const std::string &name, const D3D12_DEPTH_STENCIL_DESC &d) { depthStencilStates_.push_back(name, d); }
    bool RemoveDepthStencilState(const std::string &name) { return RemoveByKey(depthStencilStates_, name); }
    [[nodiscard]] bool HasDepthStencilState(const std::string &name) const { return HasKey(depthStencilStates_, name); }
    [[nodiscard]] const D3D12_DEPTH_STENCIL_DESC &GetDepthStencilState(const std::string &name) const { return depthStencilStates_.at(name).value; }
    [[nodiscard]] const DepthStencilStatePresets &DepthStencilStates() const { return depthStencilStates_; }

    //--------- GraphicsPipelineState ---------//
    void RegisterGraphicsPipelineState(const std::string &name, const Pipeline::JsonParser::GraphicsPipelineStateParsedInfo &d) { graphicsPipelineStates_.push_back(name, d); }
    bool RemoveGraphicsPipelineState(const std::string &name) { return RemoveByKey(graphicsPipelineStates_, name); }
    [[nodiscard]] bool HasGraphicsPipelineState(const std::string &name) const { return HasKey(graphicsPipelineStates_, name); }
    [[nodiscard]] const Pipeline::JsonParser::GraphicsPipelineStateParsedInfo &GetGraphicsPipelineState(const std::string &name) const { return graphicsPipelineStates_.at(name).value; }
    [[nodiscard]] const GraphicsPipelineStatePresets &GraphicsPipelineStates() const { return graphicsPipelineStates_; }

    //--------- ComputePipelineState ---------//
    void RegisterComputePipelineState(const std::string &name, const D3D12_COMPUTE_PIPELINE_STATE_DESC &d) { computePipelineStates_.push_back(name, d); }
    bool RemoveComputePipelineState(const std::string &name) { return RemoveByKey(computePipelineStates_, name); }
    [[nodiscard]] bool HasComputePipelineState(const std::string &name) const { return HasKey(computePipelineStates_, name); }
    [[nodiscard]] const D3D12_COMPUTE_PIPELINE_STATE_DESC &GetComputePipelineState(const std::string &name) const { return computePipelineStates_.at(name).value; }
    [[nodiscard]] const ComputePipelineStatePresets &ComputePipelineStates() const { return computePipelineStates_; }

    //--------- Compiled Shaders ---------//
    void RegisterCompiledShader(const std::string &name, ShaderCompiler::ShaderCompiledInfo *ptr) { compiledShaders_.push_back(name, ptr); }
    bool RemoveCompiledShader(const std::string &name) { return RemoveByKey(compiledShaders_, name); }
    [[nodiscard]] bool HasCompiledShader(const std::string &name) const { return HasKey(compiledShaders_, name); }
    [[nodiscard]] ShaderCompiler::ShaderCompiledInfo *GetCompiledShader(const std::string &name) const { return compiledShaders_.at(name).value; }
    [[nodiscard]] const CompiledShaderPresets &CompiledShaders() const { return compiledShaders_; }

    /// @brief すべてのプリセットをクリア
    void ClearAll() {
        rootSignatures_.clear();
        rootParameters_.clear();
        descriptorRanges_.clear();
        rootConstants_.clear();
        rootDescriptors_.clear();
        samplers_.clear();
        inputLayouts_.clear();
        rasterizerStates_.clear();
        blendStates_.clear();
        depthStencilStates_.clear();
        graphicsPipelineStates_.clear();
        computePipelineStates_.clear();
        compiledShaders_.clear();
    }

private:
    mutable RootSignaturePresets         rootSignatures_;
    mutable RootParameterPresets         rootParameters_;
    mutable DescriptorRangePresets       descriptorRanges_;
    mutable RootConstantsPresets         rootConstants_;
    mutable RootDescriptorPresets        rootDescriptors_;
    mutable SamplerPresets               samplers_;
    mutable InputLayoutPresets           inputLayouts_;
    mutable RasterizerStatePresets       rasterizerStates_;
    mutable BlendStatePresets            blendStates_;
    mutable DepthStencilStatePresets     depthStencilStates_;
    mutable GraphicsPipelineStatePresets graphicsPipelineStates_;
    mutable ComputePipelineStatePresets  computePipelineStates_;
    mutable CompiledShaderPresets        compiledShaders_;

    template<typename MapT>
    static bool HasKey(const MapT &m, const std::string &name) {
        return const_cast<MapT&>(m).find(name) != const_cast<MapT&>(m).end();
    }
    template<typename MapT>
    static bool RemoveByKey(MapT &m, const std::string &name) {
        auto it = m.find(name);
        if (it == m.end()) return false;
        m.erase(it->index);
        return true;
    }
};

} // namespace KashipanEngine
