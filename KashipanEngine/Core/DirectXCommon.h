#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include "Core/DirectX/DX12DXGIs.h"
#include "Core/DirectX/DX12Device.h"
#include "Core/DirectX/DX12CommandQueue.h"
#include "Core/DirectX/DX12Fence.h"
#include "Core/DirectX/DX12SwapChain.h"
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class Window;
class GraphicsEngine;

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
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
    void ExecuteCommand();


    std::unique_ptr<DX12DXGIs> dx12DXGIs_;
    std::unique_ptr<DX12Device> dx12Device_;
    std::unique_ptr<DX12CommandQueue> dx12CommandQueue_;
    std::unique_ptr<DX12Fence> dx12Fence_;

    std::unique_ptr<RTVHeap> RTVHeap_;
    std::unique_ptr<DSVHeap> DSVHeap_;
    std::unique_ptr<SRVHeap> SRVHeap_;
};

} // namespace KashipanEngine