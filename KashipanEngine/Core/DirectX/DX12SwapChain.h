#pragma once
#include <dxgi1_6.h>
#include "Graphics/Resources.h"

namespace KashipanEngine {

class DirectXCommon;
class Window;

/// @brief DirectX12スワップチェーンクラス
class DX12SwapChain final {
    static inline DirectXCommon *sDirectXCommon = nullptr;
    static inline ID3D12Device *sDevice = nullptr;
    static inline IDXGIFactory7 *sDXGIFactory = nullptr;
    static inline ID3D12CommandQueue *sCommandQueue = nullptr;
    static inline ID3D12GraphicsCommandList *sCommandList = nullptr;
    static inline RTVHeap *sRTVHeap = nullptr;
    static inline DSVHeap *sDSVHeap = nullptr;

public:
    static void Initialize(Passkey<DirectXCommon>, DirectXCommon *directXCommon,
        ID3D12Device *device, IDXGIFactory7 *dxgiFactory, ID3D12CommandQueue *commandQueue,
        ID3D12GraphicsCommandList *commandList, RTVHeap *rtvHeap, DSVHeap *dsvHeap) {
        sDirectXCommon = directXCommon;
        sDevice = device;
        sDXGIFactory = dxgiFactory;
        sCommandQueue = commandQueue;
        sCommandList = commandList;
        sRTVHeap = rtvHeap;
        sDSVHeap = dsvHeap;
    }
    /// @brief コンストラクタ
    /// @param hwnd ウィンドウハンドル
    /// @param width 横幅
    /// @param height 高さ
    /// @param bufferCount バッファ数
    DX12SwapChain(Passkey<DirectXCommon>, HWND hwnd, int32_t width, int32_t height, int32_t bufferCount = 2);
    ~DX12SwapChain();

    /// @brief VSync有効化設定
    void SetVSyncEnabled(bool enabled) { enableVSync_ = enabled; }

    /// @brief 描画前処理
    void BeginDraw(Passkey<Window>);
    /// @brief 描画後処理
    void EndDraw(Passkey<DirectXCommon>);
    /// @brief SwapChainのPresent
    /// @param enableVSync VSync有効フラグ
    void Present(Passkey<DirectXCommon>);

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
    void CreateSwapChain();
    /// @brief ビューポートとシザー矩形の設定
    void SetViewportAndScissorRect();
    /// @brief バックバッファの作成
    void CreateBackBuffers();
    /// @brief 深度ステンシルバッファの作成
    void CreateDepthStencilBuffer();

    HWND hwnd_;
    int32_t width_;
    int32_t height_;
    int32_t bufferCount_;
    int32_t currentBufferIndex_ = 0;

    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissorRect_;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    std::vector<std::unique_ptr<RenderTargetResource>> backBuffers_;
    std::unique_ptr<DepthStencilResource> depthStencilBuffer_;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;

    bool enableVSync_ = true;
};

} // namespace KashipanEngine