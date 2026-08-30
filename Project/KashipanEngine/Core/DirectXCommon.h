#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>
#include "Core/DirectX/DX12DXGIs.h"
#include "Core/DirectX/DX12Device.h"
#include "Core/DirectX/DX12CommandQueue.h"
#include "Core/DirectX/DX12Fence.h"
#include "Core/DirectX/DX12SwapChain.h"
#include "Core/DirectX/DX12Commands.h"
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSampler.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class Window;
class GraphicsEngine;
class Renderer;
class TextureManager;
class FontManager;
class SamplerManager;
class VideoTexture;
class GifTexture;
class ScreenBuffer;
class ShadowMapBuffer;
class ComputeCommandProcessor;
#if defined(USE_IMGUI)
class ImGuiManager;
#endif

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
    void AllDestroyPendingSwapChains(Passkey<GameEngine>);

    DirectXCommon(Passkey<GameEngine>, bool enableDebugLayer = true);
    ~DirectXCommon();

    /// @brief 描画前処理
    void BeginDraw(Passkey<GameEngine>);
    /// @brief 描画後処理
    void EndDraw(Passkey<GameEngine>);

    /// @brief スワップチェーン作成
    /// @param hwnd ウィンドウハンドル
    /// @param width 横幅
    /// @param height 高さ
    /// @param bufferCount バッファ数
    /// @return 作成したスワップチェーンのポインタ
    DX12SwapChain *CreateSwapChain(Passkey<Window>, SwapChainType swapChainType, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);

    /// @brief スワップチェーン破棄指示
    /// @param hwnd ウィンドウハンドル
    void DestroySwapChainSignal(Passkey<Window>, HWND hwnd);

    /// @brief D3D12デバイス取得
    ID3D12Device* GetDevice(Passkey<GraphicsEngine>) const { return dx12Device_->GetDevice(); }

    /// @brief D3D12デバイス取得（TextureManager 用）
    ID3D12Device* GetDeviceForTextureManager(Passkey<TextureManager>) const { return dx12Device_->GetDevice(); }

    /// @brief D3D12デバイス取得（FontManager 用）
    ID3D12Device* GetDeviceForFontManager(Passkey<FontManager>) const { return dx12Device_->GetDevice(); }
    /// @brief コマンドキュー取得（TextureManager 用）
    ID3D12CommandQueue* GetCommandQueueForTextureManager(Passkey<TextureManager>) const { return dx12CommandQueue_->GetCommandQueue(); }
    /// @brief SRV ヒープ取得（TextureManager 用）
    SRVHeap* GetSRVHeapForTextureManager(Passkey<TextureManager>) const { return SRVHeap_.get(); }
    /// @brief Sampler ヒープ取得（TextureManager 用）
    SamplerHeap* GetSamplerHeapForTextureManager(Passkey<TextureManager>) const { return SamplerHeap_.get(); }

    /// @brief D3D12デバイス取得（SamplerManager 用）
    ID3D12Device *GetDeviceForSamplerManager(Passkey<SamplerManager>) const { return dx12Device_->GetDevice(); }
    /// @brief Sampler ヒープ取得（SamplerManager 用）
    SamplerHeap *GetSamplerHeapForSamplerManager(Passkey<SamplerManager>) const { return SamplerHeap_.get(); }

    /// @brief D3D12デバイス取得（ScreenBuffer 用）
    ID3D12Device* GetDeviceForScreenBuffer(Passkey<ScreenBuffer>) const { return dx12Device_->GetDevice(); }
    /// @brief コマンドキュー取得（ScreenBuffer 用。画像ファイル保存(ImageExporter)のキャプチャに使用）
    ID3D12CommandQueue* GetCommandQueueForScreenBuffer(Passkey<ScreenBuffer>) const { return dx12CommandQueue_->GetCommandQueue(); }

    /// @brief ワンショットでコマンドを記録・実行し、フェンス待機まで行う（TextureManager 用）
    /// @param record コマンド記録関数（Close は内部で行う）
    void ExecuteOneShotCommandsForTextureManager(Passkey<TextureManager>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

    /// @brief ワンショットでコマンドを記録・実行し、フェンス待機まで行う（FontManager 用。グリフアトラスのアップロードに使用）
    /// @param record コマンド記録関数（Close は内部で行う）
    void ExecuteOneShotCommandsForFontManager(Passkey<FontManager>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

    /// @brief ワンショットでコマンドを記録・実行し、フェンス待機まで行う（Renderer 用。
    ///        BlueNoiseGeneratorによる起動時のブルーノイズ生成（コンピュートシェーダー）に使用）
    /// @param record コマンド記録関数（Close は内部で行う）
    void ExecuteOneShotCommandsForRenderer(Passkey<Renderer>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

    /// @brief D3D12デバイス取得（VideoTexture 用）
    ID3D12Device* GetDeviceForVideoTexture(Passkey<VideoTexture>) const { return dx12Device_->GetDevice(); }
    /// @brief SRV ヒープ取得（VideoTexture 用）
    SRVHeap* GetSRVHeapForVideoTexture(Passkey<VideoTexture>) const { return SRVHeap_.get(); }

    /// @brief ワンショットでコマンドを記録・実行するが、フェンス待機（CPUブロック）は行わない（VideoTexture 用）
    /// @details 動画フレームの毎フレーム更新のようにGPUストールを避けたい高頻度呼び出しのために用意している。
    ///          共有DIRECTキューへ投入するため、この後に積まれる通常描画コマンドより必ず先にGPU側で実行される
    ///          （同一キューはFIFOで実行されるため、追加の同期なしに順序が保証される）。
    /// @param record コマンド記録関数（Close は内部で行う）
    /// @return 発行したフェンス値（IsVideoUploadFenceCompleteでの完了確認に使う）。失敗時は0
    uint64_t ExecuteOneShotCommandsForVideoTexture(Passkey<VideoTexture>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

    /// @brief ExecuteOneShotCommandsForVideoTextureで発行したフェンス値がGPU側で完了しているかをブロックせずに確認する
    bool IsVideoUploadFenceComplete(Passkey<VideoTexture>, uint64_t fenceValue) const;

    /// @brief D3D12デバイス取得（GifTexture 用）
    ID3D12Device* GetDeviceForGifTexture(Passkey<GifTexture>) const { return dx12Device_->GetDevice(); }

    /// @brief ワンショットでコマンドを記録・実行し、フェンス待機まで行う（GifTexture 用）
    /// @details GIFのフレーム切り替えは低頻度（数十ms〜数百ms間隔）なため、VideoTextureのような
    ///          ダブルバッファ+フェンスポーリングは行わず、TextureManagerと同じブロッキング方式で良い
    /// @param record コマンド記録関数（Close は内部で行う）
    void ExecuteOneShotCommandsForGifTexture(Passkey<GifTexture>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

    /// @brief フレーム終端で実行するコマンドリストを登録
    void AddRecordCommandList(Passkey<DX12SwapChain>, ID3D12CommandList* list);
    void AddRecordCommandList(Passkey<ScreenBuffer>, ID3D12CommandList* list);
    void AddRecordCommandList(Passkey<ShadowMapBuffer>, ID3D12CommandList* list);
    void AddRecordCommandList(Passkey<ComputeCommandProcessor>, ID3D12CommandList* list);
    void AddRecordCommandList(Passkey<Renderer>, ID3D12CommandList* list);

#if defined(USE_IMGUI)
    /// @brief D3D12デバイス取得（ImGui 用）
    ID3D12Device* GetDeviceForImGui(Passkey<ImGuiManager>) const { return dx12Device_->GetDevice(); }
    /// @brief コマンドキュー取得（ImGui 用）
    ID3D12CommandQueue* GetCommandQueueForImGui(Passkey<ImGuiManager>) const { return dx12CommandQueue_->GetCommandQueue(); }
    /// @brief SRV ヒープ取得（ImGui 用）
    SRVHeap* GetSRVHeapForImGui(Passkey<ImGuiManager>) const { return SRVHeap_.get(); }

    /// @brief 指定のウィンドウのコマンドリスト取得（ImGui 用）
    ID3D12GraphicsCommandList* GetRecordedCommandListForImGui(Passkey<ImGuiManager>, HWND hwnd) const;

    // ImGui viewport 用のスワップチェーンを必要に応じて生成する
    DX12SwapChain* GetOrCreateSwapChainForImGuiViewport(Passkey<ImGuiManager>, HWND hwnd, int32_t width, int32_t height);
#endif

    /// @brief コマンドオブジェクトを確保（DirectXCommon が所有）
    /// @return スロットインデックス（失敗時は -1）
    int AcquireCommandObjects(Passkey<ScreenBuffer>);
    int AcquireCommandObjects(Passkey<ShadowMapBuffer>);
    int AcquireCommandObjects(Passkey<ComputeCommandProcessor>);
    int AcquireCommandObjects(Passkey<Renderer>);

    /// @brief コマンドオブジェクトを取得
    DX12Commands* GetCommandObjects(Passkey<ScreenBuffer>, int slotIndex);
    DX12Commands* GetCommandObjects(Passkey<ShadowMapBuffer>, int slotIndex);
    DX12Commands* GetCommandObjects(Passkey<ComputeCommandProcessor>, int slotIndex);
    DX12Commands* GetCommandObjects(Passkey<Renderer>, int slotIndex);

    /// @brief コマンドオブジェクトを解放
    void ReleaseCommandObjects(Passkey<DX12SwapChain>, int slotIndex);
    void ReleaseCommandObjects(Passkey<ScreenBuffer>, int slotIndex);
    void ReleaseCommandObjects(Passkey<ShadowMapBuffer>, int slotIndex);
    void ReleaseCommandObjects(Passkey<ComputeCommandProcessor>, int slotIndex);
    void ReleaseCommandObjects(Passkey<Renderer>, int slotIndex);

private:
    DirectXCommon(const DirectXCommon &) = delete;
    DirectXCommon &operator=(const DirectXCommon &) = delete;
    DirectXCommon(DirectXCommon &&) = delete;
    DirectXCommon &operator=(DirectXCommon &&) = delete;

    /// @brief スワップチェーン破棄処理
    void DestroyPendingSwapChains();
    /// @brief フェンス待機
    /// @return 待機に成功したらtrue、タイムアウトやエラーの場合はfalseを返す
    bool WaitForFence();
    /// @brief コマンド実行
    void ExecuteCommandLists();

    int AcquireCommandObjectsInternal(std::vector<std::unique_ptr<DX12Commands>>& pool, std::vector<int>& freeSlots);
    DX12Commands* GetCommandObjectsInternal(std::vector<std::unique_ptr<DX12Commands>>& pool, int slotIndex);
    void ReleaseCommandObjectsInternal(std::vector<std::unique_ptr<DX12Commands>>& pool, std::vector<int>& freeSlots, int slotIndex);

    std::unique_ptr<DX12DXGIs> dx12DXGIs_;
    std::unique_ptr<DX12Device> dx12Device_;
    std::unique_ptr<DX12CommandQueue> dx12CommandQueue_;
    std::unique_ptr<DX12Fence> dx12Fence_;

    std::unique_ptr<RTVHeap> RTVHeap_;
    std::unique_ptr<DSVHeap> DSVHeap_;
    std::unique_ptr<SRVHeap> SRVHeap_;
    std::unique_ptr<SamplerHeap> SamplerHeap_;

    std::vector<std::unique_ptr<DX12Commands>> commandObjects_;
    std::vector<int> freeCommandObjectSlots_;

    std::vector<ID3D12CommandList*> recordedCommandLists_;
};

} // namespace KashipanEngine