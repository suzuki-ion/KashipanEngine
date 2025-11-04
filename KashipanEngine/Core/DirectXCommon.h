#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include "Core/DirectX/DX12DXGIs.h"
#include "Core/DirectX/DX12Device.h"
#include "Core/DirectX/DX12Commands.h"
#include "Core/DirectX/DX12Fence.h"
#include "Core/DirectX/DX12SwapChain.h"
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"

namespace KashipanEngine {

class GameEngine;
class Window;

/// @brief DirectX共通クラス
class DirectXCommon final {
public:
    DirectXCommon(Passkey<GameEngine>, bool enableDebugLayer = true);
    ~DirectXCommon();

    DX12SwapChain *CreateSwapChain(Passkey<Window>, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);
    void DestroySwapChain(Passkey<Window>, HWND hwnd);

    /// @brief コマンド実行とフェンス待機
    void ExecuteCommandAndWait(Passkey<GameEngine>);

    /// @brief フェンス待機（SwapChain用）
    /// @return 待機に成功したらtrue、タイムアウトやエラーの場合はfalseを返す
    bool WaitForFence(Passkey<DX12SwapChain>) { return WaitForFence(); }
    
private:
    DirectXCommon(const DirectXCommon &) = delete;
    DirectXCommon &operator=(const DirectXCommon &) = delete;
    DirectXCommon(DirectXCommon &&) = delete;
    DirectXCommon &operator=(DirectXCommon &&) = delete;

    /// @brief フェンス待機
    /// @return 待機に成功したらtrue、タイムアウトやエラーの場合はfalseを返す
    bool WaitForFence();

    std::unique_ptr<DX12DXGIs> dx12DXGIs_;
    std::unique_ptr<DX12Device> dx12Device_;
    std::unique_ptr<DX12Commands> dx12Commands_;
    std::unique_ptr<DX12Fence> dx12Fence_;

    std::unique_ptr<RTVHeap> RTVHeap_;
    std::unique_ptr<DSVHeap> DSVHeap_;
    std::unique_ptr<SRVHeap> SRVHeap_;
};

} // namespace KashipanEngine