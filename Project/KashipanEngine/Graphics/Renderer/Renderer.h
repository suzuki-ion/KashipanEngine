#pragma once
#include <memory>
#include <span>

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
    /// @brief 単一の描画先への描画処理
    void RenderToTarget(IRenderTarget *target,
        std::span<const SceneRenderer::DrawEntry> entries,
        SceneRenderer *sceneRenderer);

    /// @brief 同一（パイプライン・メッシュ・マテリアル）バッチのインスタンシング描画
    void DrawBatch(IRenderTarget *target,
        PipelineBinder &pipelineBinder,
        std::span<const SceneRenderer::DrawEntry> batch,
        SceneRenderer *sceneRenderer);

    /// @brief 指定パイプラインに対するカメラ・ライトの定数バッファバインド
    void BindCameraAndLights(ID3D12GraphicsCommandList *commandList,
        const std::string &pipelineName,
        SceneRenderer *sceneRenderer);

    /// @brief ScreenBuffer へのポストプロセス適用
    void RenderPostProcess(ScreenBuffer *screenBuffer,
        PipelineBinder &pipelineBinder,
        SceneRenderer *sceneRenderer);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;
    /// @brief GPUリソースキャッシュ
    std::unique_ptr<ResourceContainer> resourceContainer_;
};

} // namespace KashipanEngine
