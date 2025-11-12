#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Utilities/FileIO/JSON.h"

#include "Graphics/Pipeline/PipelineInfo.h"
#include "Graphics/Pipeline/System/ComponentsPresetContainer.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"

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

private:
    ID3D12Device *device_ = nullptr;
    ComponentsPresetContainer *components_ = nullptr;
    ShaderCompiler *shaderCompiler_ = nullptr;
};

} // namespace KashipanEngine