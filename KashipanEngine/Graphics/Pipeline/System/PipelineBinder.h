#pragma once
#include <d3d12.h>
#include <string>
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources.h"

namespace KashipanEngine {

/// @brief コマンドリスト単位でパイプラインバインド状態を管理するクラス
class PipelineBinder final {
public:
    PipelineBinder() = default;
    PipelineBinder(ID3D12GraphicsCommandList* commandList, PipelineManager* manager)
        : commandList_(commandList), manager_(manager) {}

    void SetManager(PipelineManager* manager) { manager_ = manager; }
    void SetCommandList(ID3D12GraphicsCommandList* commandList) { commandList_ = commandList; }

    /// @brief 指定パイプラインを使用（差分がある場合のみバインド）
    void UsePipeline(const std::string &name) {
        if (!commandList_ || !manager_ || !manager_->HasPipeline(name)) return;
        if (invalidated_ || name != currentName_) {
            const auto &info = manager_->GetPipeline(name);
            // トポロジ設定
            commandList_->IASetPrimitiveTopology(info.TopologyType());
            // ルートシグネチャ（Render のみ）
            const auto &set = info.GetPipelineSet();
            if (info.Type() == PipelineType::Render) {
                commandList_->SetGraphicsRootSignature(set.RootSignature());
            }
            // PSO
            commandList_->SetPipelineState(set.PipelineState());
            currentName_ = name;
            invalidated_ = false;
        }
    }

    /// @brief 次回強制再バインド
    void Invalidate() { invalidated_ = true; }

    /// @brief 現在使用中のパイプライン名
    const std::string &CurrentPipelineName() const { return currentName_; }

    /// @brief 単一の頂点バッファを設定する
    /// @param vb VB リソース
    /// @param stride 頂点ストライド（バイト）
    /// @param slot スロット番号（デフォルト 0）
    void SetVertexBuffer(VertexBufferResource *vb, UINT stride, UINT slot = 0) {
        if (!commandList_ || !vb) return;
        vb->SetCommandList(commandList_);
        D3D12_VERTEX_BUFFER_VIEW view = vb->GetView(stride);
        commandList_->IASetVertexBuffers(slot, 1, &view);
    }

    /// @brief 単一のインデックスバッファを設定する
    /// @param ib IB リソース
    void SetIndexBuffer(IndexBufferResource *ib) {
        if (!commandList_ || !ib) return;
        ib->SetCommandList(commandList_);
        D3D12_INDEX_BUFFER_VIEW view = ib->GetView();
        commandList_->IASetIndexBuffer(&view);
    }

    /// @brief 既に作成済みのビューで頂点バッファを設定する
    void SetVertexBufferView(UINT startSlot, UINT viewCount, const D3D12_VERTEX_BUFFER_VIEW *views) {
        if (!commandList_ || !views || viewCount == 0) return;
        commandList_->IASetVertexBuffers(startSlot, viewCount, views);
    }

    /// @brief 既に作成済みのインデックスバッファビューで設定する
    void SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW *view) {
        if (!commandList_ || !view) return;
        commandList_->IASetIndexBuffer(view);
    }

private:
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    PipelineManager* manager_ = nullptr; // 非所有
    std::string currentName_;
    bool invalidated_ = false;
};

} // namespace KashipanEngine
