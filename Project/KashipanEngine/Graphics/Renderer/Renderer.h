#pragma once
#include <memory>
#include <span>
#include <unordered_set>

#include "Utilities/Passkeys.h"
#include "Scene/Components/Render/SceneRenderer.h"

namespace KashipanEngine {

class DirectXCommon;
class GraphicsEngine;
class PipelineManager;
class PipelineBinder;
class ResourceContainer;
class SceneContext;
class ScreenBuffer;
class ShaderVariableBinder;
class IRenderTarget;

/// @brief 描画用のレンダラークラス
/// @details SceneContext から SceneRenderer コンポーネントを取得し、
///          SceneRenderer が構築するソート済み描画リストを元に描画処理を行う。
class Renderer final {
public:
    Renderer(Passkey<GraphicsEngine>, DirectXCommon *directXCommon, PipelineManager *pipelineManager);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;

    /// @brief フレーム描画処理
    /// @param sceneContext 描画対象シーンのコンテキスト
    void RenderFrame(Passkey<GraphicsEngine>, SceneContext *sceneContext);

    /// @brief GPUリソースの全開放
    void ReleaseAllResources(Passkey<GraphicsEngine>);

private:
    /// @brief シーン内のComputeShaderProcessingコンポーネントを処理する（Dispatch実行）
    /// @details 専用コマンドリスト（ComputeCommandProcessor）上で全てまとめて記録・実行される
    void ProcessComputeShaders(SceneContext *sceneContext);

    /// @brief 単一の描画先への描画処理
    void RenderToTarget(IRenderTarget *target,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer);

    /// @brief 同一（パイプライン・メッシュ・マテリアル）バッチのインスタンシング描画
    void DrawBatch(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        std::span<const SceneRenderer::DrawEntry> batch,
        SceneRenderer *sceneRenderer);

    /// @brief 指定描画先・パイプラインに対するカメラ・ライトの定数バッファバインド
    void BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
        IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer);

    /// @brief ポイント/スポットライトの構造化バッファ・個数定数・シャドウマップのバインド
    void BindLightBuffersAndShadowMap(IRenderTarget *target,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer,
        ShaderVariableBinder &shaderBinder);

    /// @brief ScreenBuffer へのポストプロセス適用
    /// @param ownerObject ScreenBuffer を所有するオブジェクト（ポストエフェクトコンポーネントの取得元）
    void RenderPostProcess(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        EmptyObject *ownerObject);

    /// @brief エディター用描画先にのみ、デバッグ表示（グリッド・当たり判定）を描画する
    /// @details target がエディター用描画先でない場合は何もしない
    void RenderEditorDebugOverlay(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief 描画リストに含まれない ScreenBuffer へのポストエフェクトのみの適用
    /// @details オブジェクトの描画が無くともポストエフェクトコンポーネントがあれば実行する
    void RenderPostProcessOnlyTargets(SceneContext *sceneContext,
        const std::unordered_set<const IRenderTarget *> &renderedTargets);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;
    /// @brief GPUリソースキャッシュ
    std::unique_ptr<ResourceContainer> resourceContainer_;
};

} // namespace KashipanEngine
