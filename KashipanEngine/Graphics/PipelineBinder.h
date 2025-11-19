#pragma once
#include <d3d12.h>
#include <string>
#include "Graphics/PipelineManager.h"

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

private:
    ID3D12GraphicsCommandList* commandList_ = nullptr;
    PipelineManager* manager_ = nullptr; // 非所有
    std::string currentName_;
    bool invalidated_ = false;
};

} // namespace KashipanEngine
