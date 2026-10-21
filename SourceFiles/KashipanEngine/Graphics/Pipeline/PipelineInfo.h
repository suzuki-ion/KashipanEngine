#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <cstdint>
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class PipelineCreator;

enum class PipelineType {
    Render,
    Compute
};

struct PipelineSet {
    ID3D12RootSignature *RootSignature() const { return rootSignature.Get(); }
    ID3D12PipelineState *PipelineState() const { return pipelineState.Get(); }
private:
    friend class PipelineCreator;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

struct PipelineInfo {
    const std::string &Name() const { return name; }
    PipelineType Type() const { return type; }
    /// @brief パイプラインの用途カテゴリ（"3D"/"2D"/"Text"等。未設定の場合は空文字）
    /// @details ImGui上のパイプライン選択で無関係な用途のものが混ざらないようフィルタするために使う
    const std::string &Category() const { return category; }
    D3D_PRIMITIVE_TOPOLOGY TopologyType() const { return topologyType; }
    std::int32_t RenderPriority() const { return renderPriority; }
    const PipelineSet &GetPipelineSet() const { return pipelineSet; }
    const std::vector<ShaderCompiler::ShaderCompiledInfo *> &Shaders() const { return shaders; }
    ShaderVariableBinder &GetVariableBinder() { return variableBinder; }
    bool IsAutoRootDescriptorGenerated() const { return autoRootDescriptorGenerated; }
    /// @brief gMaterials（Pixelシェーダーの struct Material）のバイトレイアウト。Materialを持たないパイプラインでは空
    const MaterialLayout &GetMaterialLayout() const { return materialLayout; }
private:
    friend class PipelineCreator;
    std::string name;
    std::string category;
    PipelineType type = PipelineType::Render;
    std::int32_t renderPriority = 0;
    D3D_PRIMITIVE_TOPOLOGY topologyType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    PipelineSet pipelineSet;
    std::vector<ShaderCompiler::ShaderCompiledInfo *> shaders;
    ShaderVariableBinder variableBinder{ Passkey<PipelineInfo>{} };
    bool autoRootDescriptorGenerated = false; // リフレクションから自動生成されたか
    MaterialLayout materialLayout;
};

} // namespace KashipanEngine
