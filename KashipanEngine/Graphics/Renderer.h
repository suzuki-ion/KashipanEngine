#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <windows.h>
#include <optional>
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"

namespace KashipanEngine {

class Window;
class DirectXCommon;
class GraphicsEngine;
class PipelineManager;
class GameObject2D;
class GameObject3D;

/// @brief 描画指示用構造体
struct RenderCommand final {
    UINT vertexCount = 0;           //< 頂点数
    UINT indexCount = 0;            //< インデックス数
    UINT instanceCount = 1;         //< インスタンス数
    UINT startVertexLocation = 0;   //< 開始頂点位置
    UINT startIndexLocation = 0;    //< 開始インデックス位置
    INT baseVertexLocation = 0;     //< ベース頂点位置
    UINT startInstanceLocation = 0; //< 開始インスタンス位置
};

/// @brief 2D描画用レンダーパス情報構造体
struct RenderPassInfo2D final {
    RenderPassInfo2D(Passkey<GameObject2D>) {}
    Window *window = nullptr;   //< 描画先ウィンドウ
    std::string pipelineName;   //< 使用するパイプライン名
    std::string passName;       //< パス名（デバッグ用）
    std::function<std::optional<RenderCommand>(ShaderVariableBinder &, PipelineBinder &)> renderFunction; //< 描画関数
    std::function<bool()> renderConditionFunction; //< 描画条件関数（任意。指定しない場合は常に描画）
};

/// @brief 3D描画用レンダーパス情報構造体
struct RenderPassInfo3D final {
    RenderPassInfo3D(Passkey<GameObject3D>) {}
    Window *window = nullptr;   //< 描画先ウィンドウ
    std::string pipelineName;   //< 使用するパイプライン名
    std::string passName;       //< パス名（デバッグ用）
    std::function<std::optional<RenderCommand>(ShaderVariableBinder &, PipelineBinder &)> renderFunction; //< 描画関数
    std::function<bool()> renderConditionFunction; //< 描画条件関数（任意。指定しない場合は常に描画）
};

/// @brief 描画用のレンダラークラス
class Renderer final {
public:
    /// @brief コンストラクタ
    /// @param maxRenderPasses 最大レンダーパス数
    Renderer(Passkey<GraphicsEngine>, size_t maxRenderPasses, DirectXCommon *directXCommon, PipelineManager *pipelineManager)
        : directXCommon_(directXCommon), pipelineManager_(pipelineManager) {
        renderPasses3D_.reserve(maxRenderPasses);
        renderPasses2D_.reserve(maxRenderPasses);
    }
    ~Renderer() = default;
    
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// @brief 2Dレンダーパス登録
    void RegisterRenderPass(const RenderPassInfo2D &passInfo) { renderPasses2D_.emplace_back(passInfo); }
    /// @brief 3Dレンダーパス登録
    void RegisterRenderPass(const RenderPassInfo3D &passInfo) { renderPasses3D_.emplace_back(passInfo); }

    /// @brief フレーム描画処理
    void RenderFrame(Passkey<GraphicsEngine>);

    /// @brief Windowの登録
    void RegisterWindow(Passkey<Window>, HWND hwnd, ID3D12GraphicsCommandList* commandList);

private:
    /// @brief 2Dレンダーパス描画処理
    void RenderPasses2D();
    /// @brief 3Dレンダーパス描画処理
    void RenderPasses3D();
    /// @brief 描画コマンド発行処理
    void IssueRenderCommand(ID3D12GraphicsCommandList *commandList, const RenderCommand &renderCommand);

    /// @brief DirectX共通クラスへのポインタ
    DirectXCommon *directXCommon_ = nullptr;
    /// @brief パイプラインマネージャーへのポインタ
    PipelineManager *pipelineManager_ = nullptr;

    /// @brief 2Dレンダーパスリスト
    std::vector<RenderPassInfo2D> renderPasses2D_;
    /// @brief 3Dレンダーパスリスト
    std::vector<RenderPassInfo3D> renderPasses3D_;

    /// @brief ウィンドウごとのPipelineBinder
    std::unordered_map<HWND, PipelineBinder> windowBinders_;
};

} // namespace KashipanEngine
