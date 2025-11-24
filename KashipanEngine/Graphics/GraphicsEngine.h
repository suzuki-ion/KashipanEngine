#pragma once
#include <memory>

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class PipelineManager;
class Renderer;

/// @brief グラフィックスエンジンクラス
class GraphicsEngine final {
public:
    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    GraphicsEngine(Passkey<GameEngine>, DirectXCommon* directXCommon);
    ~GraphicsEngine();

    /// @brief フレーム描画処理
    void RenderFrame(Passkey<GameEngine>);

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
