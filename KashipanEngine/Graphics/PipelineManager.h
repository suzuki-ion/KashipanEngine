#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Utilities/FileIO/JSON.h"
#include "Graphics/Pipeline/PipelineInfo.h"
#include "Graphics/Pipeline/System/ShaderCompiler.h"
#include "Graphics/Pipeline/ComponentsPresetContainer.h"
#include "Graphics/Pipeline/System/PipelineCreator.h"
#include "Graphics/Pipeline/System/ShaderVariableMapCreator.h"

namespace KashipanEngine {

class GraphicsEngine;

/// @brief パイプライン管理用クラス
class PipelineManager {
public:
    /// @brief コンストラクタ（GraphicsEngine からのみ生成可能）
    /// @param device D3D12 デバイス
    /// @param pipelineSettingsPath パイプライン設定ファイルパス
    PipelineManager(Passkey<GraphicsEngine>, ID3D12Device *device, const std::string &pipelineSettingsPath = "Resources/PipelineSetting.json");
    ~PipelineManager() = default;

    /// @brief パイプラインの再読み込み
    void ReloadPipelines();

    /// @brief パイプライン情報の取得
    [[nodiscard]] const PipelineInfo &GetPipeline(const std::string &pipelineName) { return pipelineInfos_.at(pipelineName); }
    /// @brief パイプラインの存在確認
    [[nodiscard]] bool HasPipeline(const std::string &pipelineName) const { return pipelineInfos_.find(pipelineName) != pipelineInfos_.end(); }

    /// @brief 指定のコマンドリストにパイプラインをセット（差分管理は PipelineBinder 側で行う想定）
    void ApplyPipeline(ID3D12GraphicsCommandList* commandList, const std::string &pipelineName);

    /// @brief 指定パイプラインのシェーダーから NameMap を構築して返す
    /// @param pipelineName パイプライン名
    /// @param appendSpace Space をキーへ付与するか ("name#s0")
    /// @return NameMap<ShaderVariableBinding> （コピー）
    [[nodiscard]] MyStd::NameMap<ShaderVariableBinding> BuildPipelineShaderVariableMap(const std::string &pipelineName, bool appendSpace = true) const {
        auto it = pipelineInfos_.find(pipelineName);
        if (it == pipelineInfos_.end()) return {};
        MyStd::NameMap<ShaderVariableBinding> combined;
        for (auto *shader : it->second.Shaders()) {
            if (!shader) continue;
            auto single = CreateShaderVariableMap(*shader, appendSpace);
            // マージ（既存キーは上書きしない）
            for (auto e = single.begin(); e != single.end(); ++e) {
                const auto &k = e->key;
                if (!combined.Contains(k)) {
                    combined.Set(k, e->value);
                }
            }
        }
        return combined;
    }

private:
    void LoadPreset();
    void LoadPipelines();

    ID3D12Device *device_ = nullptr;

    std::unique_ptr<ShaderCompiler> shaderCompiler_;
    ComponentsPresetContainer components_{ Passkey<PipelineManager>{} };
    std::unique_ptr<PipelineCreator> pipelineCreator_;

    std::string pipelineSettingsPath_;
    std::string pipelineFolderPath_;
    std::unordered_map<std::string, std::string> presetFolderNames_;

    std::unordered_map<std::string, PipelineInfo> pipelineInfos_;
};

} // namespace KashipanEngine