#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <optional>
#include <string>
#include <unordered_map>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/PipelineInfo.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/JsonParser/RootSignature.h"

namespace KashipanEngine {

class PipelineManager;

/// @brief パイプライン構築を担当するクラス（PipelineManager専用）
class PipelineCreator final {
public:
    /// @brief PipelineManager からのみ生成可能
    PipelineCreator(Passkey<PipelineManager>, ID3D12Device *device,
                    ComponentsPresetContainer *components,
                    ShaderCompiler *shaderCompiler)
        : device_(device), components_(components), shaderCompiler_(shaderCompiler) {}

    /// @brief レンダーパイプラインを構築
    bool CreateRender(const Json &json, PipelineInfo &outInfo);
    /// @brief コンピュートパイプラインを構築
    bool CreateCompute(const Json &json, PipelineInfo &outInfo);

    /// @brief RootSignatureの再利用キャッシュをクリアする（PipelineManager::ReloadPipelines用）
    void ClearRootSignatureCache(Passkey<PipelineManager>) { rootSignatureCache_.clear(); }

private:
    /// @brief ShaderVariableBinder を構築
    /// @param isCompute Computeパイプライン用かどうか（ShaderVisibility::ALLの解釈とBind先の切り替えに使用）
    void BuildShaderVariableBinder(PipelineInfo &outInfo,
        const std::vector<std::pair<ShaderCompiler::ShaderCompiledInfo*, std::string>> &shadersWithStages,
        std::optional<Pipeline::JsonParser::RootSignatureParsed> customRootSig = std::nullopt,
        bool isCompute = false);

    /// @brief シリアライズ済みRootSignature内容が同一なら既存の ID3D12RootSignature を再利用する
    /// @details パイプラインバリアント動的生成（PipelineVariantResolver由来）で同種のRootSignatureが
    ///          何度も生成されるのを避け、GPUオブジェクト数の増大を抑える
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetOrCreateRootSignature(ID3DBlob *signatureBlob);

    ID3D12Device *device_ = nullptr;
    ComponentsPresetContainer *components_ = nullptr;
    ShaderCompiler *shaderCompiler_ = nullptr;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatureCache_;
};

} // namespace KashipanEngine