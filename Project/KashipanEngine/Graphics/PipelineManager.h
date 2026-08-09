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
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

namespace KashipanEngine {

class GraphicsEngine;
class Renderer;

/// @brief パイプライン管理用クラス
class PipelineManager {
public:
    /// @brief コンストラクタ（GraphicsEngine からのみ生成可能）
    /// @param device D3D12 デバイス
    /// @param pipelineSettingsPath パイプライン設定ファイルパス
    PipelineManager(Passkey<GraphicsEngine>, ID3D12Device *device, const std::string &pipelineSettingsPath = "Resources/PipelineSetting.json");
    ~PipelineManager();

    /// @brief パイプラインの再読み込み
    void ReloadPipelines();

    /// @brief パイプライン情報の取得
    const PipelineInfo &GetPipeline(const std::string &pipelineName) { return pipelineInfos_.at(pipelineName); }
    const PipelineInfo &GetPipeline(const std::string &pipelineName) const { return pipelineInfos_.at(pipelineName); }
    /// @brief パイプラインの存在確認
    bool HasPipeline(const std::string &pipelineName) const { return pipelineInfos_.find(pipelineName) != pipelineInfos_.end(); }

    /// @brief 指定パイプラインが未読み込みの場合、動的バリアント生成（PipelineVariantResolver）を試みる
    /// @details Object3D/Object2DのBlend×Culling×Toon(Object3Dのみ)組み合わせ名（例: "Object3D.Toon.Solid.BlendAdd"）
    ///          を解決できれば、既存の静的Pipelines/*.jsonと同一スキーマのJSONを合成してオンデマンドで
    ///          PSOを生成しキャッシュする。対応できない名前ではfalseを返す（＝今まで通り「見つからない」扱い）
    /// @return 生成/取得に成功した場合はtrue（呼び出し後は HasPipeline(pipelineName) がtrueになる）
    bool GetOrCreatePipeline(const std::string &pipelineName);

#if defined(USE_IMGUI)
    /// @brief 実行中のPipelineManagerインスタンスへGetOrCreatePipelineを呼び出す（ImGuiの
    ///        バリアントビルダーUI等、インスタンスへのアクセス手段を持たない呼び出し元向け）
    static bool TryGetOrCreatePipeline(const std::string &pipelineName);
    /// @brief 実行中のPipelineManagerインスタンスから指定パイプラインのMaterialLayoutを取得する
    ///        （マテリアルエディタのシェーダー選択・未追加パラメータ一括追加UI向け）
    /// @return パイプラインが見つからない場合はnullptr
    static const MaterialLayout *TryGetMaterialLayout(const std::string &pipelineName);
    /// @brief 実行中のPipelineManagerインスタンスからシェーダー資産のベースディレクトリを取得する
    ///        （マテリアルエディタのモジュール別パラメータグループ表示等、ShaderModuleComposerの
    ///        ユーティリティをインスタンスへのアクセス手段を持たない呼び出し元から使うため）
    /// @return インスタンスが無い場合は空文字
    static std::string TryGetShaderBaseDir();
#endif

    /// @brief 読み込み済みの描画用パイプライン名一覧を取得（ImGuiでの選択用）
    static const std::vector<std::string> &GetLoadedRenderPipelineNames();
    /// @brief 読み込み済みのコンピュート用パイプライン名一覧を取得（ImGuiでの選択用）
    static const std::vector<std::string> &GetLoadedComputePipelineNames();
    /// @brief 指定カテゴリ（PipelineInfo::Category、例: "3D"/"2D"/"Text"）に一致する
    ///        読み込み済み描画用パイプライン名一覧を取得（ImGuiでの選択用。無関係なパイプラインが
    ///        混ざらないようフィルタする目的）
    static std::vector<std::string> GetLoadedRenderPipelineNames(const std::string &category);
    /// @brief 指定カテゴリに一致する読み込み済みコンピュート用パイプライン名一覧を取得（ImGuiでの選択用）
    static std::vector<std::string> GetLoadedComputePipelineNames(const std::string &category);

    /// @brief 指定のコマンドリストにパイプラインをセット（差分管理は PipelineBinder 側で行う想定）
    void ApplyPipeline(ID3D12GraphicsCommandList* commandList, const std::string &pipelineName);
    /// @brief シェーダーの変数バインダーを取得（Rendererから呼ばれる想定）
    ShaderVariableBinder &GetShaderVariableBinder(Passkey<Renderer>, const std::string &pipelineName) {
        auto it = pipelineInfos_.find(pipelineName);
        if (it == pipelineInfos_.end()) {
            throw std::runtime_error("PipelineManager::GetShaderVariableBinder: Pipeline not found: " + pipelineName);
        }
        return it->second.GetVariableBinder();
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
