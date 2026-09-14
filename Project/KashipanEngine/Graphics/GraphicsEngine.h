#pragma once
#include <cstdint>
#include <memory>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class PipelineManager;
class Renderer;
class SceneContext;
class Scene;

/// @brief グラフィックスエンジンクラス
class GraphicsEngine final {
public:
    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    GraphicsEngine(Passkey<GameEngine>, DirectXCommon* directXCommon);
    ~GraphicsEngine();

    /// @brief フレーム描画処理
    /// @param sceneContext 描画対象シーンのコンテキスト
    void RenderFrame(Passkey<GameEngine>, SceneContext *sceneContext);

    /// @brief レンダラーのGPUリソース全開放
    void ReleaseRendererResources(Passkey<GameEngine>);
    /// @brief レンダラーのGPUリソース全開放（Scene用）
    /// @details Scene::PlayStop()がClearSceneObjects/ClearSceneComponents+LoadFromJSONで
    ///          シーン内の全オブジェクト・コンポーネントを新しいインスタンスへ作り直す際に呼ぶ。
    ///          Renderer::resourceContainer_（構造化バッファ等のキャッシュ）の一部キーは
    ///          コンポーネントのインスタンスアドレス等、再生成のたびに変わる値を含むため、
    ///          このタイミングでキャッシュごと破棄しないと、古いインスタンス由来のエントリが
    ///          二度と参照されないまま溜まり続け、ディスクリプタヒープを再生・停止のたびに
    ///          消費し尽くしてクラッシュする（Play/Stopは通常のシーン切り替え
    ///          （SceneManager::CommitPendingSceneChange）を経由しないため、
    ///          そちらで行っているキャッシュ破棄の対象から漏れていた）
    void ReleaseRendererResources(Passkey<Scene>);

    /// @brief 直近のRenderFrameで実際に発行されたドローコール数（パフォーマンス調査用）
    std::uint32_t GetLastFrameDrawCallCount() const;

private:
    GraphicsEngine(const GraphicsEngine&) = delete;
    GraphicsEngine& operator=(const GraphicsEngine&) = delete;
    GraphicsEngine(GraphicsEngine&&) = delete;
    GraphicsEngine& operator=(GraphicsEngine&&) = delete;

    DirectXCommon* directXCommon_ = nullptr;
    std::unique_ptr<PipelineManager> pipelineManager_;
    std::unique_ptr<Renderer> renderer_;
};

} // namespace KashipanEngine
