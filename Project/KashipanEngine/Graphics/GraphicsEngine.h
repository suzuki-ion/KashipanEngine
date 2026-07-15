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
