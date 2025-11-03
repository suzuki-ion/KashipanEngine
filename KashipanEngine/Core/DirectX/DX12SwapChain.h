#pragma once
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;
class Window;

/// @brief DirectX12スワップチェーンクラス
class DX12SwapChain final {
public: 
    DX12SwapChain(Passkey<DirectXCommon>, ID3D12Device *device, IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue,
                  HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);
    ~DX12SwapChain();

    /// @brief SwapChainのサイズ変更
    /// @param width 横幅
    /// @param height 高さ
    void Resize(Passkey<Window>, int32_t width, int32_t height);

private:
    DX12SwapChain(const DX12SwapChain &) = delete;
    DX12SwapChain &operator=(const DX12SwapChain &) = delete;
    DX12SwapChain(DX12SwapChain &&) = delete;
    DX12SwapChain &operator=(DX12SwapChain &&) = delete;

    /// @brief スワップチェーンの作成
    void CreateSwapChain(IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue, HWND hwnd);
    /// @brief ビューポートとシザー矩形の設定
    void SetViewportAndScissorRect();
    /// @brief バックバッファの作成
    void CreateBackBuffers();
    /// @brief 深度ステンシルバッファの作成
    void CreateDepthStencilBuffer();

    int32_t bufferCount_;
    int32_t currentBufferIndex_;
    int32_t width_;
    int32_t height_;

    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissorRect_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
};

} // namespace KashipanEngine