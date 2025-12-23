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
#include "Core/DirectX/DescriptorHeaps/HeapRTV.h"
#include "Core/DirectX/DescriptorHeaps/HeapDSV.h"
#include "Core/DirectX/DescriptorHeaps/HeapSRV.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class Window;
class GraphicsEngine;
class Renderer;
class TextureManager;
#if defined(USE_IMGUI)
class ImGuiManager;
#endif

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

    /// @brief D3D12デバイス取得（TextureManager 用）
    ID3D12Device* GetDeviceForTextureManager(Passkey<TextureManager>) const { return dx12Device_->GetDevice(); }
    /// @brief コマンドキュー取得（TextureManager 用）
    ID3D12CommandQueue* GetCommandQueueForTextureManager(Passkey<TextureManager>) const { return dx12CommandQueue_->GetCommandQueue(); }
    /// @brief SRV ヒープ取得（TextureManager 用）
    SRVHeap* GetSRVHeapForTextureManager(Passkey<TextureManager>) const { return SRVHeap_.get(); }

    /// @brief ワンショットでコマンドを記録・実行し、フェンス待機まで行う（TextureManager 用）
    /// @param record コマンド記録関数（Close は内部で行う）
    void ExecuteOneShotCommandsForTextureManager(Passkey<TextureManager>, const std::function<void(ID3D12GraphicsCommandList*)>& record);

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
    // ImGui viewport 用スワップチェーンの破棄指示（遅延破棄）
    void DestroySwapChainSignalForImGuiViewport(Passkey<ImGuiManager>, HWND hwnd);
#endif

    /// @brief 指定のウィンドウのスワップチェーン取得
    DX12SwapChain *GetSwapChain(Passkey<Renderer>, HWND hwnd) const;
    /// @brief 指定のウィンドウのコマンドリスト取得
    ID3D12GraphicsCommandList *GetRecordedCommandList(Passkey<Renderer>, HWND hwnd) const;

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