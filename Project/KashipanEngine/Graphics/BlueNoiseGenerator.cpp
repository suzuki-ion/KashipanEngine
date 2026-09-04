#include "BlueNoiseGenerator.h"

#include <cstring>
#include <vector>

#include "Core/DirectXCommon.h"
#include "Debug/Logger.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"

namespace KashipanEngine {

namespace {

/// @brief BlueNoiseInitConstants（BlueNoiseInitCS.hlsl）と同レイアウトの構造体
struct InitConstants {
    std::uint32_t size = 0;
    std::uint32_t padding[3]{};
};
/// @brief BlueNoiseDFTConstants（BlueNoiseDFTCS.hlsl）と同レイアウトの構造体
struct DFTConstants {
    std::uint32_t size = 0;
    std::uint32_t inverse = 0;
    float padding[2]{};
};
/// @brief BlueNoiseFilterConstants（BlueNoiseFilterCS.hlsl）と同レイアウトの構造体
struct FilterConstants {
    std::uint32_t size = 0;
    float lowFreqCutoff = 0.0f;
    float padding[2]{};
};
/// @brief BlueNoiseExtractConstants（BlueNoiseExtractFieldCS.hlsl）と同レイアウトの構造体
struct ExtractConstants {
    std::uint32_t size = 0;
    std::uint32_t padding[3]{};
};
/// @brief BlueNoiseSortConstants（BlueNoiseBitonicSortCS.hlsl）と同レイアウトの構造体
struct SortConstants {
    std::uint32_t total = 0;
    std::uint32_t blockSize = 0;
    std::uint32_t compareDistance = 0;
    std::uint32_t padding = 0;
};
/// @brief BlueNoiseRankConstants（BlueNoiseRankCS.hlsl）と同レイアウトの構造体
struct RankConstants {
    std::uint32_t total = 0;
    std::uint32_t padding[3]{};
};

/// @brief UAVバリアを1つ発行する（compute→computeの依存関係はTransitionToではバリアが
///        挿入されない＝UNORDERED_ACCESS状態のまま変化が無いため、明示的に呼ぶ必要がある）
void UavBarrier(ID3D12GraphicsCommandList *commandList, RWStructuredBufferResource *resource) {
    LogScope scope;
    if (!commandList || !resource) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource->GetResource();
    commandList->ResourceBarrier(1, &barrier);
}

template <typename T>
void UploadConstants(ConstantBufferResource *buffer, const T &data) {
    LogScope scope;
    if (!buffer) return;
    if (void *mapped = buffer->Map()) {
        std::memcpy(mapped, &data, sizeof(T));
    }
}

} // namespace

BlueNoiseGenerator::~BlueNoiseGenerator() = default;

void BlueNoiseGenerator::Generate(DirectXCommon *directXCommon, PipelineManager *pipelineManager, Passkey<Renderer> rendererPasskey) {
    LogScope scope;
    if (!directXCommon || !pipelineManager) return;
    static const char *kPipelines[] = {
        "BlueNoiseInit", "BlueNoiseDFT", "BlueNoiseFilter",
        "BlueNoiseExtractField", "BlueNoiseBitonicSort", "BlueNoiseRank",
    };
    for (const char *name : kPipelines) {
        if (!pipelineManager->HasPipeline(name)) return;
    }

    const std::uint32_t size = kSize;
    const std::uint32_t total = size * size;

    bufferA_ = std::make_unique<RWStructuredBufferResource>(sizeof(float) * 2, total);
    bufferB_ = std::make_unique<RWStructuredBufferResource>(sizeof(float) * 2, total);
    sortKeys_ = std::make_unique<RWStructuredBufferResource>(sizeof(float), total);
    sortIndices_ = std::make_unique<RWStructuredBufferResource>(sizeof(std::uint32_t), total);
    ditherValues_ = std::make_unique<RWStructuredBufferResource>(sizeof(float), total, /*createSrv=*/true);

    // Bitonic Sortはステージ・ステップの組ごとに別々の定数バッファが要る。全パスの記録が
    // 実際のGPU実行より先に終わるため、同じバッファを使い回して毎回上書きすると
    // 全パスがCPU側で最後に書き込んだ値のみを参照してしまう（GPU実行はCloseした後で
    // まとめて始まるため）
    std::vector<std::unique_ptr<ConstantBufferResource>> sortConstantBuffers;
    for (std::uint32_t blockSize = 2; blockSize <= total; blockSize *= 2) {
        for (std::uint32_t compareDistance = blockSize / 2; compareDistance > 0; compareDistance /= 2) {
            sortConstantBuffers.push_back(std::make_unique<ConstantBufferResource>(sizeof(SortConstants)));
        }
    }

    auto initConstants = std::make_unique<ConstantBufferResource>(sizeof(InitConstants));
    auto dftForwardConstants = std::make_unique<ConstantBufferResource>(sizeof(DFTConstants));
    auto dftInverseConstants = std::make_unique<ConstantBufferResource>(sizeof(DFTConstants));
    auto filterConstants = std::make_unique<ConstantBufferResource>(sizeof(FilterConstants));
    auto extractConstants = std::make_unique<ConstantBufferResource>(sizeof(ExtractConstants));
    auto rankConstants = std::make_unique<ConstantBufferResource>(sizeof(RankConstants));

    UploadConstants(initConstants.get(), InitConstants{ size, {} });
    UploadConstants(dftForwardConstants.get(), DFTConstants{ size, 0, {} });
    UploadConstants(dftInverseConstants.get(), DFTConstants{ size, 1, {} });
    // 中心から25%以内の低周波成分を弱める（値が小さいほど高周波寄り＝よりブルーノイズらしくなるが、
    // 弱めすぎると点の粗密が目立つ低品質な白色ノイズに近づく）
    UploadConstants(filterConstants.get(), FilterConstants{ size, 0.25f, {} });
    UploadConstants(extractConstants.get(), ExtractConstants{ size, {} });
    UploadConstants(rankConstants.get(), RankConstants{ total, {} });

    const std::uint32_t group1D = (total + 63) / 64;
    const std::uint32_t group2D = (size + 7) / 8;

    directXCommon->ExecuteOneShotCommandsForRenderer(rendererPasskey,
        [&](ID3D12GraphicsCommandList *commandList) {
            PipelineBinder pipelineBinder(commandList, pipelineManager);
            pipelineBinder.Invalidate();

            bufferA_->SetCommandList(commandList);
            bufferB_->SetCommandList(commandList);
            sortKeys_->SetCommandList(commandList);
            sortIndices_->SetCommandList(commandList);
            ditherValues_->SetCommandList(commandList);
            bufferA_->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            bufferB_->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            sortKeys_->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            sortIndices_->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            ditherValues_->TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            // 1. 白色ノイズ生成 → bufferA
            pipelineBinder.UsePipeline("BlueNoiseInit");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseInit");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseInitConstants", initConstants.get());
                binder.Bind("Compute:gSpectrum", bufferA_.get());
                commandList->Dispatch(group1D, 1, 1);
            }
            UavBarrier(commandList, bufferA_.get());

            // 2. 順方向DFT: bufferA → bufferB
            pipelineBinder.UsePipeline("BlueNoiseDFT");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseDFT");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseDFTConstants", dftForwardConstants.get());
                binder.Bind("Compute:gSource", bufferA_.get());
                binder.Bind("Compute:gDest", bufferB_.get());
                commandList->Dispatch(group2D, group2D, 1);
            }
            UavBarrier(commandList, bufferB_.get());

            // 3. 低周波フィルタ: bufferB → bufferA
            pipelineBinder.UsePipeline("BlueNoiseFilter");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseFilter");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseFilterConstants", filterConstants.get());
                binder.Bind("Compute:gSource", bufferB_.get());
                binder.Bind("Compute:gDest", bufferA_.get());
                commandList->Dispatch(group2D, group2D, 1);
            }
            UavBarrier(commandList, bufferA_.get());

            // 4. 逆方向DFT: bufferA → bufferB
            pipelineBinder.UsePipeline("BlueNoiseDFT");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseDFT");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseDFTConstants", dftInverseConstants.get());
                binder.Bind("Compute:gSource", bufferA_.get());
                binder.Bind("Compute:gDest", bufferB_.get());
                commandList->Dispatch(group2D, group2D, 1);
            }
            UavBarrier(commandList, bufferB_.get());

            // 5. 実部を取り出しソート用キー・インデックスを初期化: bufferB → sortKeys/sortIndices
            pipelineBinder.UsePipeline("BlueNoiseExtractField");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseExtractField");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseExtractConstants", extractConstants.get());
                binder.Bind("Compute:gSpatial", bufferB_.get());
                binder.Bind("Compute:gSortKeys", sortKeys_.get());
                binder.Bind("Compute:gSortIndices", sortIndices_.get());
                commandList->Dispatch(group1D, 1, 1);
            }
            UavBarrier(commandList, sortKeys_.get());
            UavBarrier(commandList, sortIndices_.get());

            // 6. Bitonic Sort（グローバル実装。ステージ・ステップの組ごとに1回Dispatchする）
            pipelineBinder.UsePipeline("BlueNoiseBitonicSort");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseBitonicSort");
                binder.SetCommandList(commandList);
                size_t passIndex = 0;
                for (std::uint32_t blockSize = 2; blockSize <= total; blockSize *= 2) {
                    for (std::uint32_t compareDistance = blockSize / 2; compareDistance > 0; compareDistance /= 2) {
                        auto *sortConstants = sortConstantBuffers[passIndex].get();
                        UploadConstants(sortConstants, SortConstants{ total, blockSize, compareDistance, 0 });
                        binder.Bind("Compute:BlueNoiseSortConstants", sortConstants);
                        binder.Bind("Compute:gSortKeys", sortKeys_.get());
                        binder.Bind("Compute:gSortIndices", sortIndices_.get());
                        commandList->Dispatch(group1D, 1, 1);
                        UavBarrier(commandList, sortKeys_.get());
                        UavBarrier(commandList, sortIndices_.get());
                        ++passIndex;
                    }
                }
            }

            // 7. ソート結果から元のインデックスごとの正規化順位を書き出す: sortIndices → ditherValues
            pipelineBinder.UsePipeline("BlueNoiseRank");
            {
                auto &binder = pipelineManager->GetShaderVariableBinder(rendererPasskey, "BlueNoiseRank");
                binder.SetCommandList(commandList);
                binder.Bind("Compute:BlueNoiseRankConstants", rankConstants.get());
                binder.Bind("Compute:gSortIndices", sortIndices_.get());
                binder.Bind("Compute:gDitherValues", ditherValues_.get());
                commandList->Dispatch(group1D, 1, 1);
            }
            UavBarrier(commandList, ditherValues_.get());

            // 後続の描画パスでピクセルシェーダーからStructuredBufferとして読めるように状態遷移しておく
            ditherValues_->TransitionTo(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        });

    // 中間バッファは以後使わないため、GPUメモリを早めに解放しておく
    bufferA_.reset();
    bufferB_.reset();
    sortKeys_.reset();
    sortIndices_.reset();

    ready_ = true;
}

} // namespace KashipanEngine
