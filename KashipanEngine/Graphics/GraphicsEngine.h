#pragma once
#include <memory>

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class PipelineManager;

/// @brief グラフィックスエンジンクラス
class GraphicsEngine final {
public:
    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    GraphicsEngine(Passkey<GameEngine>, DirectXCommon* directXCommon);
    ~GraphicsEngine();

private:
    GraphicsEngine(const GraphicsEngine&) = delete;
    GraphicsEngine& operator=(const GraphicsEngine&) = delete;
    GraphicsEngine(GraphicsEngine&&) = delete;
    GraphicsEngine& operator=(GraphicsEngine&&) = delete;

    DirectXCommon* directXCommon_ = nullptr; // 非所有
    std::unique_ptr<PipelineManager> pipelineManager_;
};

} // namespace KashipanEngine
